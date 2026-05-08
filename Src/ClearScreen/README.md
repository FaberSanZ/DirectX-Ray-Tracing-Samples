# ClearScreen

The first step in the DXR series: write directly to a texture from a ray generation shader.

This sample has no geometry, BLAS, TLAS, miss shader, or closest-hit shader. The goal is to isolate the minimum required pieces for creating a DXR pipeline and calling `DispatchRays`.

## What You Learn

- Create a Direct3D 12 device with DXR support.
- Create an `ID3D12StateObject` raytracing pipeline.
- Compile a `lib_6_3` HLSL library with DXC.
- Create a global root signature with one UAV.
- Create a shader table with a single `rayGen` record.
- Write pixels into a `RWTexture2D<float4>`.
- Copy the output texture to the swap-chain back buffer.

## DXR Flow

1. Create `mpOutputResource` with `D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS`.
2. Create a UAV in a shader-visible descriptor heap.
3. Expose that UAV as `u0` in the root signature.
4. The `rayGen` shader uses `DispatchRaysIndex()` and `DispatchRaysDimensions()` to compute one color per pixel.
5. The application calls `DispatchRays`.
6. The result is copied into the swap-chain back buffer.

## Main Shader

The shader lives in `Assets/Shaders/ClearScreen/Mesh.hlsl`.

```hlsl
RWTexture2D<float4> gOutput : register(u0);

[shader("raygeneration")]
void rayGen()
{
    uint2 id = DispatchRaysIndex().xy;
    float2 uv = id / float2(DispatchRaysDimensions().xy);
    gOutput[id] = float4(uv, 0.5, 1.0);
}
```

## Build

```powershell
cmake --build build --config Debug --target ClearScreen
```

## Run

```powershell
.\build\Debug\ClearScreen.exe
```
