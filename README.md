# OpenGL Data App

A native C++20/OpenGL 3D data viewer and editor for TIFF voxel stacks and editable Data Points.

## Code Layout

- `src/main.cpp`: application startup, frame loop, and shutdown
- `src/app_state.h` / `src/app_state.cpp`: scene data, selection, picking, and undo/redo history
- `src/file_io.h` / `src/file_io.cpp`: TIFF import, OBJ project open/save, and HDF5 export
- `src/editor.h` / `src/editor.cpp`: ImGui panels and keyboard/mouse interaction
- `src/renderer.h` / `src/renderer.cpp`: OpenGL lighting, grid, image layers, and Data Point drawing

## Controls

- Click a Data Point: select it for transformation
- Drag with the left mouse button: orbit the camera
- Mouse wheel: zoom
- `W` / `S`: move camera forward/backward in the current view direction
- `A` / `D`: strafe left/right relative to the camera
- Floor grid: camera-following world-space XZ plane that continues across the visible horizon; red X and blue Z axes stay at origin
- Layers tab: select TIFF slices or Data Points and toggle TIFF visibility
- Transform tab: TIFF layers and Data Points both use voxel-based XYZ position and XYZ size transforms; TIFF layers have no Value field
- History tab: undo, redo, or restore a previous editing action; the panel shows the Previous Action
- New Data Point: starts at the origin with a size of one voxel (`0.01` scene units per axis)
- Delete Data Point: remove the selected Data Point
- **Open Project**: load a project saved in this app's OBJ format
- **Import TIFF Stack**: multi-select `.tif` or `.tiff` files; they appear flat on the XZ plane at native pixel dimensions (100 pixels per scene unit)
- **Save Project**: save all Data Point positions and sizes to OBJ
- **Export (HDF5)**: export positions, scales, and typed custom values to HDF5
- Arrow keys: move along X/Z; Page Up/Page Down: move along Y
- `Esc`: quit

## Build on Windows

Prerequisites:

- Visual Studio 2022 with the Desktop development with C++ workload
- CMake 3.20 or newer
- Git available on `PATH` for CMake FetchContent

From the project root:

```powershell
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build-msvc --config Release --target package_executable
.\bin\opengl_data_app.exe
```

The first configure downloads GLFW, Dear ImGui, and HDF5 into CMake's build directory. TIFF import uses Windows Imaging Component and reads the first page of each selected file. Stack slices are sorted by filename and must have matching pixel dimensions. TIFF layers use the same voxel-based XYZ position and size transform as Data Points; their source intensity plane is extruded through the Y size to create a textured 3D block and matching HDF5 volume. HDF5 writes TIFF data to `volume` in `[Y voxel, row, column]` order as 16-bit grayscale, plus `voxel_spacing_world_units` and `image_layer_transforms` with voxel-based position and size. Data Point positions, sizes, and custom values are exported in voxel units. New OBJ project files are tagged as voxel coordinates; older untagged app projects remain readable.
