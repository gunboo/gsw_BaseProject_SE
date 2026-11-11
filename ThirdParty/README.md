# Bundled dependencies

All build inputs are checked in. CMake does not fetch packages or require Git submodules.

| Component | Version | Use | License |
|---|---|---|---|
| GLFW | 3.4 | Window, OpenGL context, input, event loop | `glfw/LICENSE.md` (zlib/libpng) |
| GLEW | 2.3.1 | OpenGL entry-point loader, built from `glew/src/glew.c` | `glew/LICENSE.txt` |
| stb_truetype | 1.26, pinned commit in manifest | Portable text coverage rasterization | `stb/LICENSE` (MIT / public domain) |
| Nanum Gothic / Nanum Myeongjo | Regular and Bold | Bundled Korean UI and serif fallback | `../SimpleGame/Data/Fonts/*-OFL.txt` (SIL OFL) |

`manifest.json` records original release URLs or immutable commit URLs and download SHA-256 digests. Generated GLEW sources are included; Perl or a GLEW generator is not required. GLFW builds the selected native backend from source. The CMake options disable upstream examples, tests and online documentation builds.

Sources: [GLFW source releases](https://github.com/glfw/glfw/releases/tag/3.4), [GLEW 2.3.1](https://github.com/nigels-com/glew/releases/tag/glew-2.3.1), [stb](https://github.com/nothings/stb), [Google Fonts Nanum Gothic](https://github.com/google/fonts/tree/main/ofl/nanumgothic), [Nanum Myeongjo](https://github.com/google/fonts/tree/main/ofl/nanummyeongjo).

The old `SimpleGame/Dependencies` files and old output DLLs are legacy assets. They are not used by the new CMake build and are not packaged. Do not copy them into the GLFW release.
