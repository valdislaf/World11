# Project map

- Build and tests: `CMakeLists.txt`, `tests/`.
- Entry point: `cpp/main.cpp` (World 11 by default).
- World registration: `cpp/world/WorldCatalog.cpp` (ocean 11 and return hub 6).
- Shared runtime: `cpp/hg_*`, `cpp/world/`.
- Rendering: `cpp/render/gl33/`.
- World details: [WORLD11.md](WORLD11.md).
- Runtime assets: `datasets/`; editable PNGs: `assets/`.

The shared GL33 implementation retains legacy helper code for other worlds; they are not registered in this standalone application.
