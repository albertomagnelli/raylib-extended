# camera_controller

A small header-only C++ class (`CameraController`) that drives a raylib `Camera3D` with
smooth movement and smooth rotation, as a replacement for `UpdateCamera()`, which moves the
camera at a fixed speed with no acceleration and no rotation smoothing.

```
extensions/
├── camera_controller.hpp           # The CameraController class, header-only
├── camera_controller_example.cpp   # Runnable example with all four modes
├── particle_example.cpp            # 2d particle demo, ported from the RayLib-Demo project
├── shadows_example.cpp             # Directional light with shadow mapping, ported from RayLib-Demo
├── CMakeLists.txt                  # Builds the examples, standalone or with raylib
└── README.md
```

## Why it feels smoother

Everything the controller does is *requested* and then *followed*, using exponential smoothing
that is computed from the frame time, so the result is the same at 30, 60 or 240 fps:

| What | How it is smoothed | Tuning knob |
| :--- | :--- | :--- |
| Movement | The camera has a velocity that accelerates towards the requested one and decays when the keys are released | `moveSpeed`, `moveResponse` |
| Rotation | Mouse/keyboard/gamepad input sets a requested yaw and pitch, the camera rotates towards them | `lookSensitivity`, `lookResponse` |
| Zoom | Mouse wheel sets a requested distance or field-of-view, the camera eases into it | `distanceResponse`, `fovyResponse` |
| Roll | Same as rotation, free flight mode only | `rollSpeed`, `rollResponse` |

`response` values are "how fast the value reaches the target" (1/seconds):
lower = smoother and heavier, higher = snappier, `0.0f` = instant.

## Quick start

```cpp
#include "raylib.h"
#include "camera_controller.hpp"

int main()
{
    InitWindow(1280, 720, "camera_controller");
    SetTargetFPS(60);

    Camera3D initialCamera = { { 12.0f, 8.0f, 12.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 45.0f, CAMERA_PERSPECTIVE };

    CameraController camera(initialCamera, CAMERA_CONTROLLER_THIRD_PERSON);
    camera.SetMoveSpeed(8.0f);
    camera.SetDistance(10.0f);
    camera.SetCursorLocked(true);

    while (!WindowShouldClose())
    {
        camera.Update();                    // once per frame, before BeginMode3D()

        BeginDrawing();
            ClearBackground(RAYWHITE);
            BeginMode3D(camera.GetCamera());
                DrawGrid(20, 1.0f);
            EndMode3D();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```

## Modes

| Mode | Pivot (what the controller moves) | Movement | Zoom drives |
| :--- | :--- | :--- | :--- |
| `CAMERA_CONTROLLER_FIRST_PERSON` | The camera position | World plane by default | Field-of-view |
| `CAMERA_CONTROLLER_THIRD_PERSON` | The look-at point (the player) | World plane by default | Distance |
| `CAMERA_CONTROLLER_ORBIT` | The look-at point | Pans the pivot | Distance |
| `CAMERA_CONTROLLER_FREE` | The camera position | Full 3d, roll enabled with `Q`/`E` | Field-of-view |

`SetMode()` keeps the current view direction and reattaches the pivot, so switching modes
while playing does not teleport the view.

## Default controls

| Input | Action |
| :--- | :--- |
| `W` `A` `S` `D` | Move |
| `LEFT_SHIFT` / `LEFT_ALT` | Sprint / move slowly |
| `SPACE` / `LEFT_CONTROL` | Move up / down |
| `MOUSE` | Look around |
| `ARROW KEYS` | Look around without the mouse |
| `MOUSE WHEEL` | Zoom |
| `Q` / `E` | Roll (free flight mode) |
| `R` | Reset to the camera the controller was created with |

All keys are reassignable:

```cpp
CameraControllerKeys keys = camera.GetKeys();
keys.forward = KEY_UP;
keys.sprint = KEY_RIGHT_SHIFT;
camera.SetKeys(keys);
```

Gamepad input (left stick moves, right stick looks, triggers fly up/down, left trigger sprints)
is opt-in: `camera.SetGamepadEnabled(true);`

## Main API

