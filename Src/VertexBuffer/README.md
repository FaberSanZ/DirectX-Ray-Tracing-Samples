# VertexBuffer

The third step in the DXR series: read vertex data from the closest-hit shader.

The previous sample only used geometry for intersection. Here, the vertex buffer is also exposed as a `StructuredBuffer<Vertex>` so the shader can interpolate color with barycentric coordinates.

## What You Learn

- Create a vertex buffer with position and color.
- Create a structured-buffer SRV.
- Add a `t1` descriptor for vertex data.
- Use `PrimitiveIndex()` inside the closest-hit shader.
- Interpolate attributes with `BuiltInTriangleIntersectionAttributes`.

## DXR Flow

1. Create a buffer with three vertices.
2. The BLAS uses positions from that same buffer to describe the triangle.
3. Create a structured SRV so HLSL can read the buffer.
4. The root signature exposes:
   - `t0`: TLAS.
   - `t1`: `StructuredBuffer<Vertex>`.
   - `u0`: output texture.
5. The closest-hit shader computes the triangle vertex indices.
6. The shader interpolates colors with `u`, `v`, and `w`.

## Key Shader

```hlsl
uint triIndex = PrimitiveIndex();

Vertex v0 = gVertices[triIndex * 3 + 0];
Vertex v1 = gVertices[triIndex * 3 + 1];
Vertex v2 = gVertices[triIndex * 3 + 2];

float u = attribs.barycentrics.x;
float v = attribs.barycentrics.y;
float w = 1.0 - u - v;

payload.color = v0.color * w + v1.color * u + v2.color * v;
```

## Build

```powershell
cmake --build build --config Debug --target VertexBuffer
```

## Run

```powershell
.\build\Debug\VertexBuffer.exe
```
