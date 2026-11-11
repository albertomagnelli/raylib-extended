/*******************************************************************************************
*
*   raylib [audio] example - spectrum visualizer
*
*   Example complexity rating: [★★★☆] 3/4
*
*   Example originally created with raylib 6.0, last time updated with raylib 6.0
*
*   Inspired by Inigo Quilez's https://www.shadertoy.com/
*   Resources/specification: https://gist.github.com/soulthreads/2efe50da4be1fb5f7ab60ff14ca434b8
*
*   Example created by created by IANN (@meisei4) reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 IANN (@meisei4)
*
********************************************************************************************/

#include "raylib.h"

#include "raymath.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            100
#endif

#define MONO                           1
#define SAMPLE_RATE                    44100
#define SAMPLE_RATE_F                  44100.0f
#define FFT_WINDOW_SIZE                1024
#define BUFFER_SIZE                    512
#define PER_SAMPLE_BIT_DEPTH           16
#define AUDIO_STREAM_RING_BUFFER_SIZE  (FFT_WINDOW_SIZE*2)
#define EFFECTIVE_SAMPLE_RATE          (SAMPLE_RATE_F*0.5f)
#define WINDOW_TIME                    (static_cast<f64>(FFT_WINDOW_SIZE)/static_cast<f64>(EFFECTIVE_SAMPLE_RATE))
#define FFT_HISTORICAL_SMOOTHING_DUR   2.0f
#define MIN_DECIBELS                   (-100.0f) // https://developer.mozilla.org/en-US/docs/Web/API/AnalyserNode/minDecibels
#define MAX_DECIBELS                   (-30.0f)  // https://developer.mozilla.org/en-US/docs/Web/API/AnalyserNode/maxDecibels
#define INVERSE_DECIBEL_RANGE          (1.0f/(MAX_DECIBELS - MIN_DECIBELS))
#define DB_TO_LINEAR_SCALE             (20.0f/2.302585092994046f)
#define SMOOTHING_TIME_CONSTANT        0.8f // https://developer.mozilla.org/en-US/docs/Web/API/AnalyserNode/smoothingTimeConstant
#define TEXTURE_HEIGHT                 1
#define FFT_ROW                        0
#define UNUSED_CHANNEL                 0.0f

typedef struct FFTComplex { f32 real, imaginary; } FFTComplex;

typedef struct FFTData {
    FFTComplex *spectrum;
    FFTComplex *workBuffer;
    f32 *prevMagnitudes;
    f32 (*fftHistory)[BUFFER_SIZE];
    i32 fftHistoryLen;
    i32 historyPos;
    f64 lastFftTime;
    f32 tapbackPos;
} FFTData;

