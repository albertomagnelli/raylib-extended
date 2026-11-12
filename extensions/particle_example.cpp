/*******************************************************************************************
*
*   raylib-extended [particle] - example 02: 2d particle playground
*
*   Ported from the "mx Partikel-Demo" of the RayLib-Demo project (examples/app/main.cpp),
*   the mx engine calls are replaced by their raylib equivalents:
*
*       mx::Window::create({ .multisample = 4 })   ->  SetWindowSamples(4) before InitWindow()
*       mx::Window setMinSize / setFullscreen      ->  SetWindowMinSize() / ToggleFullscreen()
*       mx::Input isMouseDown / mousePosition      ->  IsMouseButtonDown() / GetMousePosition()
*       mx::Renderer2D drawLine/drawCircle/drawRect->  DrawLineEx() / DrawCircleV() / DrawRectangle()
*       mx::Font::loadFromFile + measureText       ->  LoadFontEx() / MeasureTextEx()
*       mx::Clock (maxDelta) + FrameLimiter{120}   ->  clamped GetFrameTime() + SetTargetFPS(120)
*
*   Controls:
*       MOUSE         : The particles are attracted to the cursor while the left button is held
*       SPACE         : Particle explosion at the cursor
*       R             : Clear all particles
*       F11           : Toggle fullscreen
*       ESC           : Close the window
*
*   License: zlib/libpng
**********************************************************************************************/

#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

constexpr int screenWidth = 1280;
constexpr int screenHeight = 720;
constexpr int maxParticles = 900;

struct Particle
{
    vec2 position = { 0.0f, 0.0f };
    vec2 velocity = { 0.0f, 0.0f };
    float radius = 1.0f;
    float life = 1.0f;
    float maxLife = 1.0f;
    Color color = RAYWHITE;
};

const Color backgroundTop{ 12, 14, 30, 255 };
const Color backgroundBottom{ 28, 12, 44, 255 };
const Color accent{ 0, 210, 255, 255 };
const Color accentWarm{ 255, 90, 160, 255 };
const Color textPrimary{ 235, 240, 255, 255 };
const Color textSecondary{ 150, 160, 200, 255 };
const Color panelColor{ 8, 10, 22, 255 };

const Color palette[5] = {
    Color{ 0, 220, 255, 255 },
    Color{ 90, 140, 255, 255 },
    Color{ 200, 90, 255, 255 },
    Color{ 255, 90, 160, 255 },
    Color{ 255, 190, 80, 255 },
};

static std::mt19937 &RandomGenerator()
{
    static std::mt19937 generator{ std::random_device{}() };
    return generator;
}

static float RandomFloat(float minimum, float maximum)
{
    std::uniform_real_distribution<float> distribution(minimum, maximum);
    return distribution(RandomGenerator());
}

static int RandomIndex(int count)
{
    std::uniform_int_distribution<int> distribution(0, count - 1);
    return distribution(RandomGenerator());
}

static Color WithAlpha(Color color, float alpha)
{
    color.a = static_cast<unsigned char>(std::clamp(alpha, 0.0f, 1.0f)*255.0f);
    return color;
}

static Color MixColor(Color first, Color second, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);

    Color result{
        static_cast<unsigned char>(static_cast<float>(first.r) + static_cast<float>(second.r - first.r)*t),
        static_cast<unsigned char>(static_cast<float>(first.g) + static_cast<float>(second.g - first.g)*t),
        static_cast<unsigned char>(static_cast<float>(first.b) + static_cast<float>(second.b - first.b)*t),
        255,
    };

    return result;
}

// First system font that can be found, NULL falls back to the raylib default font
static const char *FindSystemFont(void)
{
    const char *candidates[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    };

    for (const char *candidate : candidates)
    {
        if (FileExists(candidate)) return candidate;
    }

    return nullptr;
}

