# ConstantBuffer

The fifth step in the DXR series: add constant buffers, a real camera, and matrices.

This sample turns the indexed mesh into a cube and adds a CBV to the global root signature. The shader receives `world`, `view`, `projection`, and `inverseViewProjection` matrices through a constant buffer.

The cube rotates in the DXR scene itself: every frame updates the TLAS instance transform and rebuilds the TLAS before calling `DispatchRays`.

## What You Learn

- Create a cube with a vertex buffer and index buffer.
- Build a BLAS with 8 vertices and 36 indices.
- Create a 256-byte-aligned upload buffer for constants.
- Bind a CBV directly in the global root signature.
- Read `ConstantBuffer<T>` from HLSL in a DXR library.
- Generate primary rays from a matrix-based camera.
- Rotate a DXR instance by rebuilding the TLAS.

## DXR Flow

1. Create an indexed cube.
2. Build the BLAS from the cube vertices and indices.
3. Create `SceneConstants` with real camera matrices.
4. The root signature exposes:
   - `t0`: TLAS.
   - `t1`: vertices.
   - `t2`: indices.
   - `u0`: output texture.
   - `b0`: constant buffer.
5. The ray generation shader uses `inverseViewProjection` to reconstruct camera rays.
6. The closest-hit shader interpolates cube colors.
7. Every frame, the app changes the `world` matrix, updates the TLAS instance, and the cube rotates.

## Constant Buffer

```cpp
struct SceneConstants
{
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 projection;
    DirectX::XMFLOAT4X4 inverseViewProjection;
    float tint[4];
    float cameraPosition[4];
};
```

In HLSL:

```hlsl
ConstantBuffer<SceneConstants> gScene : register(b0);
```

The ray generation shader creates rays with:

```hlsl
float4 nearPoint = mul(float4(ndc, 0.0f, 1.0f), gScene.inverseViewProjection);
float4 farPoint = mul(float4(ndc, 1.0f, 1.0f), gScene.inverseViewProjection);
ray.Origin = gScene.cameraPosition.xyz;
ray.Direction = normalize(farPoint.xyz - nearPoint.xyz);
```

## Build

```powershell
cmake --build build --config Debug --target ConstantBuffer
```

## Run

```powershell
.\build\Debug\ConstantBuffer.exe
```
