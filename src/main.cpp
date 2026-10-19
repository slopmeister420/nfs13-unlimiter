// NFS13Unlimiter.asi: raises fixed limits in NFS Most Wanted (2012).
// - jack spots: 185 -> kCapacity (jackspot_sites.inl, tools/gen_limit.py). The car swap logic keeps every spot in a
//   fixed array; the fields after it move out so the array can grow, and the object is allocated that much bigger.
// - ui draw lists: the map and other ui screens draw at most 512 items per list; past that the renderer reads an empty
//   entry and the game crashes. Those lists get kUiKeys.
// Each group is checked against the original instructions first: if the exe differs, that group writes nothing.
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
#include "jackspot_sites.inl"

constexpr uint32_t kUiKeys = 0x800;
// UIEntityModule::GenerateDispatchLists: push 0x200; add ecx, <list>; call PreAllocateContiguousKeyBlock
const Site kUiSites[] = {
    {0x624963, 11, 1, {0x68, 0x00, 0x02, 0x00, 0x00, 0x81, 0xc1, 0x00, 0x0f, 0x00, 0x00}},
    {0x6249a5, 11, 1, {0x68, 0x00, 0x02, 0x00, 0x00, 0x81, 0xc1, 0x00, 0x10, 0x00, 0x00}},  // world map
    {0x6249c6, 11, 1, {0x68, 0x00, 0x02, 0x00, 0x00, 0x81, 0xc1, 0x80, 0x0d, 0x00, 0x00}},
    {0x624a08, 11, 1, {0x68, 0x00, 0x02, 0x00, 0x00, 0x81, 0xc1, 0x80, 0x0e, 0x00, 0x00}},
    {0x624a29, 11, 1, {0x68, 0x00, 0x02, 0x00, 0x00, 0x81, 0xc1, 0x00, 0x11, 0x00, 0x00}},
    {0x624a4a, 11, 1, {0x68, 0x00, 0x02, 0x00, 0x00, 0x81, 0xc1, 0x80, 0x10, 0x00, 0x00}},
};

// per-frame key pool all draw lists take their blocks from: counter pointer, capacity in keys
constexpr uintptr_t kPoolCounter = 0x10cf380, kPoolKeys = 0x10cf384;

struct Patch {
    const Site* site;
    uint32_t value;     // new displacement / immediate
};

struct Group {
    const char* name;
    std::vector<Patch> patches;
    bool done = false;
};

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

// logs the highest key pool use seen, so ui list growth can be checked against the pool
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
    Group groups[2] = {{"jack spots"}, {"ui draw lists"}};
    for (const Site& s : kSites) groups[0].patches.push_back({&s, Field(s) + kDelta});
    for (const Site& s : kUiSites) groups[1].patches.push_back({&s, kUiKeys});
    const DWORD t0 = GetTickCount();
    // the EA wrapper decrypts .text after we load: wait until each group's sites read as the original instructions
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
    Log("jack spot limit %d, ui draw lists %u keys, %lu ms after load", kCapacity, kUiKeys, GetTickCount() - t0);
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
        CreateThread(nullptr, 0, Install, nullptr, 0, nullptr);
    }
    return TRUE;
}