static void SpawnParticle(std::vector<Particle> &particles, vec2 origin, vec2 direction, float speed)
{
    if (static_cast<int>(particles.size()) >= maxParticles) return;

    Particle particle;
    particle.position = { origin.x + RandomFloat(-12.0f, 12.0f), origin.y + RandomFloat(-12.0f, 12.0f) };
    particle.velocity = { direction.x*speed, direction.y*speed };
    particle.radius = RandomFloat(1.5f, 4.5f);
    particle.maxLife = RandomFloat(1.2f, 3.2f);
    particle.life = particle.maxLife;
    particle.color = palette[RandomIndex(5)];

    particles.push_back(particle);
}

static void SpawnBurst(std::vector<Particle> &particles, vec2 origin, int count)
{
    for (int i = 0; i < count; i++)
    {
        const float angle = RandomFloat(0.0f, 2.0f*static_cast<float>(std::numbers::pi));
        SpawnParticle(particles, origin, { cosf(angle), sinf(angle) }, RandomFloat(120.0f, 460.0f));
    }
}

static void UpdateParticles(std::vector<Particle> &particles, vec2 mouse, bool attracting, vec2 bounds, float delta)
{
    const float damping = expf(-1.6f*delta);

    for (Particle &particle : particles)
    {
        particle.life -= delta;

        if (attracting)
        {
            const vec2 toMouse = { mouse.x - particle.position.x, mouse.y - particle.position.y };
            const float distance = std::max(glm::length(toMouse), 1.0f);
            const float strength = 3200.0f/(distance + 80.0f);

            particle.velocity.x += toMouse.x*(strength*delta/distance);
            particle.velocity.y += toMouse.y*(strength*delta/distance);
        }

        particle.velocity.x *= damping;
        particle.velocity.y *= damping;
        particle.position.x += particle.velocity.x*delta;
        particle.position.y += particle.velocity.y*delta;

        // Bounce on the screen edges, keeping some energy
        const float radius = particle.radius;

        if (particle.position.x < radius)
        {
            particle.position.x = radius;
            particle.velocity.x = fabsf(particle.velocity.x)*0.7f;
        }
        else if (particle.position.x > bounds.x - radius)
        {
            particle.position.x = bounds.x - radius;
            particle.velocity.x = -fabsf(particle.velocity.x)*0.7f;
        }

        if (particle.position.y < radius)
        {
            particle.position.y = radius;
            particle.velocity.y = fabsf(particle.velocity.y)*0.7f;
        }
        else if (particle.position.y > bounds.y - radius)
        {
            particle.position.y = bounds.y - radius;
            particle.velocity.y = -fabsf(particle.velocity.y)*0.7f;
        }
    }

    std::erase_if(particles, [](const Particle &particle) { return particle.life <= 0.0f; });
}

static void DrawBackground(vec2 size, float time)
{
    const int width = static_cast<int>(size.x);
    const int height = static_cast<int>(size.y);

    // Vertical gradient, one line pair of pixels per rectangle
    for (int y = 0; y < height; y += 2)
    {
        const float t = static_cast<float>(y)/size.y;
        DrawRectangle(0, y, size.x, 2.0f, MixColor(backgroundTop, backgroundBottom, t));
    }

    // Soft pulsing glow behind everything
    const vec2 center = { size.x*0.5f, size.y*0.5f };
    const float pulse = 1.0f + 0.03f*sinf(time*1.7f);
    const float maxRadius = std::max(size.x, size.y)*0.55f*pulse;

    for (int layer = 18; layer > 0; layer--)
    {
        const float t = static_cast<float>(layer)/18.0f;
        DrawCircleV(center, maxRadius*t, WithAlpha(MixColor(accent, accentWarm, t), 0.012f));
    }

    // Faint grid
    for (int x = 0; x < width; x += 64)
        DrawLineEx({ static_cast<float>(x), 0.0f }, { static_cast<float>(x), size.y }, 1.0f, WithAlpha(RAYWHITE, 0.025f));

    for (int y = 0; y < height; y += 64)
        DrawLineEx({ 0.0f, static_cast<float>(y) }, { size.x, static_cast<float>(y) }, 1.0f, WithAlpha(RAYWHITE, 0.025f));
}