| Call | Description |
| :--- | :--- |
| `Update(frameTime)` | Read raylib input and advance the camera, `frameTime` optional |
| `UpdateManual(frameTime, velocity, rotation, zoom)` | Advance the camera with your own input, raylib input is never read |
| `GetCamera()` | The `Camera3D` to pass to `BeginMode3D()` |
| `GetPosition()` / `SetPosition(v)` | Camera eye position |
| `GetTarget()` / `SetTarget(v)` / `LookAt(v)` | Point the camera looks at |
| `MoveBy(delta)` | Translate the camera rig in world units |
| `RotateBy(yaw, pitch, roll)` | Add degrees to the requested rotation |
| `SetYawPitch(yaw, pitch, roll, snap)` | Set the requested orientation in degrees |
| `GetYaw()` / `GetPitch()` / `GetRoll()` | Current orientation in degrees |
| `GetForward()` / `GetRight()` / `GetUp()` | Normalized camera basis vectors |
| `GetVelocity()` / `GetSpeed()` / `StopMovement()` / `AddVelocity(impulse)` | Movement state, `AddVelocity()` is for hits and camera kicks |
| `GetDistance()` / `SetDistance(d)` | Pivot distance (third person, orbit) |
| `GetFovy()` / `SetFovy(fovy)` | Field-of-view aperture in Y (degrees) |
| `GetMode()` / `SetMode(mode)` | Movement mode |
| `Reset()` / `SetCamera(camera)` | Restore or replace the whole camera state |
| `GetConfig()` / `SetConfig(config)` | All tuning values at once |
| `SetMoveSpeed()`, `SetMoveResponse()`, `SetLookSensitivity()`, `SetLookResponse()`, `SetMaxPitch()`, `SetAutoRotate()`, `SetCursorLocked()`, `SetInvertY()`, `SetMoveInWorldPlane()`, `SetGamepadEnabled()`, `SetEnabled()`, `SetKeys()` | Individual tuning setters |

Setters that change a smoothed value take an optional `snap` argument:
`SetDistance(4.0f)` eases into the new distance, `SetDistance(4.0f, true)` applies it immediately.

## Driving the camera from game code

`UpdateManual()` is the version to use when the camera should follow something else,
or when the input comes from a replay, an AI or the network:

```cpp
// Move at 6 units per second along the local forward axis, rotate 1.5 degrees of yaw
glm::vec3 desiredVelocity{ 0.0f, 0.0f, 6.0f };
glm::vec2 lookDelta{ 1.5f, 0.0f };

camera.UpdateManual(GetFrameTime(), desiredVelocity, lookDelta, 0.0f);
```

The smoothing is still applied, so the camera keeps accelerating and turning smoothly.

## Tuning examples

```cpp
// Heavy, cinematic camera
camera.SetMoveSpeed(6.0f);
camera.SetMoveResponse(4.0f);      // slow to start, slow to stop
camera.SetLookResponse(8.0f);      // look lags behind the mouse

// Tight, arcade camera
camera.SetMoveResponse(40.0f);
camera.SetLookResponse(60.0f);

// Instant, unsmoothed behaviour (raylib UpdateCamera() like)
camera.SetMoveResponse(0.0f);
camera.SetLookResponse(0.0f);

// Auto-rotating showcase orbit
camera.SetMode(CAMERA_CONTROLLER_ORBIT);
camera.SetAutoRotate(20.0f);       // degrees per second
camera.SetMoveInWorldPlane(false); // WASD pans the pivot in full 3d
```

## Building the examples

With the raylib project (the root `CMakeLists.txt` adds `extensions/` when `BUILD_EXTENSIONS` is on, which is the default for top-level builds):

```
cmake -B build
cmake --build build
./build/camera_controller_example
./build/particle_example
./build/shadows_example
```

Standalone, from the `extensions/` directory (it configures and builds raylib from the parent folder):

```
cd extensions
cmake -B build
cmake --build build
./build/camera_controller_example
./build/particle_example
./build/shadows_example
```

In CLion, either open the project root and configure with `-DBUILD_EXTENSIONS=ON`, or open
`extensions/` as the project folder.

