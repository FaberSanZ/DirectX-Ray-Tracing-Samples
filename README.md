# DirectX Ray Tracing Samples

Progressive DirectX Raytracing (DXR) samples built with CMake.

Each sample adds one focused concept to the previous one: first writing to a texture, then acceleration structures, vertex buffers, index buffers, constant buffers, camera matrices, and finally Dear ImGui on top of a DXR frame.

## Build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

Run a sample:

```powershell
.\build\Debug\ClearScreen.exe
```

## Requirements

- Windows 10/11
- Visual Studio 2022 with the C++ desktop workload
- DirectX 12 / DXR capable GPU
- CMake 3.24 or newer

## Examples

Example | Details
---------|--------
<img src="Screenshots/ClearScreen.png" width=380> | [ClearScreen](Src/ClearScreen)<br>The first DXR sample. It creates a minimal raytracing pipeline with only `rayGen`, writes a gradient into a UAV texture, and copies that texture to the swap chain. There is no geometry, BLAS, TLAS, miss shader, or closest-hit shader yet.
<img src="Screenshots/AccelerationStructures.png" width=380> | [AccelerationStructures](Src/AccelerationStructures)<br>Introduces acceleration structures. It builds a BLAS for one triangle and a TLAS that instances it. The ray generation shader fires primary rays, the miss shader writes the background, and the closest-hit shader colors the triangle.
<img src="Screenshots/VertexBuffer.png" width=380> | [VertexBuffer](Src/VertexBuffer)<br>Adds vertex-buffer reads from DXR shaders. The closest-hit shader accesses a `StructuredBuffer<Vertex>` SRV and uses barycentric coordinates to interpolate per-vertex colors.
<img src="Screenshots/IndexBuffer.png" width=380> | [IndexBuffer](Src/IndexBuffer)<br>Adds indexed geometry. The BLAS uses shared vertices and indices to form two triangles, and the closest-hit shader reads a `StructuredBuffer<uint>` to resolve each primitive's vertices.
<img src="Screenshots/ConstantBuffer.png" width=380> | [ConstantBuffer](Src/ConstantBuffer)<br>Adds constant buffers to the global root signature. The sample renders an indexed DXR cube, uses `world/view/projection/inverseViewProjection` matrices for a real camera, and rotates the cube by updating the TLAS instance transform every frame.
<img src="Screenshots/ImGui.png" width=380> | [ImGui](Src/ImGui)<br>Integrates Dear ImGui on top of the DXR result. It keeps the real matrix-based camera, the cube rotating through TLAS updates, and lets the UI adjust parameters before drawing ImGui over the back buffer.

## Repository Layout

- `Assets/Shaders`: DXR HLSL libraries per sample.
- `External`: third-party dependencies used by `Common`.
- `Src/Common`: shared helpers for windows, descriptors, GUI integration, and DXC compilation.
- `Src/<Example>`: one `.cpp` sample file and one tutorial `README.md`.

## Notes

- `.sln`, `.vcxproj`, and Visual Studio generated files are not committed.
- CMake generates the solution under `build/`.
- CMake also adds the NuGet reference to `Microsoft.Direct3D.DXC`.
