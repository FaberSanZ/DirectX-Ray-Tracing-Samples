# IndexBuffer

Cuarto paso de la serie DXR: agregar index buffer.

En `VertexBuffer`, cada triangulo asumiria vertices consecutivos. Aqui se separan vertices e indices, que es el patron normal para mallas reales.

## Que se aprende

- Crear un index buffer de `uint32_t`.
- Construir la BLAS con `IndexBuffer`, `IndexCount` e `IndexFormat`.
- Crear una SRV para leer indices desde el closest-hit shader.
- Usar `PrimitiveIndex()` para buscar tres indices por triangulo.
- Reutilizar vertices compartidos.

## Flujo DXR

1. Se crean cuatro vertices para formar un rectangulo.
2. Se crean seis indices para dos triangulos.
3. La BLAS recibe el vertex buffer y el index buffer.
4. El root signature expone:
   - `t0`: TLAS.
   - `t1`: vertices.
   - `t2`: indices.
   - `u0`: textura de salida.
5. El closest-hit shader lee tres indices.
6. Luego usa esos indices para traer los vertices reales.

## Shader clave

```hlsl
uint triIndex = PrimitiveIndex() * 3;

Vertex v0 = gVertices[gIndices[triIndex + 0]];
Vertex v1 = gVertices[gIndices[triIndex + 1]];
Vertex v2 = gVertices[gIndices[triIndex + 2]];
```

## Compilar

```powershell
cmake --build build --config Debug --target IndexBuffer
```

## Ejecutar

```powershell
.\build\Debug\IndexBuffer.exe
```
