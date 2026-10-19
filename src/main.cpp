// NFS13Unlimiter.asi: raises fixed limits in NFS Most Wanted (2012).
// - jack spots: 185 -> kCapacity (jackspot_sites.inl, tools/gen_limit.py). The car swap logic keeps every spot in a
//   fixed array; the fields after it move out so the array can grow, and the object is allocated that much bigger.
// - draw lists: lists reserve a fixed number of draw items per frame and one more crashes the renderer (the world map
//   with more jack spots than retail). The ui lists go to 2048 items, the others double (drawlist_sites.inl,
//   tools/gen_drawlists.py).
// - car locator arena: each race car clones its locator table into a 0xc00-byte buffer; ported cars with more
//   locator groups overflowed it. Now 0x2000.
// - resource pools: the VFX pool (GLOBALEFFECTS took one resource more than retail) and the ui streaming pool (extra
//   ui renderables hung the world map load) get twice the resources and more memory; the heap the locator arenas come
//   from grows to fit them. Every bank above a grown pool or heap grows by the same amount.
// Each group is checked against the original bytes first: if the exe differs, that group writes nothing.
#include <windows.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <share.h>
#include <vector>

namespace {

struct Site {
    uint32_t addr;      // instruction
    uint8_t len;        // instruction length
    uint8_t field;      // offset of the 4-byte displacement / immediate inside it
    uint8_t orig[16];   // original instruction bytes
};
struct Raise {
    Site site;
    uint32_t value;     // new immediate
};
#include "jackspot_sites.inl"
#include "drawlist_sites.inl"

constexpr uint32_t kArenaBytes = 0x2000;
// RaceCarEntityModule setup: per car push 0x10; push 0xc00 (allocate) ... push 0xc00 (LinearMalloc size)
const Raise kArenaSites[] = {
    {{0x58e368, 7, 3, {0x6a, 0x10, 0x68, 0x00, 0x0c, 0x00, 0x00}}, kArenaBytes},
    {{0x58e374, 9, 5, {0x8b, 0x4c, 0x24, 0x20, 0x68, 0x00, 0x0c, 0x00, 0x00}}, kArenaBytes},
};

// per-frame key pool all draw lists take their blocks from: counter pointer, capacity in keys
constexpr uintptr_t kPoolCounter = 0x10cf380, kPoolKeys = 0x10cf384;

FILE* g_log = nullptr;

void Log(const char* fmt, ...) {
    if (!g_log) return;
    va_list a;
    va_start(a, fmt);
    vfprintf(g_log, fmt, a);
    va_end(a);
    fputc('\n', g_log);
    fflush(g_log);
}

// --- memory map (.data, read once by the game's startup, so it is changed from DllMain) ---------------------------
namespace memmap {

constexpr uintptr_t kHeader = 0x10c1508;
// header: +4 bank count, +8 pool count, +0x18 heap count, +0x28 banks, +0x2c pools, +0x3c heaps
// bank 0x48 bytes, pool 0x68, heap 0x30: name[0x20], id +0x20, parent bank +0x24
// bank: main bytes +0x28, graphics bytes +0x2c. pool: main +0x38, graphics +0x3c, resources u16 +0x5c, heap nodes
// u16[4] +0x5e. heap: bytes +0x28
struct PoolRaise {
    const char* name;
    uint32_t mem0, mem1;
    uint16_t n, h[4];
    uint32_t newMem0, newMem1;
    uint16_t newN, newH[4];
};
const PoolRaise kPools[] = {
    {"VFX Pool", 0xd7000, 0x1800000, 540, {540, 220, 0, 0}, 0x1ae000, 0x2000000, 1080, {1080, 440, 0, 0}},
    {"UI_Streaming_FLAPT", 0x30c800, 0x2000000, 2560, {2560, 1000, 0, 0},
     0x619000, 0x3000000, 5120, {5120, 4000, 0, 0}},
};
struct HeapRaise {
    const char* name;
    uint32_t bytes, newBytes;
};
// the 16 locator arenas grow by 16 * (kArenaBytes - 0xc00) = 0x14000
const HeapRaise kHeaps[] = {{"Cars Behaviour Allocator", 0x80000, 0xa0000}};

template <class T> T& At(uintptr_t a) { return *reinterpret_cast<T*>(a); }
uint32_t Align(uint32_t v, uint32_t a) { return (v + a - 1) & ~(a - 1); }

// the pool's management data, carved from its bank with its main memory (same sum as the game's pool setup)
uint32_t Overhead(uint32_t n, const uint16_t h[4]) {
    uint64_t p = 3ull * n - 1;
    for (int s = 1; s <= 16; s <<= 1) p |= p >> s;
    uint32_t a = Align(72 * n, 16);
    a = Align(a + n, 16);
    a = Align(a + 2 * n, 16);
    a = Align(a + 2 * n, 16);
    a = Align(a + 4 * n, 16);
    a += static_cast<uint32_t>(16 * p + 16);
    for (int i = 0; i < 4; ++i) a = Align(a + 20 * h[i], 16);
    return a;
}

struct Grow { int32_t bank; uint32_t d0, d1; };

uintptr_t Bank(uintptr_t banks, uint32_t n, int32_t id) {
    for (uint32_t i = 0; i < n; ++i)
        if (At<int32_t>(banks + i * 0x48 + 0x20) == id) return banks + i * 0x48;
    return 0;
}

uintptr_t Find(uintptr_t table, uint32_t count, uint32_t stride, const char* name) {
    for (uint32_t i = 0; i < count; ++i)
        if (!strcmp(reinterpret_cast<const char*>(table + i * stride), name)) return table + i * stride;
    return 0;
}

bool Apply() {
    __try {
        const uint32_t nBanks = At<uint32_t>(kHeader + 4), nPools = At<uint32_t>(kHeader + 8),
                       nHeaps = At<uint32_t>(kHeader + 0x18);
        // the table pointers are offsets from the header until the startup turns them into addresses
        auto table = [](uint32_t v) -> uintptr_t { return v < 0x10000 ? kHeader + v : v; };
        const uintptr_t banks = table(At<uint32_t>(kHeader + 0x28)), pools = table(At<uint32_t>(kHeader + 0x2c)),
                        heaps = table(At<uint32_t>(kHeader + 0x3c));
        if (nBanks != 0x13 || nPools != 0x19 || nHeaps != 0x10 || banks != 0x10c1554 || pools != 0x10c1aac ||
            heaps != 0x10c266c || strcmp(reinterpret_cast<const char*>(banks), "Root Bank")) {
            Log("memory map: unexpected header - nothing written");
            return false;
        }
        Grow grow[8];
        int nGrow = 0;
        // check everything before writing anything
        for (const PoolRaise& r : kPools) {
            uintptr_t p = Find(pools, nPools, 0x68, r.name);
            if (!p || At<uint32_t>(p + 0x38) != r.mem0 || At<uint32_t>(p + 0x3c) != r.mem1 ||
                At<uint16_t>(p + 0x5c) != r.n || memcmp(reinterpret_cast<void*>(p + 0x5e), r.h, 8)) {
                Log("memory map: pool %s differs - nothing written", r.name);
                return false;
            }
            grow[nGrow++] = {At<int32_t>(p + 0x24), (r.newMem0 + Overhead(r.newN, r.newH)) - (r.mem0 + Overhead(r.n, r.h)),
                            r.newMem1 - r.mem1};
        }
        for (const HeapRaise& r : kHeaps) {
            uintptr_t p = Find(heaps, nHeaps, 0x30, r.name);
            if (!p || At<uint32_t>(p + 0x28) != r.bytes) {
                Log("memory map: heap %s differs - nothing written", r.name);
                return false;
            }
            grow[nGrow++] = {At<int32_t>(p + 0x24), r.newBytes - r.bytes, 0};
        }
        // bank chains must resolve to the root
        for (int k = 0; k < nGrow; ++k) {
            const Grow& g = grow[k];
            int depth = 0;
            for (int32_t id = g.bank; id != -1; id = At<int32_t>(Bank(banks, nBanks, id) + 0x24))
                if (!Bank(banks, nBanks, id) || ++depth > 8) {
                    Log("memory map: bank chain broken at %d - nothing written", id);
                    return false;
                }
        }

        DWORD old;
        VirtualProtect(reinterpret_cast<void*>(0x10c1000), 0x2000, PAGE_READWRITE, &old);
        for (const PoolRaise& r : kPools) {
            uintptr_t p = Find(pools, nPools, 0x68, r.name);
            At<uint32_t>(p + 0x38) = r.newMem0;
            At<uint32_t>(p + 0x3c) = r.newMem1;
            At<uint16_t>(p + 0x5c) = r.newN;
            memcpy(reinterpret_cast<void*>(p + 0x5e), r.newH, 8);
            Log("memory map: pool %s: %u -> %u resources, %u -> %u graphics nodes, main %u -> %u KB, graphics %u -> %u KB",
                r.name, r.n, r.newN, r.h[1], r.newH[1], r.mem0 >> 10, r.newMem0 >> 10, r.mem1 >> 10, r.newMem1 >> 10);
        }
        for (const HeapRaise& r : kHeaps) {
            At<uint32_t>(Find(heaps, nHeaps, 0x30, r.name) + 0x28) = r.newBytes;
            Log("memory map: heap %s: %u -> %u KB", r.name, r.bytes >> 10, r.newBytes >> 10);
        }
        for (int k = 0; k < nGrow; ++k)
            for (int32_t id = grow[k].bank; id != -1; id = At<int32_t>(Bank(banks, nBanks, id) + 0x24)) {
                uintptr_t b = Bank(banks, nBanks, id);
                const Grow& g = grow[k];
                At<uint32_t>(b + 0x28) += Align(g.d0, 0x1000);
                At<uint32_t>(b + 0x2c) += Align(g.d1, 0x1000);
            }
        for (uint32_t i = 0; i < nBanks; ++i) {
            uintptr_t b = banks + i * 0x48;
            Log("memory map: bank %-20s main %6u KB, graphics %6u KB", reinterpret_cast<const char*>(b),
                At<uint32_t>(b + 0x28) >> 10, At<uint32_t>(b + 0x2c) >> 10);
        }
        VirtualProtect(reinterpret_cast<void*>(0x10c1000), 0x2000, old, &old);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("memory map: fault while reading it - nothing written");
        return false;
    }
}

}  // namespace memmap

// --- code patches (wait for the EA wrapper to decrypt .text) -------------------------------------------------------
struct Patch {
    const Site* site;
    uint32_t value;
};

struct Group {
    const char* name;
    std::vector<Patch> patches;
    bool done = false;
};

uint32_t Field(const Site& s) {
    uint32_t v;
    memcpy(&v, s.orig + s.field, 4);
    return v;
}

// 0: original bytes, 1: already patched, -1: neither (not decrypted yet, or another exe)
int State(const Patch& p) {
    __try {
        auto at = reinterpret_cast<const uint8_t*>(p.site->addr);
        if (!memcmp(at, p.site->orig, p.site->len)) return 0;
        uint8_t want[16];
        memcpy(want, p.site->orig, p.site->len);
        memcpy(want + p.site->field, &p.value, 4);
        return memcmp(at, want, p.site->len) ? -1 : 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

bool Write(const Patch& p) {
    auto at = reinterpret_cast<uint8_t*>(p.site->addr + p.site->field);
    DWORD old;
    if (!VirtualProtect(at, 4, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy(at, &p.value, 4);
    VirtualProtect(at, 4, old, &old);
    return true;
}

// writes the group once every site reads original; true when finished either way
bool TryGroup(Group& g, bool lastChance) {
    int orig = 0, patched = 0, other = 0;
    for (const Patch& p : g.patches) {
        int st = State(p);
        orig += st == 0;
        patched += st == 1;
        other += st < 0;
    }
    const int n = static_cast<int>(g.patches.size());
    if (patched == n) {
        Log("%s: already patched", g.name);
        return true;
    }
    if (orig != n) {
        if (lastChance) {
            Log("%s: gave up after 120 s: %d of %d sites original, %d patched, %d unknown - not the NFS13.exe this "
                "was made for; nothing written", g.name, orig, n, patched, other);
            for (const Patch& p : g.patches)
                if (State(p) < 0) Log("  differs: %08x", p.site->addr);
        }
        return lastChance;
    }
    int ok = 0;
    for (const Patch& p : g.patches) ok += Write(p);
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    int verified = 0;
    for (const Patch& p : g.patches) verified += State(p) == 1;
    Log("%s: %d/%d sites written, %d verified", g.name, ok, n, verified);
    return true;
}

// logs the highest key pool use seen, so draw list growth can be checked against the pool
void WatchPool() {
    uint32_t peak = 0, logged = 0;
    for (;;) {
        __try {
            uint32_t cap = *reinterpret_cast<const uint32_t*>(kPoolKeys);
            auto counter = *reinterpret_cast<const volatile uint32_t* const*>(kPoolCounter);
            if (cap && counter) {
                uint32_t used = *counter;
                if (used <= cap && used > peak) peak = used;
                if (peak >= logged + cap / 50) {
                    Log("key pool peak %u of %u keys (%.1f%%)", peak, cap, 100.0 * peak / cap);
                    logged = peak;
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        Sleep(1);
    }
}

DWORD WINAPI Install(LPVOID) {
    Group groups[3] = {{"jack spots"}, {"draw lists"}, {"car locator arena"}};
    for (const Site& s : kSites) groups[0].patches.push_back({&s, Field(s) + kDelta});
    for (const Raise& r : kDrawListSites) groups[1].patches.push_back({&r.site, r.value});
    for (const Raise& r : kArenaSites) groups[2].patches.push_back({&r.site, r.value});
    const DWORD t0 = GetTickCount();
    for (;;) {
        const bool last = GetTickCount() - t0 > 120000;
        bool all = true;
        for (Group& g : groups) {
            if (!g.done) g.done = TryGroup(g, last);
            all &= g.done;
        }
        if (all) break;
        Sleep(1);
    }
    Log("jack spot limit %d, car locator arena %u bytes, %lu ms after load", kCapacity, kArenaBytes,
        GetTickCount() - t0);
    WatchPool();
    return 0;
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        char path[MAX_PATH];
        GetModuleFileNameA(inst, path, MAX_PATH);
        char* dot = strrchr(path, '.');
        if (dot) strcpy_s(dot, path + MAX_PATH - dot, ".log");
        g_log = _fsopen(path, "w", _SH_DENYWR);
        // the memory map is read during the game's startup, after plugins load
        memmap::Apply();
        CreateThread(nullptr, 0, Install, nullptr, 0, nullptr);
    }
    return TRUE;
}
