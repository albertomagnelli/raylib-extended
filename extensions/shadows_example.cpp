/*******************************************************************************************
*
*   raylib-extended [shadows] - example 03: 3d shapes with directional light and shadow mapping
*
*   Ported from the "mx Phase 9" demo of the RayLib-Demo project
*   (examples/phase9_shadows/main.cpp), the mx render hardware interface calls are
*   replaced by their raylib/rlgl equivalents:
*
*       mx::gpu::Device / Framebuffer          ->  rlLoadFramebuffer() + rlLoadTextureDepth()
*       mx::gpu::Program                       ->  LoadShaderFromMemory()
*       mx::gpu::MeshBuffer                    ->  GenMesh*() + own VAO/VBO upload through rlgl
*       mx::gpu::Pipeline                      ->  rlEnableShader() / rlEnableBackfaceCulling()
*       mx::gpu::Sampler (nearest, clamp)      ->  the defaults of rlLoadTextureDepth()
*       mx::make*Mesh()                        ->  GenMesh*() from the models module
*       glm math                               ->  glm, fetched with FetchContent in CMake
*
*   The original shaders asked for GLSL 460, the mx engine created a 4.6 context for that.
*   raylib defaults to an OpenGL 3.3 core context, so the shaders use #version 330,
*   the lighting, the 3x3 PCF and the tonemapping logic are unchanged.
*
*   Controls:
*       MOUSE LEFT BUTTON : Orbit the camera around the scene
*       MOUSE WHEEL       : Zoom in and out
*       L                 : Pause / resume the animated light
*       R                 : Reset camera and light
*       F11               : Toggle fullscreen
*       ESC               : Close the window
*       F12               : Save a screenshot (raylib built-in)
*
*   License: zlib/libpng
**********************************************************************************************/

#include "raylib.h"
#include "rlgl.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>

constexpr int screenWidth = 1280;
constexpr int screenHeight = 720;

constexpr int shadowSize = 2048;
constexpr int shadowUnit = 15;
constexpr float shadowExtent = 10.0f;

const Color background = { 4, 5, 8, 255 };
const Color panelColor = { 0, 0, 0, 140 };

static const char *shadowVertexShader = R"glsl(
#version 330

layout(location = 0) in vec3 aPosition;

uniform mat4 uModel;
uniform mat4 uLightViewProjection;

void main() {
    gl_Position = uLightViewProjection * uModel * vec4(aPosition, 1.0);
}
)glsl";

static const char *shadowFragmentShader = R"glsl(
#version 330

void main() {
    // Reiner Tiefenpass: Es gibt keinen Farbanhang.
}
)glsl";

static const char *litVertexShader = R"glsl(
#version 330

layout(location = 0) in vec3 aPosition;
layout(location = 2) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uViewProjection;
uniform mat4 uLightViewProjection;

out vec3 vWorldPosition;
out vec3 vNormalWorld;
out vec4 vLightSpacePosition;

void main() {
    vec4 world = uModel * vec4(aPosition, 1.0);

    vWorldPosition = world.xyz;
    vNormalWorld = mat3(uModel) * aNormal;
    vLightSpacePosition = uLightViewProjection * world;
    gl_Position = uViewProjection * world;
}
)glsl";

static const char *litFragmentShader = R"glsl(
#version 330

in vec3 vWorldPosition;
in vec3 vNormalWorld;
in vec4 vLightSpacePosition;

uniform vec4 uColor;
uniform vec3 uCameraPosition;
uniform vec3 uLightDirection;
uniform vec3 uLightColor;
uniform float uAmbient;

uniform sampler2D uShadowMap;
uniform float uShadowBias;
uniform vec2 uShadowTexelSize;

out vec4 FragColor;

float shadowFactor(vec3 normal, float nDotL) {
    vec3 projection = vLightSpacePosition.xyz / vLightSpacePosition.w;
    projection = projection * 0.5 + 0.5;

    if (projection.z <= 0.0 || projection.z >= 1.0 ||
        projection.x <= 0.0 || projection.x >= 1.0 ||
        projection.y <= 0.0 || projection.y >= 1.0) {
        return 1.0;
    }

    float bias = max(uShadowBias * (1.0 - nDotL), uShadowBias * 0.15);

    float visible = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(float(x), float(y)) * uShadowTexelSize;
            float closest = texture(uShadowMap, projection.xy + offset).r;
            visible += (projection.z - bias > closest) ? 0.0 : 1.0;
        }
    }

    return visible / 9.0;
}

