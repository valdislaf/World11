# World11

Standalone underwater World 11 from Horizon Gates: procedural seabed, water refraction,
corals, fish, bubbles and reef landmarks. C++17, OpenGL 3.3 Core and GLFW.

The application starts in World 11. Its return portal leads to the small World 6 hub,
which has one portal back to the ocean. The shared runtime is included; no access to
any private repository is required. The executable retains the name `horizongates_cpp`
for compatibility with the existing review tools.

## Build on Ubuntu / Debian

```sh
sudo apt-get install cmake ninja-build g++ libglfw3-dev libgl1-mesa-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cd build
./horizongates_cpp
```

Run from the executable directory so that `datasets/` can be found.

## Build on Windows (Visual Studio 2022)

Install GLFW with vcpkg (`vcpkg install glfw3:x64-windows`), then:

```powershell
cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
Set-Location build/Release
./horizongates_cpp.exe
```

`VCPKG_ROOT` must point to your vcpkg checkout. CMake copies the GLFW DLL and
runtime datasets next to the executable. OpenGL 3.3 graphics drivers are required.

## Checks

Five deterministic CPU tests cover decor, coral geometry, fish trajectories, the tube
sponge colony and the
reef fish mesh (including its texture outline).
The optional `world11_scene_probe` target captures six views and tests wave clearance,
seabed collision, the arch opening and the portal round trip:

```sh
cmake --build build --target world11_scene_probe --parallel
mkdir -p build/probe
(cd build && ./world11_scene_probe probe)
```

On a headless Linux machine use `xvfb-run -a` with Mesa. Software-renderer timings
are not representative of a physical GPU.

## Assets

All seven required binary resources are included in `datasets/`. See
[datasets/README.md](datasets/README.md). `scripts/assets.py unpack` restores their
editable PNG payloads into `assets/`; `scripts/assets.py pack` writes edited PNGs
back to the runtime format. There are no Git LFS or private download dependencies.

## Contributions

Read [AGENTS.md](AGENTS.md) and [CONTRIBUTING.md](CONTRIBUTING.md).
Use an Issue, a fork and a pull request. A PR must include its test results and,
for rendering changes, reproducible screenshots. The owner controls merging.
Automated checks do not themselves approve or merge a PR.

See [docs/PROJECT_MAP.md](docs/PROJECT_MAP.md) for the code map and
[docs/WORLD11.md](docs/WORLD11.md) for World 11 internals.

## License

The original MIT license and copyright notice are preserved in [LICENSE](LICENSE).
The vendored stb headers contain their own license notices.
