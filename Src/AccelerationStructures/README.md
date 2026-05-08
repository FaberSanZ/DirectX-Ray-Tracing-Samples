# AccelerationStructures

Segundo paso de la serie DXR: agregar geometria real usando acceleration structures.

Este ejemplo introduce una BLAS para un triangulo y una TLAS que instancia esa BLAS. A partir de este punto los rayos pueden intersectar geometria y ejecutar un closest-hit shader.

## Que se aprende

- Crear una `D3D12_RAYTRACING_GEOMETRY_DESC`.
- Construir una bottom-level acceleration structure (BLAS).
- Construir una top-level acceleration structure (TLAS).
- Crear un SRV de tipo `D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE`.
- Agregar `miss` y `closesthit` al pipeline DXR.
- Llenar el shader table con `rayGen`, `miss` y `HitGroup`.

## Flujo DXR

1. Se crea un vertex buffer con tres posiciones.
2. La BLAS describe ese triangulo.
3. La TLAS contiene una instancia de la BLAS.
4. El root signature expone:
   - `t0`: TLAS.
   - `u0`: textura de salida.
5. `rayGen` dispara rayos primarios desde una camara simple.
6. Si el rayo falla, `miss` pinta el fondo.
7. Si el rayo pega, `chs` pinta el triangulo.

## Shaders

El shader esta en `Assets/Shaders/AccelerationStructures/Mesh.hlsl`.

Entradas principales:

```hlsl
RaytracingAccelerationStructure gRtScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);
```

`TraceRay` usa la TLAS para buscar intersecciones:

```hlsl
TraceRay(gRtScene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
```

## Compilar

```powershell
cmake --build build --config Debug --target AccelerationStructures
```

## Ejecutar

```powershell
.\build\Debug\AccelerationStructures.exe
```