void main() {
    vec3 normal = normalize(vNormalWorld);
    vec3 lightDirection = normalize(-uLightDirection);

    float nDotL = max(dot(normal, lightDirection), 0.0);
    float shadow = shadowFactor(normal, nDotL);

    vec3 viewDirection = normalize(uCameraPosition - vWorldPosition);
    vec3 halfway = normalize(viewDirection + lightDirection);
    float specular = pow(max(dot(normal, halfway), 0.0), 48.0) * 0.35 * nDotL;

    vec3 baseColor = uColor.rgb;
    vec3 ambient = baseColor * uAmbient;
    vec3 diffuse = baseColor * uLightColor * nDotL;
    vec3 highlight = uLightColor * specular;

    vec3 color = ambient + (diffuse + highlight) * shadow;

    // Reinhard-Tonemapping und Gamma, damit die Szene nicht ausbrennt.
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, uColor.a);
}
)glsl";

// Interleaved vertex, the same layout the original mx vertex layout provided
struct SceneVertex
{
    glm::vec3 position;
    glm::vec3 normal;
};

// A mesh uploaded for the custom shaders: own VAO, interleaved VBO and optional index buffer
struct SceneMesh
{
    unsigned int vaoId = 0;
    unsigned int vboId = 0;
    unsigned int eboId = 0;
    int vertexCount = 0;
    int indexCount = 0;
    bool indexed = false;
};

struct SceneObject
{
    const SceneMesh *mesh = nullptr;
    glm::mat4 model = glm::mat4(1.0F);
    glm::vec4 color = glm::vec4(1.0F);
    bool castsShadow = true;
};

struct OrbitCamera
{
    float yaw = 0.65f;
    float pitch = 0.32f;
    float distance = 14.0f;
    glm::vec3 target = { 0.0f, 0.8f, 0.0f };
};

static glm::vec3 OrbitPosition(const OrbitCamera &camera)
{
    const float cosPitch = cosf(camera.pitch);

    return camera.target + glm::vec3(camera.distance*cosPitch*sinf(camera.yaw),
                                     camera.distance*sinf(camera.pitch),
                                     camera.distance*cosPitch*cosf(camera.yaw));
}

static SceneMesh UploadSceneMesh(Mesh mesh)
{
    SceneMesh result;

    std::vector<SceneVertex> vertices;
    vertices.reserve(mesh.vertexCount);

    for (int i = 0; i < mesh.vertexCount; i++)
    {
        SceneVertex vertex;
        vertex.position = { mesh.vertices[i*3], mesh.vertices[i*3 + 1], mesh.vertices[i*3 + 2] };
        vertex.normal = { mesh.normals[i*3], mesh.normals[i*3 + 1], mesh.normals[i*3 + 2] };
        vertices.push_back(vertex);
    }

    result.vertexCount = mesh.vertexCount;
    result.indexed = (mesh.indices != nullptr);
    result.indexCount = mesh.triangleCount*3;

    result.vaoId = rlLoadVertexArray();
    rlEnableVertexArray(result.vaoId);

    result.vboId = rlLoadVertexBuffer(vertices.data(), static_cast<int>(vertices.size())*static_cast<int>(sizeof(SceneVertex)), false);
    rlEnableVertexBuffer(result.vboId);

    // The attribute locations the shaders declare: position at 0, normal at 2
    rlSetVertexAttribute(0, 3, RL_FLOAT, 0, static_cast<int>(sizeof(SceneVertex)), 0);
    rlEnableVertexAttribute(0);
    rlSetVertexAttribute(2, 3, RL_FLOAT, 0, static_cast<int>(sizeof(SceneVertex)), static_cast<int>(sizeof(glm::vec3)));
    rlEnableVertexAttribute(2);

    // NOTE: The element buffer is loaded while the VAO is active, so it stays bound to it.
    // GenMeshSphere/Cylinder/Cone/Torus come out unindexed, only plane and cube have indices
    if (result.indexed)
    {
        result.eboId = rlLoadVertexBufferElement(mesh.indices, result.indexCount*static_cast<int>(sizeof(unsigned short)), false);
        rlEnableVertexBufferElement(result.eboId);
    }

    rlDisableVertexArray();

    // The CPU side mesh data is not needed anymore, it was never uploaded to the raylib batch
    UnloadMesh(mesh);

    return result;
}

static void UnloadSceneMesh(SceneMesh &mesh)
{
    rlUnloadVertexArray(mesh.vaoId);
    rlUnloadVertexBuffer(mesh.vboId);
    if (mesh.eboId > 0) rlUnloadVertexBuffer(mesh.eboId);

    mesh = SceneMesh{ };
}

