# shadertoy
---

[![build-windows](https://github.com/dtcxzyw/shadertoy/actions/workflows/build-windows.yml/badge.svg)](https://github.com/dtcxzyw/shadertoy/actions/workflows/build-windows.yml)
[![build-linux](https://github.com/dtcxzyw/shadertoy/actions/workflows/build-linux.yml/badge.svg)](https://github.com/dtcxzyw/shadertoy/actions/workflows/build-linux.yml)
[![build-macos](https://github.com/dtcxzyw/shadertoy/actions/workflows/build-macos.yml/badge.svg)](https://github.com/dtcxzyw/shadertoy/actions/workflows/build-macos.yml)

Unofficial ShaderToy live viewer

## Gallery

[![goo](https://user-images.githubusercontent.com/15650457/236786522-80c10c46-f3b0-46f3-88ef-abbe39c3cd5f.png)](https://www.shadertoy.com/view/lllBDM)


[![expansive reaction-diffusion](https://user-images.githubusercontent.com/15650457/236787527-b26fa835-1d36-4dc6-be59-6d508e898e04.png)](https://www.shadertoy.com/view/4dcGW2)


[![mandelbrot](https://user-images.githubusercontent.com/15650457/236788040-2411c757-7c51-407a-869f-5c6709bf5e5d.png)](https://www.shadertoy.com/view/lsX3W4)

[![MultiscaleMIPFluid](https://user-images.githubusercontent.com/15650457/236790106-5ebeb8a2-0c16-4cbd-a7cf-d8bbb21ad613.png)](https://www.shadertoy.com/view/tsKXR3)

[![RainbowSand](https://user-images.githubusercontent.com/15650457/236790355-c20303e1-7abd-4d42-9088-2133a0e756fa.png)](https://www.shadertoy.com/view/stdyRr)

[![subsurface](https://user-images.githubusercontent.com/15650457/236790664-3defcade-c5b4-4f9c-9f21-0a1b67b72536.png)](https://www.shadertoy.com/view/dltGWl)

[![noise-contour](https://user-images.githubusercontent.com/15650457/236791146-b3b9cdff-6754-42ae-83c3-d69ef2ea9387.png)](https://www.shadertoy.com/view/MscSzf)

[![cubemaps](https://github.com/dtcxzyw/shadertoy/assets/15650457/e1719ed0-2748-47e0-a7eb-db6d35cd8dec)](https://www.shadertoy.com/view/MsXGz4)

See [examples](examples) for more shaders.


## Features

Render passes:

+ [x] Image
+ [x] Cubemap
+ [ ] Sound
+ [x] Buffer
+ [x] Common 

Channels:
+ [x] Textures
+ [ ] Music
+ [ ] Video
+ [ ] Volumes
+ [x] Cubemaps
+ [x] Buffer
+ [x] Keyboard
+ [ ] Webcam
+ [ ] Microphone

Utilities:

+ [x] Import from shadertoy.com
+ [x] Render pass editor
+ [x] GLSL shader editor
+ [x] Export/import shaders in STTF(ShaderToy Transmission Format)
+ [x] Screenshots
+ [ ] Video recording
+ [ ] Custom uniforms
+ [ ] Custom meshes
+ [ ] Anti aliasing

Contributions are welcome!

## Releases
See [Releases Page](https://github.com/dtcxzyw/shadertoy/releases) for pre-built binaries (windows x64/linux x86-64/macos arm64/macos x86-64).

## Build Instructions

### Prerequisites
+ Windows/Linux/macOS
+ CMake 3.12+
+ [vcpkg](https://github.com/microsoft/vcpkg)
+ Graphics API: OpenGL 4.5+

### Clone the repository
```bash
git clone --recursive https://github.com/dtcxzyw/shadertoy.git
cd shadertoy
```

### Configure and build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=<path-to-prefix> -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build -j
cmake --build build -t install
```

### Run shadertoy live viewer
```bash
<path-to-prefix>/shadertoy[.exe] [<path-to-sttf/shadertoy-url>]
```

## License
This repository is licensed under the Apache License 2.0. See [LICENSE](LICENSE) for details.
