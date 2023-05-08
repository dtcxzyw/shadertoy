# shadertoy
---

[![build-windows](https://github.com/dtcxzyw/shadertoy/actions/workflows/build-windows.yml/badge.svg)](https://github.com/dtcxzyw/shadertoy/actions/workflows/build-windows.yml)
[![build-linux](https://github.com/dtcxzyw/shadertoy/actions/workflows/build-linux.yml/badge.svg)](https://github.com/dtcxzyw/shadertoy/actions/workflows/build-linux.yml)

Unofficial ShaderToy live viewer

## Gallery

## Features

Render passes:

+ [x] Image
+ [ ] Cubemap
+ [ ] Sound
+ [x] Buffer
+ [x] Common 

Channels:
+ [x] Textures
+ [ ] Music
+ [ ] Video
+ [ ] Volumes
+ [ ] Cubemaps
+ [x] Buffer
+ [x] Keyboard
+ [ ] Webcam
+ [ ] Microphone

Utilities:

+ [x] Import from shadertoy.com
+ [x] Render pass editor
+ [x] GLSL shader editor
+ [x] Export/import shaders in STTF(ShaderToy Transmission Format)
+ [ ] Screenshots
+ [ ] Video recording
+ [ ] Custom uniforms
+ [ ] Custom meshes

Contributions are welcome!

## Releases
See [Releases Page](https://github.com/dtcxzyw/shadertoy/releases) for pre-built binaries (windows/linux x86-64).

## Build Instructions

### Prerequisites
+ Windows/Linux x86-64
+ CMake 3.12+
+ vcpkg
+ Graphics API: OpenGL 4.5+

### Clone the repository
```bash
git clone --recursive https://github.com/dtcxzyw/shadertoy.git
cd shadertoy
```

### Configure and build
```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=<path-to-prefix> -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build -j
cmake --build build -t install
```

### Run shadertoy live viewer
```bash
<path-to-prefix>/shadertoy[.exe] [<path-to-sttf/shadertoy-url>]
```

## License
This repository is licensed under the Apache License 2.0. See [LICENSE](LICENSE) for details.
