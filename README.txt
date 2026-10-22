NFS13Unlimiter

NFS13Unlimiter is a plugin for the PC version of Need for Speed: Most Wanted (2012) that raises
fixed limits the game was built with. It is meant for mods that add content: more jack spots,
more effects, more ui elements, or cars with bigger light and siren setups.

WHAT IT RAISES
* Jack spots: 185 -> 240
* Draw lists: the world map and other ui lists 512 -> 2048 items, every other list doubled
  (fixes the crash when hovering jack spot icons on the map with more spots than retail)
* Car locator buffer: 3 KB -> 8 KB per car (fixes a crash with cars that have many lights)
* VFX pool: 540 -> 1080 resources (GLOBALEFFECTS.BNDL can hold more effects)
* UI streaming pool: 2560 -> 5120 resources (fixes the world map hanging with extra ui elements)
* Jack spots holding a car the game does not know read as empty instead of crashing the game

INSTALLATION
Install Ultimate ASI Loader (32-bit): https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases
Then extract this archive into the game directory. It replaces JackspotLimit.asi; delete that
file if you have it. The plugin writes NFS13Unlimiter.log next to itself.

Each change is checked against the game's original code first. On a different game version that
change is skipped and the log says so.

NFS13Unlimiter uses libmem (AGPL-3.0); its licence and those of the libraries it bundles are in
the licenses folder.
