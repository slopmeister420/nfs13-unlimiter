# nfs13-unlimiter

`NFS13Unlimiter.asi` raises fixed limits in Need for Speed Most Wanted (2012, PC). Put it next to `NFS13.exe`
(needs an ASI loader). It writes `NFS13Unlimiter.log` next to itself.

- **Jack spots: 185 -> 240.** The car swap logic keeps every spot in a fixed array. The fields after it move out so
  the array can grow, and the object is allocated that much bigger. `tools/gen_limit.py [capacity]` regenerates
  `src/jackspot_sites.inl` (225 instructions) from `NFS13_dump.exe`. Replaces `JackspotLimit.asi` from `jackspots`.
- **UI draw lists: 512 -> 2048 items.** Six UI lists (the world map's among them) hold at most 512 draw items per
  frame. One more and the renderer reads an empty entry and crashes, e.g. hovering jack spot icons on the map with
  more spots than retail. They take their space from the 8 MB per-frame key pool (524288 keys); the raise uses
  9216 more. The log records the highest pool use it sees.

Each group is checked against the original instructions first. If the exe differs, that group writes nothing and the
log lists the sites that differ.

    cmake -S . -B build -A Win32
    cmake --build build --config Release
