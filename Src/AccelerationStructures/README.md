# AccelerationStructures

The second step in the DXR series: add real geometry through acceleration structures.

This sample introduces a BLAS for one triangle and a TLAS that instances that BLAS. From this point on, rays can intersect geometry and run a closest-hit shader.

## What You Learn

- Create a `D3D12_RAYTRACING_GEOMETRY_DESC`.
- Build a bottom-level acceleration structure (BLAS).
- Build a top-level acceleration structure (TLAS).
- Create an SRV with `D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE`.
- Add `miss` and `closesthit` shaders to the DXR pipeline.
- Fill the shader table with `rayGen`, `miss`, and `HitGroup` records.

## DXR Flow

1. Create a vertex buffer with three positions.
2. The BLAS describes that triangle.
3. The TLAS contains one instance of the BLAS.
4. The root signature exposes:
   - `t0`: TLAS.
   - `u0`: output texture.
5. `rayGen` fires primary rays from a simple camera.
6. If the ray misses, `miss` writes the background.
7. If the ray hits, `chs` colors the triangle.

## Shaders

The shader lives in `Assets/Shaders/AccelerationStructures/Mesh.hlsl`.

Main inputs:

```hlsl
RaytracingAccelerationStructure gRtScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);
```

`TraceRay` uses the TLAS to find intersections:

```hlsl
TraceRay(gRtScene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
```

## Build

```powershell
cmake --build build --config Debug --target AccelerationStructures
```

## Run

```powershell
.\build\Debug\AccelerationStructures.exe
```