static void DrawOrbit(vec2 center, vec2 size, float time)
{
    // The original demo rotates in radians, raylib polygons are rotated in degrees
    const float rotation = time*18.0f;

    const float screenMin = std::min(size.x, size.y);

    DrawCircleLinesEx(center, screenMin*0.24f, 1.0f, WithAlpha(accent, 0.10f));
    DrawCircleLinesEx(center, screenMin*0.32f, 1.0f, WithAlpha(accentWarm, 0.08f));

    DrawPolyLinesEx(center, 6, screenMin*0.17f, rotation*RAD2DEG, 2.0f, WithAlpha(accent, 0.35f));
    DrawPolyLinesEx(center, 3, screenMin*0.12f, -rotation*1.6f*RAD2DEG, 2.0f, WithAlpha(accentWarm, 0.35f));

    const float orbitRadius = screenMin*0.24f;
    for (int i = 0; i < 8; i++)
    {
        const float angle = rotation*0.03f + 2.0f*static_cast<float>(std::numbers::pi)*static_cast<float>(i)/8.0f;
        DrawCircleV({ center.x + cosf(angle)*orbitRadius, center.y + sinf(angle)*orbitRadius }, 3.0f, WithAlpha(accent, 0.55f));
    }
}

static void DrawParticles(const std::vector<Particle> &particles)
{
    for (const Particle &particle : particles)
    {
        const float fade = std::clamp(particle.life/particle.maxLife, 0.0f, 1.0f);

        DrawCircleV(particle.position, particle.radius*4.0f, WithAlpha(particle.color, 0.05f*fade));
        DrawCircleV(particle.position, particle.radius*2.2f, WithAlpha(particle.color, 0.12f*fade));
        DrawCircleV(particle.position, particle.radius, WithAlpha(particle.color, 0.95f*fade));
    }
}

static void DrawPanel(Rectangle bounds, float alpha)
{
    DrawRectangleRec(bounds, WithAlpha(panelColor, 0.72f*alpha));
    DrawRectangleLinesEx(bounds, 1.0f, WithAlpha(accent, 0.18f*alpha));
}

static void DrawHud(Font font, vec2 size, int particleCount, float time, float delta)
{
    const float alpha = std::clamp(time*0.8f, 0.0f, 1.0f);

    DrawPanel(Rectangle{ 28.0f, 28.0f, 430.0f, 152.0f }, alpha);

    DrawTextEx(font, "raylib-extended - Partikel-Demo", { 48.0f, 46.0f }, 26.0f, 1.0f, WithAlpha(textPrimary, alpha));
    DrawTextEx(font, "Linke Maustaste: Teilchen anziehen", { 48.0f, 88.0f }, 18.0f, 1.0f, WithAlpha(textSecondary, alpha));
    DrawTextEx(font, "Leertaste: Explosion | R: Zuruecksetzen", { 48.0f, 112.0f }, 18.0f, 1.0f, WithAlpha(textSecondary, alpha));
    DrawTextEx(font, "F11: Vollbild | ESC: Beenden", { 48.0f, 136.0f }, 18.0f, 1.0f, WithAlpha(textSecondary, alpha));

    const std::string counter = TextFormat("%i Partikel", particleCount);
    const vec2 counterSize = MeasureTextEx(font, counter, 20.0f, 1.0f);
    DrawTextEx(font, counter, { size.x - counterSize.x - 40.0f, 40.0f }, 20.0f, 1.0f, WithAlpha(textSecondary, alpha));

    constexpr const char *hint = "Bewege die Maus und halte die linke Taste gedrueckt";
    const vec2 hintSize = MeasureTextEx(font, hint, 18.0f, 1.0f);
    DrawTextEx(font, hint, { size.x*0.5f - hintSize.x*0.5f, size.y - 44.0f }, 18.0f, 1.0f, WithAlpha(textSecondary, 0.75f*alpha));

    const std::string fpsText = TextFormat("%.0f FPS", (delta > 0.0f)? 1.0f/delta : 0.0f);
    const vec2 fpsSize = MeasureTextEx(font, fpsText, 18.0f, 1.0f);
    DrawTextEx(font, fpsText, { size.x - fpsSize.x - 40.0f, size.y - 40.0f }, 18.0f, 1.0f, WithAlpha(textSecondary, alpha));
}