static void CaptureFrame(FFTData *fftData, const f32 *audioSamples);
static void RenderFrame(const FFTData *fftData, Image *fftImage);
static void CooleyTukeyFFTSlow(FFTComplex *spectrum, i32 n);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //-----------------------------------------------------------------------------------     ---
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [audio] example - spectrum visualizer");

    Image fftImage = GenImageColor(BUFFER_SIZE, TEXTURE_HEIGHT, WHITE);
    const Texture2D fftTexture = LoadTextureFromImage(fftImage);
    const RenderTexture2D bufferA = LoadRenderTexture(screenWidth, screenHeight);
    vec2 iResolution = { static_cast<f32>(screenWidth), static_cast<f32>(screenHeight) };

    const Shader shader = LoadShader({}, TextFormat("resources/shaders/glsl%i/fft.fs", GLSL_VERSION));

    const i32 iResolutionLocation = GetShaderLocation(shader, "iResolution");
    const i32 iChannel0Location = GetShaderLocation(shader, "iChannel0");
    SetShaderValue(shader, iResolutionLocation, &iResolution, SHADER_UNIFORM_VEC2);
    SetShaderValueTexture(shader, iChannel0Location, fftTexture);

    InitAudioDevice();
    SetAudioStreamBufferSizeDefault(AUDIO_STREAM_RING_BUFFER_SIZE);

    // WARNING: Memory out-of-bounds on PLATFORM_WEB
    Wave wav = LoadWave("resources/country.mp3");
    WaveFormat(&wav, SAMPLE_RATE, PER_SAMPLE_BIT_DEPTH, MONO);

    const AudioStream audioStream = LoadAudioStream(SAMPLE_RATE, PER_SAMPLE_BIT_DEPTH, MONO);
    PlayAudioStream(audioStream);

    const i32 fftHistoryLen = static_cast<i32>(ceilf(FFT_HISTORICAL_SMOOTHING_DUR/WINDOW_TIME)) + 1;

    FFTData fft = {
        .spectrum = static_cast<FFTComplex *>(calloc(sizeof(FFTComplex), FFT_WINDOW_SIZE)),
        .workBuffer = static_cast<FFTComplex *>(calloc(sizeof(FFTComplex), FFT_WINDOW_SIZE)),
        .prevMagnitudes = static_cast<f32 *>(calloc(BUFFER_SIZE, sizeof(f32))),
        .fftHistory = static_cast<f32 (*)[BUFFER_SIZE]>(calloc(fftHistoryLen, sizeof(f32[BUFFER_SIZE]))),
        .fftHistoryLen = fftHistoryLen,
        .historyPos = 0,
        .lastFftTime = 0.0,
        .tapbackPos = 0.01f
    };

    u32 wavCursor = 0;
    const short *wavPCM16 = static_cast<const short *>(wav.data);

    short chunkSamples[AUDIO_STREAM_RING_BUFFER_SIZE] = {};
    f32 audioSamples[FFT_WINDOW_SIZE] = {};

    SetTargetFPS(60);
    //----------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        while (IsAudioStreamProcessed(audioStream))
        {
            for (i32 i = 0; i < AUDIO_STREAM_RING_BUFFER_SIZE; i++)
            {
                const i32 left = (wav.channels == 2)? wavPCM16[wavCursor*2 + 0] : wavPCM16[wavCursor];
                const i32 right = (wav.channels == 2)? wavPCM16[wavCursor*2 + 1] : left;
                chunkSamples[i] = static_cast<short>((left + right)/2);

                if (++wavCursor >= wav.frameCount) wavCursor = 0;
            }

            UpdateAudioStream(audioStream, chunkSamples, AUDIO_STREAM_RING_BUFFER_SIZE);

            for (i32 i = 0; i < FFT_WINDOW_SIZE; i++) audioSamples[i] = (chunkSamples[i*2] + chunkSamples[i*2 + 1])*0.5f/32767.0f;
        }

        CaptureFrame(&fft, audioSamples);
        RenderFrame(&fft, &fftImage);
        UpdateTexture(fftTexture, fftImage.data);
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            BeginShaderMode(shader);
                SetShaderValueTexture(shader, iChannel0Location, fftTexture);
                DrawTextureRec(bufferA.texture,
                    Rectangle{ 0, 0, static_cast<f32>(screenWidth), static_cast<f32>(-screenHeight) },
                    vec2{ 0, 0 }, WHITE);
            EndShaderMode();

        EndDrawing();
        //------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadShader(shader);
    UnloadRenderTexture(bufferA);
    UnloadTexture(fftTexture);
    UnloadImage(fftImage);
    UnloadAudioStream(audioStream);
    UnloadWave(wav);
    CloseAudioDevice();

    free(fft.spectrum);
    free(fft.workBuffer);
    free(fft.prevMagnitudes);
    free(fft.fftHistory);

    CloseWindow();        // Close window and OpenGL context
    //----------------------------------------------------------------------------------

    return 0;
}

