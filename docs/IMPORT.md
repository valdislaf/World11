# Standalone import

Source snapshot: `codex-gl33-world11-new33`, commit
`800aa3e35efbb8b04e255639139c7b458020b898`.

The public repository has a separate Git history. It contains the complete C++
runtime needed by World 11, the original five packed PNG resources, three CPU
tests, the scene probe, the MIT license and contributor documentation.

Standalone changes:
- Start in World 11 by default.
- Register the ocean (11) and return hub (6); the hub has only the ocean portal.
- Install the datasets with the executable; remove an obsolete top.png install reference.
- Retain the shared GL33 helper implementation for source compatibility.
- Add portable build instructions, asset unpack/repack support and PR checks.

The renderer and procedural generation algorithms were preserved during extraction.
The historical upstream review is in WORLD11_REVIEW.md; its earlier measurements
must not be interpreted as measurements from this import.

Resource Git blob SHA-1 values were verified against the source before upload,
after the PNG unpack/repack round trip and after creating the public Git blobs.

## Verification in the extraction environment

- GCC 13.3 Release: complete application and scene-probe targets built successfully.
- CTest: 3/3 passed (decor, coral geometry, fish trajectories).
- Asset unpack/repack preserved all five original Git blob hashes.
- Local graphics execution was blocked because the environment could not create
  an X server listening socket. The scene probe is also configured in GitHub Actions.
- Windows build instructions are provided; a Windows build was not run here.