When `extensions/` is configured standalone it does not inherit `CMAKE_CXX_STANDARD` from the
parent project, so `CMakeLists.txt` falls back to C++20 (`particle_example.cpp` uses
`std::numbers` and `std::erase_if`). `shadows_example.cpp` uses glm, which `CMakeLists.txt`
fetches with `FetchContent` (`g-truc/glm` 1.0.1) on the first configure, so that build needs
network access once. The MinGW runtime DLLs are linked dynamically, so run the
examples from a shell where the toolchain `bin` directory is on `PATH`, or copy the example next
to `libstdc++-6.dll`, `libgcc_s_seh-1.dll` and `libwinpthread-1.dll`.

## particle_example (ported mx Partikel-Demo)

`particle_example.cpp` is the particle demo of the `RayLib-Demo` project
(`examples/app/main.cpp`, built on the `mx` engine) ported to plain raylib, it does not use
`CameraController`:

| mx engine | raylib |
| :--- | :--- |
| `mx::Window::create({ .multisample = 4, .resizable, .vsync })` | `SetWindowSamples(4)` + `SetConfigFlags(FLAG_WINDOW_RESIZABLE \| FLAG_VSYNC_HINT)` |
| `window.setMinSize()` / `setFullscreen()` | `SetWindowMinSize()` / `ToggleFullscreen()` |
| `mx::Input` | `IsMouseButtonDown()` / `IsKeyPressed()` / `GetMousePosition()` |
| `mx::Renderer2D` `drawLine` / `drawCircle` / `drawRect` | `DrawLineEx()` / `DrawCircleV()` / `DrawRectangleRec()` |
| `drawCircleLines` / `drawPolyLines` | `DrawCircleLinesEx()` / `DrawPolyLinesEx()` |
| `mx::Font::loadFromFile()` / `mx::measureText()` | `LoadFontEx()` on a system TTF, `GetFontDefault()` as fallback / `MeasureTextEx()` |
| `mx::Clock` (`maxDelta`) + `mx::FrameLimiter{120}` | `GetFrameTime()` clamped to `0.05f` + `SetTargetFPS(120)` |

The particle behaviour, the palette, the background gradient, the orbit shapes and the HUD layout
are unchanged, including the German HUD strings. `mx::Renderer2D` rotates polygons in radians,
raylib rotates them in degrees, so the orbit rotation is converted with `RAD2DEG`.

| Input | Action |
| :--- | :--- |
| `MOUSE` + `LEFT BUTTON` | Attract the particles to the cursor and emit a stream at it |
| `SPACE` | Particle explosion at the cursor |
| `R` | Clear all particles |
| `F11` | Toggle fullscreen |
| `ESC` | Close the window |
| `F12` | Save a screenshot (raylib built-in) |

## shadows_example (ported mx Schattenwurf-Demo)

`shadows_example.cpp` is the Phase 9 demo of the `RayLib-Demo` project
(`examples/phase9_shadows/main.cpp`) ported to plain raylib: five lit primitives on a plane,
an animated directional light and a 2048x2048 depth-only shadow map with 3x3 PCF, drawn through
`rlgl` with custom shader programs instead of the raylib batch:

| mx engine | raylib |
| :--- | :--- |
| `mx::gpu::Device` / `Framebuffer` | `rlLoadFramebuffer()` + `rlLoadTextureDepth()` + `rlFramebufferAttach()` |
| `mx::gpu::Program` | `LoadShaderFromMemory()` |
| `mx::gpu::MeshBuffer` | `GenMesh*()` + own VAO/VBO upload through `rlLoadVertexArray()` / `rlLoadVertexBuffer()` |
| `mx::gpu::Pipeline` | `rlEnableShader()` / `rlEnableBackfaceCulling()` / `rlEnableDepthTest()` |
| `mx::gpu::Sampler` (nearest, clamp) | the defaults `rlLoadTextureDepth()` already sets |
| `mx::make*Mesh()` | `GenMeshPlane()` / `GenMeshCube()` / `GenMeshSphere()` / ... |
| glm math | glm is raylib's public math API (`glm::vec3`, `Matrix`, ...); the standalone build fetches `g-truc/glm` 1.0.1 with `FetchContent` |

