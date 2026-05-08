# IndexBuffer

The fourth step in the DXR series: add an index buffer.

In `VertexBuffer`, each triangle assumes consecutive vertices. This sample separates vertices and indices, which is the common pattern for real meshes.

## What You Learn

- Create a `uint32_t` index buffer.
- Build the BLAS with `IndexBuffer`, `IndexCount`, and `IndexFormat`.
- Create an SRV so the closest-hit shader can read indices.
- Use `PrimitiveIndex()` to fetch three indices per triangle.
- Reuse shared vertices.

## DXR Flow

1. Create four vertices for a rectangle.
2. Create six indices for two triangles.
3. The BLAS receives both the vertex buffer and index buffer.
4. The root signature exposes:
   - `t0`: TLAS.
   - `t1`: vertices.
   - `t2`: indices.
   - `u0`: output texture.
5. The closest-hit shader reads three indices.
6. It then uses those indices to fetch the real vertices.

## Key Shader

```hlsl
uint triIndex = PrimitiveIndex() * 3;

Vertex v0 = gVertices[gIndices[triIndex + 0]];
Vertex v1 = gVertices[gIndices[triIndex + 1]];
Vertex v2 = gVertices[gIndices[triIndex + 2]];
```

## Build

```powershell
cmake --build build --config Debug --target IndexBuffer
```

## Run

```powershell
.\build\Debug\IndexBuffer.exe
```
