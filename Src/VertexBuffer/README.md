# VertexBuffer

Tercer paso de la serie DXR: leer datos de vertices desde el closest-hit shader.

El ejemplo anterior solo usaba la geometria para intersectar. Aqui ademas exponemos el vertex buffer como `StructuredBuffer<Vertex>` para que el shader pueda interpolar color usando coordenadas baricentricas.

## Que se aprende

- Crear un vertex buffer con posicion y color.
- Crear una SRV de buffer estructurado.
- Agregar un descriptor `t1` para vertices.
- Usar `PrimitiveIndex()` dentro del closest-hit shader.
- Interpolar atributos con `BuiltInTriangleIntersectionAttributes`.

## Flujo DXR

1. Se crea un buffer con tres vertices.
2. La BLAS usa las posiciones del mismo buffer para describir el triangulo.
3. Se crea una SRV estructurada para acceder al buffer desde HLSL.
4. El root signature expone:
   - `t0`: TLAS.
   - `t1`: `StructuredBuffer<Vertex>`.
   - `u0`: textura de salida.
5. El closest-hit shader calcula los indices del triangulo.
6. Se interpolan colores con `u`, `v`, `w`.

## Shader clave

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

## Compilar

```powershell
cmake --build build --config Debug --target VertexBuffer
```

## Ejecutar

```powershell
.\build\Debug\VertexBuffer.exe
```
