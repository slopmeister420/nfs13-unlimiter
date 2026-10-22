# nfs13-unlimiter

`NFS13Unlimiter.asi` raises fixed limits in Need for Speed Most Wanted (2012, PC), for mods that add content. Put it
next to `NFS13.exe` (needs an ASI loader). It writes `NFS13Unlimiter.log` next to itself.

- **Jack spots:** 185 -> 240.
- **Draw lists:** the ui lists go from 512 to 2048 items and every other list doubles. Fixes the crash when hovering
  jack spot icons on the world map with more spots than retail.
- **Car locator buffer:** 3 KB -> 8 KB per car. Fixes a crash with cars that have many lights or sirens.
- **VFX pool:** 540 -> 1080 resources, so GLOBALEFFECTS.BNDL can hold more effects.
- **UI streaming pool:** 2560 -> 5120 resources. Fixes the world map hanging with extra ui elements.
- **Jack spots with unknown cars:** a jack spot holding a car the vehicle list does not know reads as empty instead of
  crashing the game.

Each change is checked against the game's original code first. On a different game version it is skipped and the
log says so.

## Building

    .\fetch_deps.ps1
    cmake -S . -B build -A Win32
    cmake --build build --config Release

`.\dist.ps1 -Version <v>` builds the release archive.

## License

AGPL-3.0, as it links [libmem](https://github.com/rdbo/libmem).
