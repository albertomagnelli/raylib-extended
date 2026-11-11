/*******************************************************************************************
*
*   camera_controller - Smooth Camera3D controller for raylib (C++ class)
*
*   CameraController drives a raylib Camera3D with frame-rate independent smoothing:
*       - Smooth movement: the camera accelerates and decelerates (damped velocity), no instant starts/stops
*       - Smooth rotation: mouse/keyboard/gamepad look input is damped, pitch is clamped, yaw never flips
*       - Four modes: FIRST_PERSON, THIRD_PERSON, ORBIT, FREE (free flight, with roll)
*       - Smooth zoom (mouse wheel): distance for third-person/orbit, field-of-view for first-person/free
*       - Extras: cursor lock, orbit auto-rotation, sprint/slow multipliers, configurable keys, gamepad support
*
*   DEPENDENCIES:
*       raylib 6.5+   - Window/keyboard/mouse/gamepad input, Camera3D type
*       raymath       - Vector math (glm vectors, Vector3RotateByAxisAngle(), ...)
*
*   USAGE:
*       #include "camera_controller.hpp"
*
*       Camera3D initialCamera = { { 12.0f, 8.0f, 12.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 45.0f, CAMERA_PERSPECTIVE };
*
*       CameraController camera(initialCamera, CAMERA_CONTROLLER_THIRD_PERSON);
*       camera.SetMoveSpeed(8.0f);
*       camera.SetLookSensitivity(0.08f);
*
*       while (!WindowShouldClose())
*       {
*           camera.Update();                        // Read input and smooth the camera, once per frame
*
*           BeginMode3D(camera.GetCamera());
*               DrawGrid(20, 1.0f);
*           EndMode3D();
*       }
*
*   CONFIGURATION:
*       Every tunable value lives in CameraControllerConfig, reachable with GetConfig()/SetConfig()
*       or with the individual setters: SetMoveSpeed(), SetMoveResponse(), SetLookSensitivity(),
*       SetLookResponse(), SetDistance(), SetFovy(), SetAutoRotate(), SetCursorLocked(), ...
*
*   NOTES:
*       - Header-only, all member functions are inline, just include it from a .cpp file
*       - Update() must be called once per frame, before BeginMode3D()
*       - UpdateManual() replaces Update() when the input comes from somewhere else
*         (player physics, replay system, AI, network input, ...)
*       - Angles are stored in degrees, all smoothing is frame-time based (works at any FPS)
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2026 raylib-extended contributors
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

#ifndef CAMERA_CONTROLLER_HPP
#define CAMERA_CONTROLLER_HPP

#include "raylib.h"         // Required for: Camera3D, GetFrameTime(), GetMouseDelta(), GetMouseWheelMove(), IsKeyDown(), IsKeyPressed(), GetGamepadAxisMovement(), ...
#include "raymath.h"        // Required for: vec2, vec3, Vector3RotateByAxisAngle(), DEG2RAD, EPSILON
                                    // (vector arithmetic is glm: length(), normalize(), cross(), distance(), dot())

#include <math.h>           // Required for: sinf(), cosf(), atan2f(), asinf(), expf(), powf(), fabsf()

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#ifndef CAMERA_CONTROLLER_MAX_PITCH
    #define CAMERA_CONTROLLER_MAX_PITCH      89.0f     // Default pitch limit in degrees (up and down)
#endif

#ifndef CAMERA_CONTROLLER_LOOK_AHEAD
    #define CAMERA_CONTROLLER_LOOK_AHEAD      1.0f     // Distance from the camera position to its target in first-person/free modes
#endif

#ifndef CAMERA_CONTROLLER_MAX_FRAME_TIME
    #define CAMERA_CONTROLLER_MAX_FRAME_TIME  0.1f     // Longest frame step processed, avoids big jumps after a pause or a window switch
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// Camera controller movement modes
enum CameraControllerMode
{
    CAMERA_CONTROLLER_FIRST_PERSON = 0,   // Camera is the player, it moves on the world plane (or in full 3d if configured)
    CAMERA_CONTROLLER_THIRD_PERSON,       // Camera follows a pivot point from behind, distance supported
    CAMERA_CONTROLLER_ORBIT,              // Camera orbits around a pivot point, distance and auto-rotation supported
    CAMERA_CONTROLLER_FREE                // Camera flies freely on all axes, roll supported
};

// Keyboard bindings used by the controller, they can be reassigned with SetKeys()
struct CameraControllerKeys
{
    int forward;                          // Default KEY_W
    int backward;                         // Default KEY_S
    int left;                             // Default KEY_A
    int right;                            // Default KEY_D
    int ascend;                           // Default KEY_SPACE
    int descend;                          // Default KEY_LEFT_CONTROL
    int sprint;                           // Default KEY_LEFT_SHIFT
    int slow;                             // Default KEY_LEFT_ALT
    int lookUp;                           // Default KEY_UP
    int lookDown;                         // Default KEY_DOWN
    int lookLeft;                         // Default KEY_LEFT
    int lookRight;                        // Default KEY_RIGHT
    int rollLeft;                         // Default KEY_Q, only used in CAMERA_CONTROLLER_FREE
    int rollRight;                        // Default KEY_E, only used in CAMERA_CONTROLLER_FREE
    int reset;                            // Default KEY_R, restores the camera it was created with
};

// Tunable movement and rotation parameters
struct CameraControllerConfig
{
    // Movement
    float moveSpeed;                      // Units per second, default 8.0f
    float sprintMultiplier;               // moveSpeed multiplier while the sprint key is held, default 2.5f
    float slowMultiplier;                 // moveSpeed multiplier while the slow key is held, default 0.2f
    float moveResponse;                   // How fast the velocity reaches the requested one (1/seconds), lower = smoother/heavier, default 12.0f

    // Rotation
    float lookSensitivity;                // Degrees of rotation per mouse pixel, default 0.08f
    float lookResponse;                   // How fast the rotation reaches the requested one (1/seconds), 0.0f = instant, default 18.0f
    float keyboardLookSpeed;              // Degrees per second when rotating with the arrow keys, default 60.0f
    float maxPitch;                       // Pitch limit in degrees, up and down, default CAMERA_CONTROLLER_MAX_PITCH
    float gamepadLookSpeed;               // Degrees per second at full gamepad right-stick deflection, default 120.0f

    // Roll (CAMERA_CONTROLLER_FREE only)
    float rollSpeed;                      // Degrees per second while a roll key is held, default 60.0f
    float rollResponse;                   // How fast the roll reaches the requested one (1/seconds), default 10.0f

    // Zoom
    float distance;                       // Pivot distance used by THIRD_PERSON and ORBIT, in units, default 10.0f
    float minDistance;                    // Minimum pivot distance, in units, default 1.0f
    float maxDistance;                    // Maximum pivot distance, in units, default 100.0f
    float distanceResponse;               // How fast the distance reaches the requested one (1/seconds), default 10.0f
    float fovy;                           // Camera field-of-view aperture in Y (degrees), default 45.0f
    float minFovy;                        // Minimum field-of-view (zoom in), default 15.0f
    float maxFovy;                        // Maximum field-of-view (zoom out), default 120.0f
    float fovyResponse;                   // How fast the field-of-view reaches the requested one (1/seconds), default 10.0f

    // Behaviour
    float autoRotate;                     // ORBIT mode auto-rotation in degrees per second, 0.0f = disabled, default 0.0f
    float gamepadDeadZone;                // Gamepad axis dead zone, [0..1] range, default 0.05f
    bool moveInWorldPlane;                // Movement is projected to the world plane (no flying), default true
    bool invertY;                         // Invert the vertical mouse look, default false
    bool lockCursor;                      // Hide and recenter the mouse cursor while updating, default false
    bool gamepadEnabled;                  // Read movement and rotation from gamepad 0 in addition to keyboard/mouse, default false
    bool enabled;                         // When false, Update() ignores all input, default true
};

//----------------------------------------------------------------------------------
// CameraController Class Definition
//----------------------------------------------------------------------------------
class CameraController
{
public:
    //--------------------------------------------------------------------
    // Construction
    //--------------------------------------------------------------------
    // Creates a controller for a default camera looking from (10, 10, 10) to the origin
    CameraController()
    {
        mMode = CAMERA_CONTROLLER_FIRST_PERSON;
        mConfig = DefaultConfig();
        mKeys = DefaultKeys();
        Reset(DefaultCamera());
    }

    // Creates a controller for the provided camera, orientation and distance are taken from it
    explicit CameraController(const Camera3D &camera, CameraControllerMode mode = CAMERA_CONTROLLER_FIRST_PERSON)
    {
        mMode = mode;
        mConfig = DefaultConfig();
        mKeys = DefaultKeys();
        Reset(camera);
    }

    //--------------------------------------------------------------------
    // Update
    //--------------------------------------------------------------------
    // Reads keyboard/mouse/gamepad input and advances the camera, call it once per frame
    // [frameTime] is optional, GetFrameTime() is used when it is not provided
    void Update(float frameTime = 0.0f)
    {
        float dt = (frameTime > 0.0f)? frameTime : GetFrameTime();
        if (dt > CAMERA_CONTROLLER_MAX_FRAME_TIME) dt = CAMERA_CONTROLLER_MAX_FRAME_TIME;

        if (mConfig.enabled)
        {
            vec3 velocity = { 0.0f, 0.0f, 0.0f };
            vec2 rotation = { 0.0f, 0.0f };
            float zoom = 0.0f;

            ReadInput(dt, velocity, rotation, zoom);
            UpdateManual(dt, velocity, rotation, zoom);
        }
        else UpdateCameraTransform();
    }

    // Advances the camera using caller-provided input, it never reads raylib input state
    //   [frameTime]: seconds elapsed since the previous call
    //   [velocity]: requested velocity in local camera axes (x-right, y-up, z-forward), in units per second
    //   [rotation]: requested rotation in degrees (x-yaw, y-pitch)
    //   [zoom]: zoom steps, positive zooms in (mouse-wheel like), it drives distance or field-of-view depending on the mode
    void UpdateManual(float frameTime, vec3 velocity, vec2 rotation, float zoom = 0.0f)
    {
        float dt = (frameTime > 0.0f)? frameTime : GetFrameTime();
        if (dt > CAMERA_CONTROLLER_MAX_FRAME_TIME) dt = CAMERA_CONTROLLER_MAX_FRAME_TIME;

        // Requested rotation, pitch is clamped to avoid camera somersaults
        mTargetYaw += rotation.x;
        mTargetPitch = Clampf(mTargetPitch + rotation.y, -mConfig.maxPitch, mConfig.maxPitch);

        // Keep the accumulated angles in a float-safe range, both angles move by the same amount to keep the smoothing continuous
        if (mTargetYaw > 3600.0f) { mTargetYaw -= 3600.0f; mYaw -= 3600.0f; }
        else if (mTargetYaw < -3600.0f) { mTargetYaw += 3600.0f; mYaw += 3600.0f; }

        // Roll is only available in free flight mode, it is kept float-safe the same way the yaw is
        if (mMode == CAMERA_CONTROLLER_FREE)
        {
            mTargetRoll = NearestAngle(mTargetRoll, mRoll);

            if (mTargetRoll > 3600.0f) { mTargetRoll -= 3600.0f; mRoll -= 3600.0f; }
            else if (mTargetRoll < -3600.0f) { mTargetRoll += 3600.0f; mRoll += 3600.0f; }
        }
        else { mTargetRoll = 0.0f; mRoll = 0.0f; }

        // Requested zoom, pivot-based modes change the distance, free-flying modes change the field-of-view
        if (zoom != 0.0f)
        {
            if (UsesPivot()) mTargetDistance = Clampf(mTargetDistance*powf(1.1f, -zoom), mConfig.minDistance, mConfig.maxDistance);
            else mTargetFovy = Clampf(mTargetFovy*powf(1.08f, -zoom), mConfig.minFovy, mConfig.maxFovy);
        }

        // Orbit auto-rotation
        if ((mMode == CAMERA_CONTROLLER_ORBIT) && (mConfig.autoRotate != 0.0f)) mTargetYaw += mConfig.autoRotate*dt;

        // Requested velocity, expressed on the camera basis
        vec3 forward = GetForward();
        vec3 right = GetRight();
        const bool planar = (mConfig.moveInWorldPlane && (mMode != CAMERA_CONTROLLER_FREE));

        if (planar)
        {
            const vec3 planarForward = { forward.x, 0.0f, forward.z };
            const vec3 planarRight = { right.x, 0.0f, right.z };

            if (glm::dot(planarForward, planarForward) > EPSILON) forward = glm::normalize(planarForward);
            else forward = vec3{};

            if (glm::dot(planarRight, planarRight) > EPSILON) right = glm::normalize(planarRight);
            else right = vec3{};
        }

        const vec3 up = planar? vec3{ 0.0f, 1.0f, 0.0f } : GetUp();
        mTargetVelocity = (right*velocity.x) + (up*velocity.y) + (forward*velocity.z);

        // Smooth everything: velocity, rotation, roll, distance and field-of-view
        mVelocity = DampVector(mVelocity, mTargetVelocity, mConfig.moveResponse, dt);
        mPivot = mPivot + mVelocity*dt;

        mYaw = Damp(mYaw, mTargetYaw, mConfig.lookResponse, dt);
        mPitch = Damp(mPitch, mTargetPitch, mConfig.lookResponse, dt);
        mRoll = Damp(mRoll, mTargetRoll, mConfig.rollResponse, dt);

        mDistance = Damp(mDistance, mTargetDistance, mConfig.distanceResponse, dt);
        mFovy = Damp(mFovy, mTargetFovy, mConfig.fovyResponse, dt);

        UpdateCameraTransform();
    }

    //--------------------------------------------------------------------
    // Camera access
    //--------------------------------------------------------------------
    // Returns the camera to pass to BeginMode3D(), it is already updated by Update()
    const Camera3D &GetCamera() const { return mCamera; }

    // Replaces the whole camera state, smoothing targets are synced to it
    void SetCamera(const Camera3D &camera)
    {
        // The pivot is the camera position in first-person/free, the look-at point in third-person/orbit
        mPivot = UsesPivot()? camera.target : camera.position;
        mPosition = camera.position;

        vec3 view = camera.target - camera.position;
        if (glm::dot(view, view) > EPSILON)
        {
            view = glm::normalize(view);
            mYaw = atan2f(view.x, view.z)/DEG2RAD;
            mPitch = asinf(Clampf(view.y, -1.0f, 1.0f))/DEG2RAD;
        }

        mRoll = 0.0f;
        mDistance = glm::distance(camera.position, camera.target);
        if (UsesPivot() && (mDistance < mConfig.minDistance)) mDistance = mConfig.minDistance;

        mFovy = camera.fovy;

        // The configured zoom values follow the camera, so switching modes keeps the same framing
        if (UsesPivot()) mConfig.distance = mDistance;
        mConfig.fovy = mFovy;

        mTargetYaw = mYaw;
        mTargetPitch = mPitch;
        mTargetRoll = mRoll;
        mTargetDistance = mDistance;
        mTargetFovy = mFovy;

        mVelocity = vec3{};
        mTargetVelocity = vec3{};

        mCamera = camera;
        mCamera.up = GetUp();
    }

    // Restores the camera this controller was created (or last reset) with
    void Reset()
    {
        mVelocity = vec3{};
        mTargetVelocity = vec3{};
        Reset(mInitialCamera);
    }

    // Restores the provided camera, it also becomes the camera Reset() returns to
    void Reset(const Camera3D &camera)
    {
        mInitialCamera = camera;

        // The camera is restored exactly as provided, including its distance and its field-of-view
        SetCamera(camera);

        UpdateCameraTransform();
    }

    //--------------------------------------------------------------------
    // Mode
    //--------------------------------------------------------------------
    CameraControllerMode GetMode() const { return mMode; }

    // Changes the mode keeping the current view direction, the pivot is reattached accordingly
    void SetMode(CameraControllerMode mode)
    {
        if (mode == mMode) return;

        if ((mode == CAMERA_CONTROLLER_THIRD_PERSON) || (mode == CAMERA_CONTROLLER_ORBIT))
        {
            mPivot = mCamera.target;
            mDistance = Clampf(mConfig.distance, mConfig.minDistance, mConfig.maxDistance);
            mTargetDistance = mDistance;
        }
        else mPivot = mCamera.position;

        mMode = mode;
        UpdateCameraTransform();
    }

    //--------------------------------------------------------------------
    // Position and orientation
    //--------------------------------------------------------------------
    vec3 GetPosition() const { return mCamera.position; }

    // Moves the camera eye to [position], in third-person and orbit modes the pivot follows the move
    void SetPosition(const vec3 &position)
    {
        if (UsesPivot()) mPivot = position + GetForward()*mDistance;
        else mPivot = position;

        mPosition = position;
        UpdateCameraTransform();
    }

    // Translates the camera rig by [delta], in world units (first-person/free move the eye, third-person/orbit move the pivot)
    void MoveBy(const vec3 &delta)
    {
        mPivot = mPivot + delta;
        UpdateCameraTransform();
    }

    vec3 GetTarget() const { return mCamera.target; }

    // Points the camera to [target], in third-person and orbit modes the pivot is moved to it
    void SetTarget(const vec3 &target, bool snap = false)
    {
        if (UsesPivot()) mPivot = target;
        else LookAt(target, snap);

        UpdateCameraTransform();
    }

    // Rotates the camera to look at [target], keeping the current position
    void LookAt(const vec3 &target, bool snap = false)
    {
        vec3 view = target - mCamera.position;
        if (glm::dot(view, view) <= EPSILON) return;

        view = glm::normalize(view);

        mTargetYaw = NearestAngle(atan2f(view.x, view.z)/DEG2RAD, mYaw);
        mTargetPitch = Clampf(asinf(Clampf(view.y, -1.0f, 1.0f))/DEG2RAD, -mConfig.maxPitch, mConfig.maxPitch);

        if (snap)
        {
            mYaw = mTargetYaw;
            mPitch = mTargetPitch;
            UpdateCameraTransform();
        }
    }

    // Adds a rotation to the requested one, in degrees, it is smoothed by lookResponse
    void RotateBy(float yaw, float pitch, float roll = 0.0f)
    {
        mTargetYaw += yaw;
        mTargetPitch = Clampf(mTargetPitch + pitch, -mConfig.maxPitch, mConfig.maxPitch);
        mTargetRoll += roll;
    }

    // Sets the requested orientation, in degrees, [snap] applies it immediately instead of smoothing it
    void SetYawPitch(float yaw, float pitch, float roll = 0.0f, bool snap = false)
    {
        mTargetYaw = NearestAngle(yaw, mYaw);
        mTargetPitch = Clampf(pitch, -mConfig.maxPitch, mConfig.maxPitch);
        mTargetRoll = NearestAngle(roll, mRoll);

        if (snap)
        {
            mYaw = mTargetYaw;
            mPitch = mTargetPitch;
            mRoll = mTargetRoll;
            UpdateCameraTransform();
        }
    }

    float GetYaw() const { return WrapDegrees(mYaw); }
    float GetPitch() const { return mPitch; }
    float GetRoll() const { return WrapDegrees(mRoll); }

    // Normalized camera basis vectors, [forward] is the direction the camera looks at
    vec3 GetForward() const
    {
        const float yawRad = mYaw*DEG2RAD;
        const float pitchRad = mPitch*DEG2RAD;

        const vec3 forward = { sinf(yawRad)*cosf(pitchRad), sinf(pitchRad), cosf(yawRad)*cosf(pitchRad) };
        return forward;
    }

    vec3 GetUp() const
    {
        vec3 up = { 0.0f, 1.0f, 0.0f };
        if (fabsf(mRoll) > EPSILON) up = Vector3RotateByAxisAngle(up, GetForward(), mRoll*DEG2RAD);
        return up;
    }

    vec3 GetRight() const
    {
        const vec3 right = glm::cross(GetForward(), GetUp());

        // Degenerate case: the camera looks straight up or down, any horizontal vector is a valid right vector
        if (glm::dot(right, right) <= EPSILON)
        {
            const float yawRad = mYaw*DEG2RAD;
            const vec3 horizontalRight = { -cosf(yawRad), 0.0f, sinf(yawRad) };
            return horizontalRight;
        }

        return glm::normalize(right);
    }

    //--------------------------------------------------------------------
    // Movement state
    //--------------------------------------------------------------------
    vec3 GetVelocity() const { return mVelocity; }     // Current smoothed velocity, in world units per second
    float GetSpeed() const { return glm::length(mVelocity); }

    // Stops the camera immediately, clearing both the current and the requested velocity
    void StopMovement()
    {
        mVelocity = vec3{};
        mTargetVelocity = vec3{};
    }

    // Adds velocity to the camera without changing the requested one, useful for hits, knockbacks and camera kicks
    void AddVelocity(const vec3 &impulse) { mVelocity = mVelocity + impulse; }

    //--------------------------------------------------------------------
    // Zoom
    //--------------------------------------------------------------------
    float GetDistance() const { return mDistance; }

    // Sets the pivot distance used by third-person and orbit modes, [snap] applies it immediately instead of smoothing it
    void SetDistance(float distance, bool snap = false)
    {
        mConfig.distance = Clampf(distance, mConfig.minDistance, mConfig.maxDistance);
        mTargetDistance = mConfig.distance;
        if (snap)
        {
            mDistance = mTargetDistance;
            UpdateCameraTransform();
        }
    }

    float GetFovy() const { return mFovy; }

    // Sets the camera field-of-view aperture in Y (degrees), [snap] applies it immediately instead of smoothing it
    void SetFovy(float fovy, bool snap = false)
    {
        mConfig.fovy = Clampf(fovy, mConfig.minFovy, mConfig.maxFovy);
        mTargetFovy = mConfig.fovy;
        if (snap)
        {
            mFovy = mTargetFovy;
            UpdateCameraTransform();
        }
    }

    //--------------------------------------------------------------------
    // Configuration
    //--------------------------------------------------------------------
    const CameraControllerConfig &GetConfig() const { return mConfig; }
    void SetConfig(const CameraControllerConfig &config)
    {
        mConfig = config;
        ClampConfig();
        mTargetDistance = Clampf(mTargetDistance, mConfig.minDistance, mConfig.maxDistance);
        mTargetFovy = Clampf(mTargetFovy, mConfig.minFovy, mConfig.maxFovy);
        mTargetPitch = Clampf(mTargetPitch, -mConfig.maxPitch, mConfig.maxPitch);
    }

    const CameraControllerKeys &GetKeys() const { return mKeys; }
    void SetKeys(const CameraControllerKeys &keys) { mKeys = keys; }

    void SetMoveSpeed(float speed) { mConfig.moveSpeed = speed; }
    float GetMoveSpeed() const { return mConfig.moveSpeed; }

    void SetSprintMultiplier(float multiplier) { mConfig.sprintMultiplier = multiplier; }
    void SetSlowMultiplier(float multiplier) { mConfig.slowMultiplier = multiplier; }

    // Lower moveResponse = heavier, floaty camera; higher = snappy, close to instant
    void SetMoveResponse(float response) { mConfig.moveResponse = response; }
    float GetMoveResponse() const { return mConfig.moveResponse; }

    void SetLookSensitivity(float sensitivity) { mConfig.lookSensitivity = sensitivity; }
    float GetLookSensitivity() const { return mConfig.lookSensitivity; }

    // Lower lookResponse = smoother, laggy look; higher = tighter; 0.0f = instant
    void SetLookResponse(float response) { mConfig.lookResponse = response; }
    float GetLookResponse() const { return mConfig.lookResponse; }

    void SetMaxPitch(float degrees) { mConfig.maxPitch = Clampf(degrees, 0.0f, 180.0f); mTargetPitch = Clampf(mTargetPitch, -mConfig.maxPitch, mConfig.maxPitch); }
    void SetKeyboardLookSpeed(float degreesPerSecond) { mConfig.keyboardLookSpeed = degreesPerSecond; }

    void SetDistanceLimits(float minDistance, float maxDistance)
    {
        mConfig.minDistance = minDistance;
        mConfig.maxDistance = maxDistance;
        ClampConfig();
        SetDistance(mTargetDistance);
    }

    void SetFovyLimits(float minFovy, float maxFovy)
    {
        mConfig.minFovy = minFovy;
        mConfig.maxFovy = maxFovy;
        ClampConfig();
        SetFovy(mTargetFovy);
    }

    void SetAutoRotate(float degreesPerSecond) { mConfig.autoRotate = degreesPerSecond; }
    float GetAutoRotate() const { return mConfig.autoRotate; }

    void SetMoveInWorldPlane(bool enabled) { mConfig.moveInWorldPlane = enabled; }
    void SetInvertY(bool enabled) { mConfig.invertY = enabled; }
    void SetGamepadEnabled(bool enabled) { mConfig.gamepadEnabled = enabled; }

    // Hides and recenters the cursor on Update(), it prevents the mouse look from running out of the window
    void SetCursorLocked(bool locked)
    {
        mConfig.lockCursor = locked;
        if (!locked && mCursorHidden) ShowCursor();
        mCursorHidden = false;
    }

    // Disables input reading, the camera keeps its smoothing and its transform up to date
    void SetEnabled(bool enabled) { mConfig.enabled = enabled; }
    bool IsEnabled() const { return mConfig.enabled; }

    static Camera3D DefaultCamera()
    {
        Camera3D camera = { { 10.0f, 10.0f, 10.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 45.0f, CAMERA_PERSPECTIVE };
        return camera;
    }

    static CameraControllerConfig DefaultConfig()
    {
        CameraControllerConfig config;

        config.moveSpeed = 8.0f;
        config.sprintMultiplier = 2.5f;
        config.slowMultiplier = 0.2f;
        config.moveResponse = 12.0f;

        config.lookSensitivity = 0.08f;
        config.lookResponse = 18.0f;
        config.keyboardLookSpeed = 60.0f;
        config.maxPitch = CAMERA_CONTROLLER_MAX_PITCH;
        config.gamepadLookSpeed = 120.0f;

        config.rollSpeed = 60.0f;
        config.rollResponse = 10.0f;

        config.distance = 10.0f;
        config.minDistance = 1.0f;
        config.maxDistance = 100.0f;
        config.distanceResponse = 10.0f;

        config.fovy = 45.0f;
        config.minFovy = 15.0f;
        config.maxFovy = 120.0f;
        config.fovyResponse = 10.0f;

        config.autoRotate = 0.0f;
        config.gamepadDeadZone = 0.05f;
        config.moveInWorldPlane = true;
        config.invertY = false;
        config.lockCursor = false;
        config.gamepadEnabled = false;
        config.enabled = true;

        return config;
    }

    static CameraControllerKeys DefaultKeys()
    {
        CameraControllerKeys keys;

        keys.forward = KEY_W;
        keys.backward = KEY_S;
        keys.left = KEY_A;
        keys.right = KEY_D;
        keys.ascend = KEY_SPACE;
        keys.descend = KEY_LEFT_CONTROL;
        keys.sprint = KEY_LEFT_SHIFT;
        keys.slow = KEY_LEFT_ALT;
        keys.lookUp = KEY_UP;
        keys.lookDown = KEY_DOWN;
        keys.lookLeft = KEY_LEFT;
        keys.lookRight = KEY_RIGHT;
        keys.rollLeft = KEY_Q;
        keys.rollRight = KEY_E;
        keys.reset = KEY_R;

        return keys;
    }

private:
    //--------------------------------------------------------------------
    // Internal helpers
    //--------------------------------------------------------------------
    bool UsesPivot() const { return ((mMode == CAMERA_CONTROLLER_THIRD_PERSON) || (mMode == CAMERA_CONTROLLER_ORBIT)); }

    // Reads keyboard, mouse and gamepad input into the UpdateManual() parameters
    void ReadInput(float dt, vec3 &velocity, vec2 &rotation, float &zoom)
    {
        float moveX = 0.0f;
        float moveY = 0.0f;
        float moveZ = 0.0f;

        // Keyboard movement
        if (IsKeyDown(mKeys.forward)) moveZ += 1.0f;
        if (IsKeyDown(mKeys.backward)) moveZ -= 1.0f;
        if (IsKeyDown(mKeys.right)) moveX += 1.0f;
        if (IsKeyDown(mKeys.left)) moveX -= 1.0f;
        if (IsKeyDown(mKeys.ascend)) moveY += 1.0f;
        if (IsKeyDown(mKeys.descend)) moveY -= 1.0f;

        float speed = mConfig.moveSpeed;
        if (IsKeyDown(mKeys.sprint)) speed *= mConfig.sprintMultiplier;
        else if (IsKeyDown(mKeys.slow)) speed *= mConfig.slowMultiplier;

        // Mouse look
        const vec2 mouseDelta = GetMouseDelta();
        rotation.x -= mouseDelta.x*mConfig.lookSensitivity;
        rotation.y -= mouseDelta.y*mConfig.lookSensitivity;
        if (mConfig.invertY) rotation.y = -rotation.y;

        // Keyboard look
        if (IsKeyDown(mKeys.lookRight)) rotation.x -= mConfig.keyboardLookSpeed*dt;
        if (IsKeyDown(mKeys.lookLeft)) rotation.x += mConfig.keyboardLookSpeed*dt;
        if (IsKeyDown(mKeys.lookUp)) rotation.y += mConfig.keyboardLookSpeed*dt;
        if (IsKeyDown(mKeys.lookDown)) rotation.y -= mConfig.keyboardLookSpeed*dt;

        // Roll, free flight mode only
        if (mMode == CAMERA_CONTROLLER_FREE)
        {
            if (IsKeyDown(mKeys.rollLeft)) mTargetRoll -= mConfig.rollSpeed*dt;
            if (IsKeyDown(mKeys.rollRight)) mTargetRoll += mConfig.rollSpeed*dt;
        }

        // Mouse wheel zoom
        zoom = GetMouseWheelMove();

        // Gamepad movement and look
        if (mConfig.gamepadEnabled && IsGamepadAvailable(0))
        {
            moveX += ApplyDeadZone(GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X));
            moveZ -= ApplyDeadZone(GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y));
            moveY += ApplyDeadZone(GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_TRIGGER));
            moveY -= ApplyDeadZone(GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_TRIGGER));

            rotation.x -= ApplyDeadZone(GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X))*mConfig.gamepadLookSpeed*dt;
            rotation.y -= ApplyDeadZone(GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y))*mConfig.gamepadLookSpeed*dt;

            if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_2)) speed *= mConfig.sprintMultiplier;
        }

        // Reset view
        if (IsKeyPressed(mKeys.reset)) Reset();

        // Keep the cursor in the middle of the window so the look input never runs out
        if (mConfig.lockCursor)
        {
            if (!mCursorHidden)
            {
                HideCursor();
                mCursorHidden = true;
            }

            SetMousePosition(GetScreenWidth()/2, GetScreenHeight()/2);
        }

        vec3 input = { moveX, moveY, moveZ };
        if (glm::dot(input, input) > 1.0f) input = glm::normalize(input);

        velocity = input*speed;
    }

    // Writes the smoothed state into the raylib camera
    void UpdateCameraTransform()
    {
        const vec3 forward = GetForward();

        if (UsesPivot())
        {
            mCamera.target = mPivot;
            mCamera.position = mPivot - forward*mDistance;
        }
        else
        {
            mCamera.position = mPivot;
            mCamera.target = mPivot + forward*CAMERA_CONTROLLER_LOOK_AHEAD;
        }

        mPosition = mCamera.position;
        mCamera.up = GetUp();
        mCamera.fovy = mFovy;
    }

    void ClampConfig()
    {
        if (mConfig.minDistance < 0.001f) mConfig.minDistance = 0.001f;
        if (mConfig.maxDistance < mConfig.minDistance) mConfig.maxDistance = mConfig.minDistance;
        if (mConfig.minFovy < 1.0f) mConfig.minFovy = 1.0f;
        if (mConfig.maxFovy < mConfig.minFovy) mConfig.maxFovy = mConfig.minFovy;
        if (mConfig.maxPitch < 0.0f) mConfig.maxPitch = 0.0f;
        if (mConfig.gamepadDeadZone < 0.0f) mConfig.gamepadDeadZone = 0.0f;
        if (mConfig.gamepadDeadZone > 0.95f) mConfig.gamepadDeadZone = 0.95f;
    }

    // Gamepad axes are rarely perfectly centered, the remaining range is rescaled to [0..1]
    float ApplyDeadZone(float value) const
    {
        if (fabsf(value) < mConfig.gamepadDeadZone) return 0.0f;

        const float sign = (value < 0.0f)? -1.0f : 1.0f;
        return sign*((fabsf(value) - mConfig.gamepadDeadZone)/(1.0f - mConfig.gamepadDeadZone));
    }

    // Exponential smoothing, frame-time based: [response] is how fast (1/seconds) the value reaches [target]
    static float Damp(float current, float target, float response, float dt)
    {
        if (response <= 0.0f) return target;
        if (dt <= 0.0f) return current;

        return target + (current - target)*expf(-response*dt);
    }

    static vec3 DampVector(vec3 current, vec3 target, float response, float dt)
    {
        const vec3 result = { Damp(current.x, target.x, response, dt), Damp(current.y, target.y, response, dt), Damp(current.z, target.z, response, dt) };
        return result;
    }

    static float Clampf(float value, float minValue, float maxValue)
    {
        if (value < minValue) return minValue;
        if (value > maxValue) return maxValue;
        return value;
    }

    static float WrapDegrees(float angle)
    {
        while (angle > 180.0f) angle -= 360.0f;
        while (angle < -180.0f) angle += 360.0f;
        return angle;
    }

    // Chooses the equivalent angle closer to [current], so smoothing never takes the long way around
    static float NearestAngle(float angle, float current)
    {
        while (angle - current > 180.0f) angle -= 360.0f;
        while (angle - current < -180.0f) angle += 360.0f;
        return angle;
    }

    //--------------------------------------------------------------------
    // Internal state
    //--------------------------------------------------------------------
    Camera3D mCamera;                   // Camera returned by GetCamera()
    Camera3D mInitialCamera;            // Camera restored by Reset()
    CameraControllerMode mMode;         // Current movement mode
    CameraControllerConfig mConfig;     // Tunable parameters
    CameraControllerKeys mKeys;         // Keyboard bindings

    vec3 mPivot;                   // Point the controller moves: the camera position (first-person/free) or the look-at point (third-person/orbit)
    vec3 mPosition;                // Camera eye position, mirror of mCamera.position
    vec3 mVelocity;                // Current smoothed velocity, world units per second
    vec3 mTargetVelocity;          // Velocity the camera is accelerating to

    float mYaw;                         // Current yaw (degrees), rotation around the world up vector
    float mPitch;                       // Current pitch (degrees)
    float mRoll;                        // Current roll (degrees), free flight mode only
    float mTargetYaw;                   // Requested yaw (degrees)
    float mTargetPitch;                 // Requested pitch (degrees)
    float mTargetRoll;                  // Requested roll (degrees)

    float mDistance;                    // Current pivot distance (third-person/orbit)
    float mTargetDistance;              // Requested pivot distance
    float mFovy;                        // Current field-of-view aperture in Y (degrees)
    float mTargetFovy;                  // Requested field-of-view

    bool mCursorHidden;                 // Cursor visibility state managed by SetCursorLocked()
};

#endif // CAMERA_CONTROLLER_HPP
