# ConstantBuffer

Quinto paso de la serie DXR: agregar constant buffers, camara real y matrices.

Este ejemplo transforma la malla indexada en un cubo y agrega un CBV al root signature global. El shader usa ese constant buffer para recibir matrices `world`, `view`, `projection` e `inverseViewProjection`.

El cubo rota de verdad en DXR: cada frame se actualiza la matriz de instancia de la TLAS y se reconstruye la TLAS antes de llamar `DispatchRays`.

## Que se aprende

- Crear un cubo con vertex buffer e index buffer.
- Construir una BLAS con 8 vertices y 36 indices.
- Crear un upload buffer alineado a 256 bytes para constantes.
- Bindear un CBV directamente en la root signature.
- Leer `ConstantBuffer<T>` desde HLSL en una libreria DXR.
- Generar rayos primarios desde una camara con matrices.
- Rotar una instancia DXR reconstruyendo la TLAS.

## Flujo DXR

1. Se crea un cubo indexado.
2. La BLAS se construye con los vertices e indices del cubo.
3. Se crea `SceneConstants` con matrices reales de camara.
4. El root signature expone:
   - `t0`: TLAS.
   - `t1`: vertices.
   - `t2`: indices.
   - `u0`: textura de salida.
   - `b0`: constant buffer.
5. El ray generation shader usa `inverseViewProjection` para reconstruir rayos de camara.
6. El closest-hit shader interpola los colores del cubo.
7. Cada frame la app cambia la matriz `world`, actualiza la instancia TLAS y el cubo gira.

## Constant buffer

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

En HLSL:

```hlsl
ConstantBuffer<SceneConstants> gScene : register(b0);
```

El ray generation shader crea rayos con:

```hlsl
float4 nearPoint = mul(float4(ndc, 0.0f, 1.0f), gScene.inverseViewProjection);
float4 farPoint = mul(float4(ndc, 1.0f, 1.0f), gScene.inverseViewProjection);
ray.Origin = gScene.cameraPosition.xyz;
ray.Direction = normalize(farPoint.xyz - nearPoint.xyz);
```

## Compilar

```powershell
cmake --build build --config Debug --target ConstantBuffer
```

## Ejecutar

```powershell
.\build\Debug\ConstantBuffer.exe
```