// Cooley–Tukey FFT https://en.wikipedia.org/wiki/Cooley%E2%80%93Tukey_FFT_algorithm#Data_reordering,_bit_reversal,_and_in-place_algorithms
static void CooleyTukeyFFTSlow(FFTComplex *spectrum, i32 n)
{
    i32 j = 0;
    for (i32 i = 1; i < n - 1; i++)
    {
        i32 bit = n >> 1;
        while (j >= bit)
        {
            j -= bit;
            bit >>= 1;
        }
        j += bit;
        if (i < j)
        {
            const FFTComplex temp = spectrum[i];
            spectrum[i] = spectrum[j];
            spectrum[j] = temp;
        }
    }

    for (i32 len = 2; len <= n; len <<= 1)
    {
        const f32 angle = -2.0f*PI/len;
        const FFTComplex twiddleUnit = { cosf(angle), sinf(angle) };
        for (i32 i = 0; i < n; i += len)
        {
            FFTComplex twiddleCurrent = { 1.0f, 0.0f };
            for (i32 j = 0; j < len/2; j++)
            {
                const FFTComplex even = spectrum[i + j];
                const FFTComplex odd = spectrum[i + j + len/2];
                const FFTComplex twiddledOdd = {
                    odd.real*twiddleCurrent.real - odd.imaginary*twiddleCurrent.imaginary,
                    odd.real*twiddleCurrent.imaginary + odd.imaginary*twiddleCurrent.real
                };

                spectrum[i + j].real = even.real + twiddledOdd.real;
                spectrum[i + j].imaginary = even.imaginary + twiddledOdd.imaginary;
                spectrum[i + j + len/2].real = even.real - twiddledOdd.real;
                spectrum[i + j + len/2].imaginary = even.imaginary - twiddledOdd.imaginary;

                const f32 twiddleRealNext = twiddleCurrent.real*twiddleUnit.real - twiddleCurrent.imaginary*twiddleUnit.imaginary;
                twiddleCurrent.imaginary = twiddleCurrent.real*twiddleUnit.imaginary + twiddleCurrent.imaginary*twiddleUnit.real;
                twiddleCurrent.real = twiddleRealNext;
            }
        }
    }
}

static void CaptureFrame(FFTData *fftData, const f32 *audioSamples)
{
    for (i32 i = 0; i < FFT_WINDOW_SIZE; i++)
    {
        const f32 x = (2.0f*PI*i)/(FFT_WINDOW_SIZE - 1.0f);
        const f32 blackmanWeight  = 0.42f - 0.5f*cosf(x) + 0.08f*cosf(2.0f*x); // https://en.wikipedia.org/wiki/Window_function#Blackman_window
        fftData->workBuffer[i].real = audioSamples[i]*blackmanWeight;
        fftData->workBuffer[i].imaginary = 0.0f;
    }

    CooleyTukeyFFTSlow(fftData->workBuffer, FFT_WINDOW_SIZE);
    memcpy(fftData->spectrum, fftData->workBuffer, sizeof(FFTComplex)*FFT_WINDOW_SIZE);

    f32 smoothedSpectrum[BUFFER_SIZE];

    for (i32 bin = 0; bin < BUFFER_SIZE; bin++)
    {
        const f32 re = fftData->workBuffer[bin].real;
        const f32 im = fftData->workBuffer[bin].imaginary;
        const f32 linearMagnitude = sqrtf(re*re + im*im)/FFT_WINDOW_SIZE;

        const f32 smoothedMagnitude = SMOOTHING_TIME_CONSTANT*fftData->prevMagnitudes[bin] + (1.0f - SMOOTHING_TIME_CONSTANT)*linearMagnitude;
        fftData->prevMagnitudes[bin] = smoothedMagnitude;

        const f32 db = logf(fmaxf(smoothedMagnitude, 1e-40f))*DB_TO_LINEAR_SCALE;
        const f32 normalized = (db - MIN_DECIBELS)*INVERSE_DECIBEL_RANGE;
        smoothedSpectrum[bin] = Clamp(normalized, 0.0f, 1.0f);
    }

    fftData->lastFftTime = GetTime();
    memcpy(fftData->fftHistory[fftData->historyPos], smoothedSpectrum, sizeof(smoothedSpectrum));
    fftData->historyPos = (fftData->historyPos + 1)%fftData->fftHistoryLen;
}

static void RenderFrame(const FFTData *fftData, Image *fftImage)
{
    f32 framesSinceTapback = floorf(static_cast<f32>(fftData->tapbackPos/WINDOW_TIME));
    framesSinceTapback = Clamp(framesSinceTapback, 0.0f, static_cast<f32>(fftData->fftHistoryLen - 1));

    i32 historyPosition = (fftData->historyPos - 1 - static_cast<i32>(framesSinceTapback))%fftData->fftHistoryLen;
    if (historyPosition < 0) historyPosition += fftData->fftHistoryLen;

    const f32 *amplitude = fftData->fftHistory[historyPosition];
    for (i32 bin = 0; bin < BUFFER_SIZE; bin++) ImageDrawPixel(fftImage, bin, FFT_ROW, ColorFromNormalized(vec4{ amplitude[bin], UNUSED_CHANNEL, UNUSED_CHANNEL, UNUSED_CHANNEL }));
}