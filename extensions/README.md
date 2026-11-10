# camera_controller

A small header-only C++ class (`CameraController`) that drives a raylib `Camera3D` with
smooth movement and smooth rotation, as a replacement for `UpdateCamera()`, which moves the
camera at a fixed speed with no acceleration and no rotation smoothing.

```
extensions/
├── camera_controller.hpp           # The CameraController class, header-only
├── camera_controller_example.cpp   # Runnable example with all four modes
├── CMakeLists.txt                  # Builds the example, standalone or with raylib
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

int main(void)
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
Vector3 desiredVelocity = { 0.0f, 0.0f, 6.0f };
Vector2 lookDelta = { 1.5f, 0.0f };

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

## Building the example

With the raylib project (the root `CMakeLists.txt` adds `extensions/` when `BUILD_EXTENSIONS` is on, which is the default for top-level builds):

```
cmake -B build
cmake --build build
./build/camera_controller_example
```

Standalone, from the `extensions/` directory (it configures and builds raylib from the parent folder):

```
cd extensions
cmake -B build
cmake --build build
./build/camera_controller_example
```

In CLion, either open the project root and configure with `-DBUILD_EXTENSIONS=ON`, or open
`extensions/` as the project folder.

## Notes

- The controller assumes a **Y-up** world: yaw rotates around `(0, 1, 0)`, pitch is clamped to `maxPitch` to avoid camera somersaults.
- `lockCursor` hides the cursor and recenters it every frame, which is what keeps the mouse look working when the camera spins continuously.
- Long sessions are safe: the accumulated yaw is periodically normalized, so precision does not degrade after hundreds of thousands of degrees of rotation.
- Angles are always in **degrees**, distances in **world units**, speeds in **units per second**.

## License

zlib/libpng, same as raylib.
