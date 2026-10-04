# minecraft-cpp

Basic voxel setup

## Requirements

- C++20 compiler (g++ / clang++)
- CMake >= 3.20
- Vulkan SDK (headers + loader, e.g. `vulkan-headers`, `vulkan-loader`, `vulkan-validationlayers`)
- GLFW >= 3.3

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Run

```sh
./build/minecraft
```

Press `ESC` or close the window to quit. Expected output:

- `Hello, Minecraft in C++!`
- GLFW version
- Vulkan instance version
- Required instance extensions
- Physical devices (GPUs) found
- Window `800x600` titled "Minecraft C++ - Hello World"

## Layout

- `CMakeLists.txt` — finds Vulkan + GLFW, builds `minecraft`
- `src/main.cpp` — GLFW init, Vulkan instance, GPU listing, event loop