static void DrawSceneMesh(const SceneMesh &mesh)
{
    rlEnableVertexArray(mesh.vaoId);
    if (mesh.indexed) rlDrawVertexArrayElements(0, mesh.indexCount, nullptr);
    else rlDrawVertexArray(0, mesh.vertexCount);
    rlDisableVertexArray();
}

// Framebuffer with only a depth texture attachment, the shadow map
static RenderTexture2D LoadShadowMapRenderTexture(int width, int height)
{
    RenderTexture2D target = { 0 };

    target.id = rlLoadFramebuffer();
    target.texture.width = width;
    target.texture.height = height;

    if (target.id > 0)
    {
        rlEnableFramebuffer(target.id);

        // rlLoadTextureDepth() already sets NEAREST filtering and CLAMP_TO_EDGE,
        // which is what the shadow sampler of the original demo configured
        target.depth.id = rlLoadTextureDepth(width, height, false);
        target.depth.width = width;
        target.depth.height = height;
        target.depth.format = 19; // DEPTH_COMPONENT_24BIT?
        target.depth.mipmaps = 1;

        rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

        if (rlFramebufferComplete(target.id)) TRACELOG(LOG_INFO, "FBO: [ID %i] Framebuffer object created successfully", target.id);
        else TRACELOG(LOG_WARNING, "FBO: Framebuffer object can not be completed");

        rlDisableFramebuffer();
    }
    else TRACELOG(LOG_WARNING, "FBO: Framebuffer object can not be created");

    return target;
}

static void UnloadShadowMapRenderTexture(RenderTexture2D target)
{
    if (target.id > 0)
    {
        // NOTE: The depth texture is queried and deleted by rlgl itself
        rlUnloadFramebuffer(target.id);
    }
}