int main()
{
    // MSAA x4 like the original demo, requested before the window context is created
    SetWindowSamples(4);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(screenWidth, screenHeight, "raylib-extended [particle] - 2d particle playground");

    SetWindowMinSize(640, 360);
    SetTargetFPS(120);

    // Load a system font for the HUD, falling back to the raylib default font
    Font font = { 0 };
    bool fontLoaded = false;

    const char *fontPath = FindSystemFont();
    if (fontPath != nullptr)
    {
        font = LoadFontEx(fontPath, 64, nullptr, 0);
        fontLoaded = true;
        TraceLog(LOG_INFO, "FONT: Loaded system font: %s", fontPath);
    }
    else
    {
        font = GetFontDefault();
        TraceLog(LOG_WARNING, "FONT: No system font found, using the raylib default font");
    }

    std::vector<Particle> particles;
    particles.reserve(maxParticles);
    SpawnBurst(particles, { static_cast<float>(screenWidth)*0.5f, static_cast<float>(screenHeight)*0.5f }, 120);

    float time = 0.0f;
    float spawnAccumulator = 0.0f;
    int frames = 0;

    while (!WindowShouldClose())
    {
        // mx::Clock::setMaxDelta(0.05f)
        float delta = GetFrameTime();
        if (delta > 0.05f) delta = 0.05f;

        time += delta;
        frames++;

        const vec2 size = { static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()) };
        const vec2 mouse = GetMousePosition();

        const bool attracting = IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
                          (mouse.x >= 0.0f) && (mouse.y >= 0.0f) &&
                          (mouse.x <= size.x) && (mouse.y <= size.y);

        // While attracting, a stream of particles is emitted at the cursor
        if (attracting)
        {
            spawnAccumulator += delta*500.0f;

            while (spawnAccumulator >= 1.0f)
            {
                const float angle = RandomFloat(0.0f, 2.0f*static_cast<float>(std::numbers::pi));
                SpawnParticle(particles, mouse, { cosf(angle), sinf(angle) }, RandomFloat(20.0f, 140.0f));
                spawnAccumulator -= 1.0f;
            }
        }
        else spawnAccumulator = 0.0f;

        if (IsKeyPressed(KEY_SPACE)) SpawnBurst(particles, mouse, 160);
        if (IsKeyPressed(KEY_R)) particles.clear();
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        UpdateParticles(particles, mouse, attracting, size, delta);

        BeginDrawing();
            ClearBackground(backgroundTop);

            DrawBackground(size, time);
            DrawOrbit({ size.x*0.5f, size.y*0.5f }, size, time);
            DrawParticles(particles);

            // Cursor marker
            DrawCircleV(mouse, 14.0f + 2.0f*sinf(time*5.0f), WithAlpha(accent, 0.10f));
            DrawCircleLinesEx(mouse, 9.0f, 1.0f, WithAlpha(accent, 0.55f));

            DrawHud(font, size, static_cast<int>(particles.size()), time, delta);
        EndDrawing();
    }

    if (fontLoaded) UnloadFont(font);

    TraceLog(LOG_INFO, "Beendet nach %i Frames", frames);

    CloseWindow();

    return 0;
}
