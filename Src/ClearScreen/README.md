# ClearScreen

Primer paso de la serie DXR: escribir directamente a una textura desde un ray generation shader.

En este ejemplo no hay geometria, BLAS, TLAS, miss shader ni closest-hit shader. La idea es aislar el minimo necesario para crear un pipeline DXR y lanzar `DispatchRays`.

## Que se aprende

- Crear un dispositivo Direct3D 12 con soporte DXR.
- Crear una `ID3D12StateObject` de raytracing.
- Compilar una libreria HLSL `lib_6_3` con DXC.
- Crear una root signature global con una UAV.
- Crear un shader table con un unico registro: `rayGen`.
- Escribir pixeles en una `RWTexture2D<float4>`.
- Copiar la textura de salida al back buffer.

## Flujo DXR

1. Se crea una textura `mpOutputResource` con `D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS`.
2. Se crea una UAV en un descriptor heap shader-visible.
3. El root signature expone esa UAV como `u0`.
4. El shader `rayGen` usa `DispatchRaysIndex()` y `DispatchRaysDimensions()` para calcular un color por pixel.
5. La aplicacion ejecuta `DispatchRays`.
6. El resultado se copia al back buffer del swap chain.

## Shader principal

El shader esta en `Assets/Shaders/ClearScreen/Mesh.hlsl`.

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

## Compilar

```powershell
cmake --build build --config Debug --target ClearScreen
```

## Ejecutar

```powershell
.\build\Debug\ClearScreen.exe
```
