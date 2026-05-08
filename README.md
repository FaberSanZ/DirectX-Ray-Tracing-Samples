# DirectX Ray Tracing Samples

Repositorio de ejemplos progresivos de DirectX Raytracing (DXR), construidos con CMake.

La idea es que cada ejemplo agregue una sola pieza nueva del pipeline, sin saltos grandes: primero escribir a una textura, luego acceleration structures, luego buffers de vertices, indices, constant buffers e ImGui.

## Build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

Ejecutar un ejemplo:

```powershell
.\build\Debug\ClearScreen.exe
```

## Requirements

- Windows 10/11
- Visual Studio 2022 con workload de C++
- GPU compatible con DirectX 12 / DXR
- CMake 3.24 o superior

## Examples

Example | Details
---------|--------
<img src="Screenshots/ClearScreen.png" width=380> | [ClearScreen](Src/ClearScreen)<br>Primer ejemplo DXR. Crea un raytracing pipeline minimo con solo `rayGen`, escribe un gradiente a una UAV y copia esa textura al swap chain. No hay geometria, BLAS, TLAS, miss shader ni closest-hit shader.
<img src="Screenshots/AccelerationStructures.png" width=380> | [AccelerationStructures](Src/AccelerationStructures)<br>Introduce acceleration structures. Construye una BLAS para un triangulo y una TLAS que instancia esa BLAS. El ray generation shader dispara rayos primarios, el miss shader pinta el fondo y el closest-hit shader pinta el triangulo.
<img src="Screenshots/VertexBuffer.png" width=380> | [VertexBuffer](Src/VertexBuffer)<br>Agrega lectura de vertex buffer desde shaders DXR. El closest-hit shader accede a un `StructuredBuffer<Vertex>` como SRV y usa coordenadas baricentricas para interpolar colores por vertice.
<img src="Screenshots/IndexBuffer.png" width=380> | [IndexBuffer](Src/IndexBuffer)<br>Agrega index buffer. La BLAS usa vertices compartidos e indices para formar dos triangulos, y el closest-hit shader lee `StructuredBuffer<uint>` para resolver los vertices de cada primitiva.
<img src="Screenshots/ConstantBuffer.png" width=380> | [ConstantBuffer](Src/ConstantBuffer)<br>Agrega constant buffers al root signature global. El ejemplo renderiza un cubo DXR indexado, usa matrices `world/view/projection/inverseViewProjection` para una camara real y rota el cubo actualizando la instancia TLAS por frame.
<img src="Screenshots/ImGui.png" width=380> | [ImGui](Src/ImGui)<br>Integra Dear ImGui encima del resultado DXR. Mantiene la camara con matrices reales, el cubo rotando por TLAS y permite ajustar parametros desde UI antes de dibujar ImGui sobre el back buffer.

## Repository Layout

- `Assets/Shaders`: librerias HLSL DXR por ejemplo.
- `External`: dependencias externas usadas desde `Common`.
- `Src/Common`: utilidades compartidas de ventana, descriptores, GUI y compilacion DXC.
- `Src/<Example>`: un `.cpp` del ejemplo y un `README.md` tutorial.

## Notes

- No se versionan `.sln`, `.vcxproj` ni archivos generados por Visual Studio.
- CMake genera la solucion en `build/`.
- CMake tambien agrega la referencia NuGet a `Microsoft.Direct3D.DXC`.