int main()
{
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);
    InitWindow(screenWidth, screenHeight, "raylib-extended [shadows] - 3d shapes with directional light and shadow mapping");
    if (!IsWindowReady()) return EXIT_FAILURE;

    // --- Programs -----------------------------------------------------------

    const Shader litShader = LoadShaderFromMemory(litVertexShader, litFragmentShader);
    const Shader shadowShader = LoadShaderFromMemory(shadowVertexShader, shadowFragmentShader);

    if (litShader.id == 0 || shadowShader.id == 0)
    {
        TraceLog(LOG_ERROR, "SHADER: Konnte die Licht- oder Schatten-Programme nicht laden");
        CloseWindow();
        return EXIT_FAILURE;
    }

    // --- Meshes -------------------------------------------------------------

    SceneMesh planeMesh = UploadSceneMesh(GenMeshPlane(20.0f, 20.0f, 1, 1));
    SceneMesh cubeMesh = UploadSceneMesh(GenMeshCube(1.4f, 1.4f, 1.4f));
    SceneMesh sphereMesh = UploadSceneMesh(GenMeshSphere(0.8f, 32, 64));
    SceneMesh cylinderMesh = UploadSceneMesh(GenMeshCylinder(0.45f, 2.6f, 32));
    SceneMesh coneMesh = UploadSceneMesh(GenMeshCone(0.8f, 1.8f, 32));
    SceneMesh torusMesh = UploadSceneMesh(GenMeshTorus(0.65f, 0.24f, 32, 16));

    // --- Shadow map ---------------------------------------------------------

    const RenderTexture2D shadowMap = LoadShadowMapRenderTexture(shadowSize, shadowSize);
    if (shadowMap.id == 0)
    {
        TraceLog(LOG_ERROR, "FBO: Konnte die Shadow-Map nicht anlegen");
        CloseWindow();
        return EXIT_FAILURE;
    }

    // --- Uniforms -----------------------------------------------------------

    const int litModel = GetShaderLocation(litShader, "uModel");
    const int litViewProjection = GetShaderLocation(litShader, "uViewProjection");
    const int litLightViewProjection = GetShaderLocation(litShader, "uLightViewProjection");
    const int litColor = GetShaderLocation(litShader, "uColor");
    const int litCameraPosition = GetShaderLocation(litShader, "uCameraPosition");
    const int litLightDirection = GetShaderLocation(litShader, "uLightDirection");
    const int litLightColor = GetShaderLocation(litShader, "uLightColor");
    const int litAmbient = GetShaderLocation(litShader, "uAmbient");
    const int litShadowMap = GetShaderLocation(litShader, "uShadowMap");
    const int litShadowBias = GetShaderLocation(litShader, "uShadowBias");
    const int litShadowTexelSize = GetShaderLocation(litShader, "uShadowTexelSize");

    const int shadowModel = GetShaderLocation(shadowShader, "uModel");
    const int shadowLightViewProjection = GetShaderLocation(shadowShader, "uLightViewProjection");

    int samplerSlot = shadowUnit;
    SetShaderValue(litShader, litShadowMap, &samplerSlot, SHADER_UNIFORM_SAMPLER2D);

    float shadowBias = 0.0015f;
    SetShaderValue(litShader, litShadowBias, &shadowBias, SHADER_UNIFORM_FLOAT);

    glm::vec2 shadowTexelSize(1.0F/static_cast<float>(shadowSize), 1.0F/static_cast<float>(shadowSize));
    SetShaderValue(litShader, litShadowTexelSize, glm::value_ptr(shadowTexelSize), SHADER_UNIFORM_VEC2);

    float ambient = 0.18f;
    SetShaderValue(litShader, litAmbient, &ambient, SHADER_UNIFORM_FLOAT);

    glm::vec3 lightColor = glm::vec3(1.0F, 0.94F, 0.82F)*3.0F;
    SetShaderValue(litShader, litLightColor, glm::value_ptr(lightColor), SHADER_UNIFORM_VEC3);

    // --- Scene and camera ---------------------------------------------------

    OrbitCamera camera;
    bool lightAnimated = true;
    float lightTime = 0.0f;

    const glm::vec3 lightBaseDirection = glm::normalize(glm::vec3(-0.65F, -0.60F, -0.45F));
    const glm::vec3 lightTarget = { 0.0F, 0.6F, 0.0F };
    const glm::vec3 worldUp = { 0.0F, 1.0F, 0.0F };

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
        if (IsKeyPressed(KEY_L)) lightAnimated = !lightAnimated;
        if (IsKeyPressed(KEY_R)) { camera = OrbitCamera{ }; lightTime = 0.0f; }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            const glm::vec2 mouseDelta = GetMouseDelta();
            camera.yaw -= mouseDelta.x*0.005f;
            camera.pitch = std::clamp(camera.pitch + mouseDelta.y*0.005f, 0.08f, 1.45f);
        }

        const float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) camera.distance = std::clamp(camera.distance - wheel*0.8f, 4.0f, 28.0f);

        BeginDrawing();
            ClearBackground(background);

            const float delta = GetFrameTime();
            const double time = GetTime();
            if (lightAnimated) lightTime += delta;

            const glm::mat4 lightRotation = glm::rotate(glm::mat4(1.0F), lightTime*0.25f, worldUp);
            glm::vec3 lightDirection = glm::normalize(glm::vec3(lightRotation*glm::vec4(lightBaseDirection, 0.0F)));

            const glm::vec3 lightPosition = lightTarget - lightDirection*22.0F;
            const glm::mat4 lightView = glm::lookAt(lightPosition, lightTarget, worldUp);
            const glm::mat4 lightProjection = glm::ortho(-shadowExtent, shadowExtent, -shadowExtent, shadowExtent, 1.0f, 45.0f);
            const glm::mat4 lightViewProjection = lightProjection*lightView;

            glm::vec3 cameraPosition = OrbitPosition(camera);

            // --- Build the scene ----------------------------------------------

            std::array<SceneObject, 6> objects{ };

            objects[0].mesh = &planeMesh;
            objects[0].color = { 0.30F, 0.32F, 0.36F, 1.0F };
            objects[0].castsShadow = false;

            objects[1].mesh = &cubeMesh;
            objects[1].model = glm::rotate(
                glm::translate(glm::mat4(1.0F), glm::vec3(-3.2F, 0.7F, 0.0F)),
                static_cast<float>(time)*0.4F, glm::vec3(0.25F, 1.0F, 0.2F));
            objects[1].color = { 0.95F, 0.45F, 0.15F, 1.0F };

            objects[2].mesh = &sphereMesh;
            objects[2].model = glm::translate(glm::mat4(1.0F), glm::vec3(-1.6F, 0.8F, 0.0F));
            objects[2].color = { 0.22F, 0.55F, 0.95F, 1.0F };

            objects[3].mesh = &cylinderMesh;
            objects[3].model = glm::translate(glm::mat4(1.0F), glm::vec3(0.0F, 1.3F, 0.0F));
            objects[3].color = { 0.30F, 0.80F, 0.40F, 1.0F };

            objects[4].mesh = &coneMesh;
            objects[4].model = glm::rotate(
                glm::translate(glm::mat4(1.0F), glm::vec3(1.8F, 0.9F, 0.0F)),
                static_cast<float>(time)*0.5F, glm::vec3(0.0F, 1.0F, 0.0F));
            objects[4].color = { 0.95F, 0.80F, 0.20F, 1.0F };

            objects[5].mesh = &torusMesh;
            objects[5].model = glm::rotate(
                glm::translate(glm::mat4(1.0F), glm::vec3(3.4F, 0.55F, 0.0F)),
                static_cast<float>(time)*0.3F, glm::vec3(0.0F, 1.0F, 0.0F));
            objects[5].color = { 0.75F, 0.35F, 0.90F, 1.0F };

            // --- Shadow pass --------------------------------------------------

            rlEnableFramebuffer(shadowMap.id);
            rlViewport(0, 0, shadowSize, shadowSize);
            rlClearColor(255, 255, 255, 255);
            rlClearScreenBuffers();

            rlEnableShader(shadowShader.id);
            rlDisableBackfaceCulling();
            // raylib initializes with the depth test disabled, a custom pass has to enable it
            // itself or nothing is written into the depth texture of the shadow map
            rlEnableDepthTest();

            SetShaderValueMatrix(shadowShader, shadowLightViewProjection, lightViewProjection);

            for (const SceneObject &object : objects)
            {
                if (!object.castsShadow) continue;

                SetShaderValueMatrix(shadowShader, shadowModel, object.model);
                DrawSceneMesh(*object.mesh);
            }

            // --- Main pass ----------------------------------------------------

            rlDisableFramebuffer();
            rlViewport(0, 0, GetRenderWidth(), GetRenderHeight());

            const float aspect = (GetRenderWidth() > 0 && GetRenderHeight() > 0)?
                static_cast<float>(GetRenderWidth())/static_cast<float>(GetRenderHeight()) : 1.0f;
            const glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
            const glm::mat4 view = glm::lookAt(cameraPosition, camera.target, worldUp);

            rlEnableShader(litShader.id);
            rlEnableBackfaceCulling();
            rlEnableDepthTest();            // the batch of BeginMode3D would do this, the custom pass has to do it itself

            SetShaderValueMatrix(litShader, litViewProjection, projection*view);
            SetShaderValueMatrix(litShader, litLightViewProjection, lightViewProjection);
            SetShaderValue(litShader, litCameraPosition, glm::value_ptr(cameraPosition), SHADER_UNIFORM_VEC3);
            SetShaderValue(litShader, litLightDirection, glm::value_ptr(lightDirection), SHADER_UNIFORM_VEC3);

            rlActiveTextureSlot(shadowUnit);
            rlEnableTexture(shadowMap.depth.id);

            for (const SceneObject &object : objects)
            {
                SetShaderValueMatrix(litShader, litModel, object.model);
                SetShaderValue(litShader, litColor, glm::value_ptr(object.color), SHADER_UNIFORM_VEC4);
                DrawSceneMesh(*object.mesh);
            }

            // Restore the state the raylib batch expects for the 2d overlay
            rlActiveTextureSlot(shadowUnit);
            rlDisableTexture();
            rlActiveTextureSlot(0);
            rlDisableBackfaceCulling();
            rlDisableDepthTest();
            rlEnableShader(rlGetShaderIdDefault());

            // --- Overlay --------------------------------------------------------

            DrawRectangleRec({ 24.0f, 24.0f, 760.0f, 152.0f }, panelColor);
            DrawText("raylib-extended - Phase 9: Schattenwurf", 40, 38, 24, RAYWHITE);
            DrawText(TextFormat("Directional Light | Shadow Map %ix%i | 3x3 PCF | %i FPS",
                                shadowSize, shadowSize, GetFPS()), 40, 76, 17, SKYBLUE);
            DrawText("Linke Maus: Orbit | Rad: Zoom | L: Licht bewegen | R: Reset | F11: Vollbild",
                     40, 104, 15, LIGHTGRAY);
            DrawText(TextFormat("Grundkoerper: Cube | Sphere | Cylinder | Cone | Torus | Licht %s",
                                lightAnimated? "aktiv" : "pausiert"), 40, 132, 15, GRAY);
        EndDrawing();
    }

    UnloadShadowMapRenderTexture(shadowMap);

    UnloadSceneMesh(planeMesh);
    UnloadSceneMesh(cubeMesh);
    UnloadSceneMesh(sphereMesh);
    UnloadSceneMesh(cylinderMesh);
    UnloadSceneMesh(coneMesh);
    UnloadSceneMesh(torusMesh);

    UnloadShader(litShader);
    UnloadShader(shadowShader);

    CloseWindow();

    return EXIT_SUCCESS;
}
