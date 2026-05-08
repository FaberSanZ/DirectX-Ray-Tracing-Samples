# ImGui

Sexto paso de la serie DXR: superponer Dear ImGui sobre una escena DXR con camara real.

La escena sigue siendo raytraced. ImGui no reemplaza el render DXR: se dibuja despues de `DispatchRays` y despues de copiar la textura raytraced al back buffer.

Este ejemplo conserva las matrices `world`, `view`, `projection` e `inverseViewProjection` del ejemplo `ConstantBuffer`. El cubo gira actualizando la transformacion de instancia en la TLAS cada frame.

## Que se aprende

- Integrar Dear ImGui usando el backend Win32 + DirectX 12.
- Usar un descriptor heap shader-visible separado para ImGui.
- Dibujar UI encima del resultado DXR.
- Sincronizar el uso del command allocator con fence.
- Usar barreras correctas para pasar entre UAV, copy, render target y present.
- Controlar parametros de camara y tinte desde ImGui.
- Mantener el cubo rotando mientras la UI se dibuja encima.

## Flujo de frame

1. La app actualiza `SceneConstants`.
2. La app actualiza la matriz de instancia y reconstruye la TLAS.
3. El ray generation shader escribe en `mpOutputResource` como UAV.
4. Se inserta una UAV barrier.
5. `mpOutputResource` transiciona de `UNORDERED_ACCESS` a `COPY_SOURCE`.
6. El back buffer transiciona de `PRESENT` a `COPY_DEST`.
7. Se copia el resultado DXR al back buffer.
8. El back buffer transiciona de `COPY_DEST` a `RENDER_TARGET`.
9. ImGui genera y dibuja sus comandos.
10. El back buffer transiciona de `RENDER_TARGET` a `PRESENT`.
11. Se presenta el frame.
12. Se espera la fence antes de resetear allocator/list.

## Por que importan las barreras

Sin estas transiciones, la UI puede parpadear o verse inestable porque el back buffer se usa como destino de copia y render target en el mismo frame. Direct3D 12 no hace esas transiciones por nosotros.

## Compilar

```powershell
cmake --build build --config Debug --target ImGui
```

## Ejecutar

```powershell
.\build\Debug\ImGui.exe
```
