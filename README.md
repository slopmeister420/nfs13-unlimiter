# nfs13-unlimiter

`NFS13Unlimiter.asi` raises fixed limits in Need for Speed Most Wanted (2012, PC). Put it next to `NFS13.exe`
(needs an ASI loader). It writes `NFS13Unlimiter.log` next to itself.

- **Jack spots: 185 -> 240.** The car swap logic keeps every spot in a fixed array. The fields after it move out so
  the array can grow, and the object is allocated that much bigger. `tools/gen_limit.py [capacity]` regenerates
  `src/jackspot_sites.inl` (225 instructions) from `NFS13_dump.exe`. Replaces `JackspotLimit.asi` from `jackspots`.
- **Draw lists.** 36 lists reserve a fixed number of draw items per frame; one more and the renderer reads an empty
  entry and crashes (e.g. hovering jack spot icons on the world map with more spots than retail). The six 512-item ui
  lists go to 2048, every other list doubles. `tools/gen_drawlists.py` regenerates `src/drawlist_sites.inl`. They all
  take their space from the 8 MB per-frame key pool (524288 keys); together they now reserve about 45800. The log
  records the highest pool use it sees.
- **Car locator arena: 0xc00 -> 0x2000 bytes.** Each race car clones its locator table (locator groups: lights,
  sirens, driver points) into a fixed buffer; a car whose table did not fit crashed the game (NFS13+0x574281).
- **Resource pools.** The game's memory map (banks, pools and heaps, read once at startup) is changed from DllMain:
  - VFX Pool: 540 -> 1080 resources, graphics nodes 220 -> 440, memory 860 KB / 24 MB -> 1.7 MB / 32 MB.
    GLOBALEFFECTS.BNDL could take only one resource beyond retail before.
  - UI_Streaming_FLAPT: 2560 -> 5120 resources, graphics nodes 1000 -> 4000, memory 3.1 MB / 32 MB -> 6.1 MB / 48 MB.
    About a dozen extra ui renderables in a loaded feedback group used to hang the world map load.
  - Cars Behaviour Allocator (where the locator arenas live): 512 -> 640 KB.
  - Each bank above a grown pool or heap grows by the same amount, up to the root (about 4.5 MB main and 24 MB
    graphics memory more in total). A pool's bank share is its memory plus management data sized by its counts, the
    same sum the game's pool setup uses.

Each group is checked against the original instructions first. If the exe differs, that group writes nothing and the
log lists the sites that differ.

    .etch_deps.ps1
    cmake -S . -B build -A Win32
    cmake --build build --config Release

Memory access goes through [libmem](https://github.com/rdbo/libmem) (AGPL-3.0, linked statically), so this project
is AGPL-3.0 as well; see `LICENSE`. `.\dist.ps1 -Version <v>` builds the release archive.