The lighting, the 3x3 PCF and the Reinhard tonemapping are unchanged, as are the palette and the
German HUD strings. The original shaders asked for `#version 460` because mx created an OpenGL 4.6
context, the port uses `#version 330` for the OpenGL 3.3 core context raylib defaults to.

| Input | Action |
| :--- | :--- |
| `MOUSE` + `LEFT BUTTON` | Orbit the camera around the scene |
| `MOUSE WHEEL` | Zoom in and out |
| `L` | Pause / resume the animated light |
| `R` | Reset camera and light |
| `F11` | Toggle fullscreen |
| `ESC` | Close the window |
| `F12` | Save a screenshot (raylib built-in) |

### Things to know when drawing through rlgl yourself

These were the three traps while porting, all of them easy to reproduce:

- **`Matrix` is `glm::mat4`**: raylib exposes glm types directly, so a `glm::mat4` can be passed
  to `rlSetUniformMatrix()` / `SetShaderValueMatrix()` without any conversion. Both glm and
  OpenGL store matrices column-major and `rlMatrixToFloat()` preserves that layout, so the matrix
  reaches the shader unchanged.
- **Depth state is not enabled for custom passes**: raylib initializes OpenGL with
  `glDisable(GL_DEPTH_TEST)` and only `BeginMode3D()` enables it. A pass that draws straight
  through `rlgl` has to call `rlEnableDepthTest()` itself, otherwise nothing lands in the depth
  texture of the shadow map and every fragment samples depth 1.0. The example enables the depth
  test in the shadow pass and in the lit pass, and restores `rlDisableDepthTest()` before the 2d
  overlay.
- **Not every `GenMesh*()` result is indexed**: `GenMeshSphere()`, `GenMeshCylinder()`,
  `GenMeshCone()` and `GenMeshTorus()` return `indices == NULL`, only plane and cube provide an
  index buffer. Upload both cases and draw with `rlDrawVertexArrayElements()` or
  `rlDrawVertexArray()` depending on `mesh.indices`.

After the custom passes the example restores the state the raylib batch expects for the 2d
overlay: texture slot 0, backface culling off, depth test off, default shader reactivated.

## MSAA 8x / 16x in the examples

This fork also extends the raylib core with a samples-count API, both examples use it:

```cpp
SetWindowSamples(16);         // request 16x MSAA, before InitWindow()
InitWindow(1280, 720, "...");
GetWindowSamples();           // the samples really in use, zero means MSAA off
```

| Call | Description |
| :--- | :--- |
| `SetWindowSamples(samples)` | Request MSAA for the window, `0` disables it, must be called **before** `InitWindow()` |
| `GetWindowSamples()` | Samples in use, before the window exists it is the requested count, after it is what the graphics device provided |
| `FLAG_MSAA_8X_HINT` / `FLAG_MSAA_16X_HINT` | The flag form of the request, alongside the existing `FLAG_MSAA_4X_HINT`, the highest one set wins |

Press `M` in the example to cycle `4x -> 8x -> 16x -> off`, the HUD prints
`msaa: 16x requested, 8x provided`. The window is closed and recreated on every change because
the sample count is fixed when the OpenGL context is created, and the requested count is rounded
to the nearest supported one (`5` asks for `4x`, `12` asks for `8x`); if the graphics device
provides less, raylib logs `DISPLAY: Graphics device only provided MSAA x%i`.

Supported on the GLFW, SDL, RGFW and native Win32 desktop backends (native Win32 degrades
16x -> 8x -> 4x -> 2x until the driver accepts a pixel format), on DRM and Android through the
EGL config, on the web backends antialiasing is enabled but WebGL decides the sample count.
MSAA only applies to the default framebuffer, `RenderTexture` targets are not multisampled.

## Notes

- The controller assumes a **Y-up** world: yaw rotates around `(0, 1, 0)`, pitch is clamped to `maxPitch` to avoid camera somersaults.
- `lockCursor` hides the cursor and recenters it every frame, which is what keeps the mouse look working when the camera spins continuously.
- Long sessions are safe: the accumulated yaw is periodically normalized, so precision does not degrade after hundreds of thousands of degrees of rotation.
- Angles are always in **degrees**, distances in **world units**, speeds in **units per second**.

## License

zlib/libpng, same as raylib.
