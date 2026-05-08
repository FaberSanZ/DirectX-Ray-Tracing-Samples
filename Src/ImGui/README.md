# ImGui

The sixth step in the DXR series: draw Dear ImGui over a DXR scene with a real camera.

The scene is still raytraced. ImGui does not replace the DXR render path: it is drawn after `DispatchRays` and after copying the raytraced texture into the back buffer.

This sample keeps the `world`, `view`, `projection`, and `inverseViewProjection` matrices from `ConstantBuffer`. The cube rotates by updating the TLAS instance transform every frame.

## What You Learn

- Integrate Dear ImGui with the Win32 + DirectX 12 backends.
- Use a separate shader-visible descriptor heap for ImGui.
- Draw UI on top of a DXR result.
- Synchronize command allocator reuse with a fence.
- Use correct barriers between UAV, copy, render target, and present states.
- Control camera and tint parameters from ImGui.
- Keep the cube rotating while the UI is drawn on top.

## Frame Flow

1. The app updates `SceneConstants`.
2. The app updates the instance matrix and rebuilds the TLAS.
3. The ray generation shader writes into `mpOutputResource` as a UAV.
4. A UAV barrier is inserted.
5. `mpOutputResource` transitions from `UNORDERED_ACCESS` to `COPY_SOURCE`.
6. The back buffer transitions from `PRESENT` to `COPY_DEST`.
7. The DXR result is copied into the back buffer.
8. The back buffer transitions from `COPY_DEST` to `RENDER_TARGET`.
9. ImGui generates and draws its command data.
10. The back buffer transitions from `RENDER_TARGET` to `PRESENT`.
11. The frame is presented.
12. The app waits on the fence before resetting the allocator/list.

## Why Barriers Matter

Without these transitions, the UI can flicker or become unstable because the back buffer is used as both a copy destination and a render target in the same frame. Direct3D 12 does not perform those transitions automatically.

## Build

```powershell
cmake --build build --config Debug --target ImGui
```

## Run

```powershell
.\build\Debug\ImGui.exe
```
