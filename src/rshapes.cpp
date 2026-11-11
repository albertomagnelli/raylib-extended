/**********************************************************************************************
*
*   rshapes - Basic functions to draw 2d shapes and check collisions
*
*   ADDITIONAL NOTES:
*       Shapes can be draw using 3 types of primitives: LINES, TRIANGLES and QUADS
*       Some functions implement two drawing options: TRIANGLES and QUADS, by default QUADS
*       are used but TRIANGLES can be selected with by turning SUPPORT_QUADS_DRAW_MODE to 0
*
*       Some functions define texture coordinates (rlTexCoord2f()) for the shapes and use a
*       user-provided texture with SetShapesTexture(), the purpose of this implementation
*       is allowing to reduce draw calls when combined with a texture-atlas
*
*       By default, raylib sets the default texture and rectangle at InitWindow()[rcore] to one
*       white character of default font [rtext], this way, raylib text and shapes can be drawn with
*       a single draw call and it also allows users to configure it the same way with their own fonts
*
*   CONFIGURATION:
*       #define SUPPORT_MODULE_RSHAPES      1
*           rshapes module is included in the build
*
*       #define SUPPORT_QUADS_DRAW_MODE     1
*           Use QUADS instead of TRIANGLES for drawing when possible. Lines-based shapes still use LINES
*
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2013-2026 Ramon Santamaria (@raysan5)
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

#include "raylib.h"     // Declares module functions

#include "config.h"     // Defines module configuration flags

#if SUPPORT_MODULE_RSHAPES

#include "rlgl.h"       // OpenGL abstraction layer to OpenGL 1.1, 2.1, 3.3+ or ES2

#include <math.h>       // Required for: sinf(), asinf(), cosf(), acosf(), sqrtf(), fabsf()
#include <float.h>      // Required for: FLT_EPSILON
#include <stdlib.h>     // Required for: free

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
#ifndef SMOOTH_CIRCLE_ERROR_RATE
    // Define error rate to calculate how many segments are needed to draw a smooth circle
    // REF: https://stackoverflow.com/a/2244088
    #define SMOOTH_CIRCLE_ERROR_RATE    0.5f      // Circle error rate
#endif
#ifndef SPLINE_SEGMENT_DIVISIONS
    #define SPLINE_SEGMENT_DIVISIONS      24      // Spline segment divisions
#endif

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
// Not here...

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
static auto texShapes = Texture2D{ 1, 1, 1, 1, 7 };                // Texture used on shapes drawing (white pixel loaded by rlgl)
static auto texShapesRec = Rectangle{ 0.0f, 0.0f, 1.0f, 1.0f };    // Texture source rectangle used on shapes drawing

//----------------------------------------------------------------------------------
// Module Internal Functions Declaration
//----------------------------------------------------------------------------------
static f32 EaseCubicInOut(f32 t, const f32 b, const f32 c, const f32 d);    // Cubic easing

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
// Set texture and rectangle to be used on shapes drawing
// NOTE: It can be useful when using basic shapes and one single font,
// defining a font char white rectangle would allow drawing everything in a single draw call
void SetShapesTexture(const Texture2D texture, const Rectangle rec)
{
    // Reset texture to default pixel if required
    // WARNING: Shapes texture should be probably better validated,
    // it can break the rendering of all shapes if misused
    if ((texture.id == 0) || (rec.width == 0) || (rec.height == 0))
    {
        texShapes = Texture2D{ 1, 1, 1, 1, 7 };
        texShapesRec = Rectangle{ 0.0f, 0.0f, 1.0f, 1.0f };
    }
    else
    {
        texShapes = texture;
        texShapesRec = rec;
    }
}

// Get texture that is used for shapes drawing
Texture2D GetShapesTexture()
{
    return texShapes;
}

// Get texture source rectangle that is used for shapes drawing
Rectangle GetShapesTextureRectangle()
{
    return texShapesRec;
}

// Draw a pixel
void DrawPixel(const i32 posX, const i32 posY, const Color color)
{
    DrawPixelV(vec2{ static_cast<f32>(posX), static_cast<f32>(posY) }, color);
}

// Draw a pixel (Vector version)
void DrawPixelV(const vec2 position, const Color color)
{
#if SUPPORT_QUADS_DRAW_MODE
    rlSetTexture(GetShapesTexture().id);
    const auto shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);

        rlNormal3f(0.0f, 0.0f, 1.0f);
        rlColor4ub(color.r, color.g, color.b, color.a);

        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(position.x, position.y);

        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(position.x, position.y + 1);

        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(position.x + 1, position.y + 1);

        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(position.x + 1, position.y);

    rlEnd();

    rlSetTexture(0);
#else
    rlBegin(RL_TRIANGLES);

        rlColor4ub(color.r, color.g, color.b, color.a);

        rlVertex2f(position.x, position.y);
        rlVertex2f(position.x, position.y + 1);
        rlVertex2f(position.x + 1, position.y);

        rlVertex2f(position.x + 1, position.y);
        rlVertex2f(position.x, position.y + 1);
        rlVertex2f(position.x + 1, position.y + 1);

    rlEnd();
#endif
}

// Draw a line (using gl lines)
void DrawLine(const i32 startPosX, const i32 startPosY, const i32 endPosX, const i32 endPosY, const Color color)
{
    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(static_cast<f32>(startPosX), static_cast<f32>(startPosY));
        rlVertex2f(static_cast<f32>(endPosX), static_cast<f32>(endPosY));
    rlEnd();
}

// Draw a line defining thickness
void DrawLineEx(const vec2 startPos, const vec2 endPos, const f32 thick, const Color color)
{
    const auto delta = vec2{ endPos.x - startPos.x, endPos.y - startPos.y };
    const f32 length = sqrtf(delta.x*delta.x + delta.y*delta.y);

    if ((length > 0) && (thick > 0))
    {
        const f32 scale = thick/(2*length);

        const auto radius = vec2{ -scale*delta.y, scale*delta.x };
        vec2 strip[4] = {
            { startPos.x - radius.x, startPos.y - radius.y },
            { startPos.x + radius.x, startPos.y + radius.y },
            { endPos.x - radius.x, endPos.y - radius.y },
            { endPos.x + radius.x, endPos.y + radius.y }
        };

        DrawTriangleStrip(strip, 4, color);
    }
}

// Draw a line (using gl lines)
void DrawLineV(const vec2 startPos, const vec2 endPos, const Color color)
{
    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(startPos.x, startPos.y);
        rlVertex2f(endPos.x, endPos.y);
    rlEnd();
}

// Draw lines sequence (using gl lines)
void DrawLineStrip(const vec2 *points, const i32 pointCount, const Color color)
{
    if (pointCount < 2) return; // Security check

    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        for (i32 i = 0; i < pointCount - 1; i++)
        {
            rlVertex2f(points[i].x, points[i].y);
            rlVertex2f(points[i + 1].x, points[i + 1].y);
        }
    rlEnd();
}

// Draw line using cubic-bezier spline, in-out interpolation, no control points
void DrawLineBezier(const vec2 startPos, const vec2 endPos, const f32 thick, const Color color)
{
    vec2 previous = startPos;
    vec2 current{};

    vec2 points[2*SPLINE_SEGMENT_DIVISIONS + 2]{};

    for (i32 i = 1; i <= SPLINE_SEGMENT_DIVISIONS; i++)
    {
        // Cubic easing in-out
        // NOTE: Easing is calculated only for y position value
        current.y = EaseCubicInOut(static_cast<f32>(i), startPos.y, endPos.y - startPos.y, static_cast<f32>(SPLINE_SEGMENT_DIVISIONS));
        current.x = previous.x + (endPos.x - startPos.x)/static_cast<f32>(SPLINE_SEGMENT_DIVISIONS);

        const f32 dy = current.y - previous.y;
        const f32 dx = current.x - previous.x;
        const f32 size = 0.5f*thick/sqrtf(dx*dx+dy*dy);

        if (i == 1)
        {
            points[0].x = previous.x + dy*size;
            points[0].y = previous.y - dx*size;
            points[1].x = previous.x - dy*size;
            points[1].y = previous.y + dx*size;
        }

        points[2*i + 1].x = current.x - dy*size;
        points[2*i + 1].y = current.y + dx*size;
        points[2*i].x = current.x + dy*size;
        points[2*i].y = current.y - dx*size;

        previous = current;
    }

    DrawTriangleStrip(points, 2*SPLINE_SEGMENT_DIVISIONS + 2, color);
}

// Draw a dashed line
void DrawLineDashed(const vec2 startPos, const vec2 endPos, const i32 dashSize, const i32 spaceSize, const Color color)
{
    // Calculate the vector and length of the line
    const f32 dx = endPos.x - startPos.x;
    const f32 dy = endPos.y - startPos.y;
    const f32 lineLength = sqrtf(dx*dx + dy*dy);

    // If the line is too short for dashing or dash size is invalid, draw a solid thick line
    if ((lineLength < (dashSize + spaceSize)) || (dashSize <= 0))
    {
        DrawLineV(startPos, endPos, color);
        return;
    }

    // Calculate the normalized direction vector of the line
    const f32 invLineLength = 1.0f/lineLength;
    const f32 dirX = dx*invLineLength;
    const f32 dirY = dy*invLineLength;

    vec2 currentPos = startPos;
    f32 distanceTraveled = 0;

    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        while (distanceTraveled < lineLength)
        {
            // Calculate the end of the current dash
            f32 dashEndDist = distanceTraveled + dashSize;
            if (dashEndDist > lineLength) dashEndDist = lineLength;

            const auto dashEndPos = vec2{ startPos.x + dashEndDist*dirX, startPos.y + dashEndDist*dirY };

            // Draw the dash segment
            rlVertex2f(currentPos.x, currentPos.y);
            rlVertex2f(dashEndPos.x, dashEndPos.y);

            // Update the distance traveled and move the current position for the next dash
            distanceTraveled = dashEndDist + spaceSize;
            currentPos.x = startPos.x + distanceTraveled*dirX;
            currentPos.y = startPos.y + distanceTraveled*dirY;
        }
    rlEnd();
}

// Draw a triangle
// NOTE: Vertex must be provided in counter-clockwise order
void DrawTriangle(const vec2 v1, const vec2 v2, const vec2 v3, const Color color)
{
    DrawTriangleGradient(v1, v2, v3, color, color, color);
}

// Draw triangle with interpolated colors (vertex in counter-clockwise order!)
void DrawTriangleGradient(const vec2 v1, const vec2 v2, const vec2 v3, const Color c1, const Color c2, const Color c3)
{
#if SUPPORT_QUADS_DRAW_MODE
    rlSetTexture(GetShapesTexture().id);
    const auto shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);
        rlNormal3f(0.0f, 0.0f, 1.0f);

        rlColor4ub(c1.r, c1.g, c1.b, c1.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(v1.x, v1.y);

        rlColor4ub(c2.r, c2.g, c2.b, c2.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(v2.x, v2.y);

        rlColor4ub(c3.r, c3.g, c3.b, c3.a);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(v3.x, v3.y);

        rlColor4ub(c3.r, c3.g, c3.b, c3.a);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(v3.x, v3.y);
    rlEnd();

    rlSetTexture(0);
#else
    rlBegin(RL_TRIANGLES);
        rlColor4ub(c1.r, c1.g, c1.b, c1.a);
        rlVertex2f(v1.x, v1.y);
        rlColor4ub(c2.r, c2.g, c2.b, c2.a);
        rlVertex2f(v2.x, v2.y);
        rlColor4ub(c3.r, c3.g, c3.b, c3.a);
        rlVertex2f(v3.x, v3.y);
    rlEnd();
#endif
}

// Draw a triangle using lines
// NOTE: Vertex must be provided in counter-clockwise order
void DrawTriangleLines(const vec2 v1, const vec2 v2, const vec2 v3, const Color color)
{
    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(v1.x, v1.y);
        rlVertex2f(v2.x, v2.y);

        rlVertex2f(v2.x, v2.y);
        rlVertex2f(v3.x, v3.y);

        rlVertex2f(v3.x, v3.y);
        rlVertex2f(v1.x, v1.y);
    rlEnd();
}

// Draw a triangle using lines with thickness
// NOTE: Vertex must be provided in counter-clockwise order
void DrawTriangleLinesEx(vec2 v1, vec2 v2, vec2 v3, const f32 thick, const Color color)
{
    /*
    A sketch to make things simpler

    The exterior points are v1-3, the interior points are v4-6, and the exterior edges are e1-3

             v1
             /\
            /v4\
           //  \\
          //    \\
      e3 //      \\ e2
        //        \\
       //          \\
      //v5        v6\\
     v2==============v3
             e1
    */

    const auto e1 = vec2{ v2.x - v3.x, v2.y - v3.y };
    const auto e2 = vec2{ v3.x - v1.x, v3.y - v1.y };
    const auto e3 = vec2{ v1.x - v2.x, v1.y - v2.y };

    const f32 e1Length = sqrtf(e1.x*e1.x + e1.y*e1.y);
    const f32 e2Length = sqrtf(e2.x*e2.x + e2.y*e2.y);
    const f32 e3Length = sqrtf(e3.x*e3.x + e3.y*e3.y);

    const f32 perimeter = e1Length + e2Length + e3Length;
    const f32 semiperimeter = perimeter/2.0f;

    // The incenter of a triangle is equidistant from each edge, which is useful for drawing a nice looking outline
    const vec2 incenter = {
        (e1Length*v1.x + e2Length*v2.x + e3Length*v3.x)/perimeter,
        (e1Length*v1.y + e2Length*v2.y + e3Length*v3.y)/perimeter
    };

    // The inradius of a triangle is the radius of the biggest circle that can fit inside of said triangle
    // That circle is also centered on the incenter
    const f32 inradius = sqrtf(((semiperimeter - e1Length)*(semiperimeter - e2Length)*(semiperimeter - e3Length))/semiperimeter);

    // The triangle (v1, v2, v3) will be scaled by this to get (v4, v5, v6)
    const f32 scale = 1.0f - thick/inradius;

    // Just a filled-in triangle
    if (scale <= 0.0f)
    {
        DrawTriangle(v1, v2, v3, color);
        return;
    }

    // In order for the scaling to be correct, the incenter has to be at the origin (0, 0) when scaling
    auto v4 = vec2{ incenter.x + (v1.x - incenter.x)*scale, incenter.y + (v1.y - incenter.y)*scale };
    auto v5 = vec2{ incenter.x + (v2.x - incenter.x)*scale, incenter.y + (v2.y - incenter.y)*scale };
    auto v6 = vec2{ incenter.x + (v3.x - incenter.x)*scale, incenter.y + (v3.y - incenter.y)*scale };

    // Swap the vertices so the winding order is correct
    if (thick < 0.0f)
    {
        vec2 temp = v1;
        v1 = v4;
        v4 = temp;

        temp = v2;
        v2 = v5;
        v5 = temp;

        temp = v3;
        v3 = v6;
        v6 = temp;
    }

#if SUPPORT_QUADS_DRAW_MODE
    rlSetTexture(GetShapesTexture().id);
    const auto shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);

        rlColor4ub(color.r, color.g, color.b, color.a);

        // Edge 3
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(v1.x, v1.y);

        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(v2.x, v2.y);

        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(v5.x, v5.y);

        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(v4.x, v4.y);

        // Edge 1
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(v2.x, v2.y);

        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(v3.x, v3.y);

        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(v6.x, v6.y);

        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(v5.x, v5.y);

        // Edge 2
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(v3.x, v3.y);

        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(v1.x, v1.y);

        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(v4.x, v4.y);

        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(v6.x, v6.y);

    rlEnd();

    rlSetTexture(0);
#else
    rlBegin(RL_TRIANGLES);

        rlColor4ub(color.r, color.g, color.b, color.a);

        // Edge 3
        rlVertex2f(v1.x, v1.y);
        rlVertex2f(v2.x, v2.y);
        rlVertex2f(v4.x, v4.y);

        rlVertex2f(v2.x, v2.y);
        rlVertex2f(v5.x, v5.y);
        rlVertex2f(v4.x, v4.y);

        // Edge 1
        rlVertex2f(v2.x, v2.y);
        rlVertex2f(v3.x, v3.y);
        rlVertex2f(v5.x, v5.y);

        rlVertex2f(v3.x, v3.y);
        rlVertex2f(v6.x, v6.y);
        rlVertex2f(v5.x, v5.y);

        // Edge 2
        rlVertex2f(v3.x, v3.y);
        rlVertex2f(v1.x, v1.y);
        rlVertex2f(v4.x, v4.y);

        rlVertex2f(v3.x, v3.y);
        rlVertex2f(v4.x, v4.y);
        rlVertex2f(v6.x, v6.y);

    rlEnd();
#endif
}

// Draw a triangle fan defined by points
// NOTE: First vertex provided is the center, shared by all triangles
// By default, following vertex should be provided in counter-clockwise order
void DrawTriangleFan(const vec2 *points, const i32 pointCount, const Color color)
{
    if (pointCount >= 3)
    {
        rlSetTexture(GetShapesTexture().id);
        const auto shapeRect = GetShapesTextureRectangle();

        rlBegin(RL_QUADS);
            rlColor4ub(color.r, color.g, color.b, color.a);

            for (i32 i = 1; i < pointCount - 1; i++)
            {
                rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                rlVertex2f(points[0].x, points[0].y);

                rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                rlVertex2f(points[i].x, points[i].y);

                rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                rlVertex2f(points[i + 1].x, points[i + 1].y);

                rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                rlVertex2f(points[i + 1].x, points[i + 1].y);
            }
        rlEnd();
        rlSetTexture(0);
    }
}

// Draw a triangle strip defined by points
// NOTE: Every new vertex connects with previous two
void DrawTriangleStrip(const vec2 *points, const i32 pointCount, const Color color)
{
    if (pointCount >= 3)
    {
        rlBegin(RL_TRIANGLES);
            rlColor4ub(color.r, color.g, color.b, color.a);

            for (i32 i = 2; i < pointCount; i++)
            {
                if ((i%2) == 0)
                {
                    rlVertex2f(points[i].x, points[i].y);
                    rlVertex2f(points[i - 2].x, points[i - 2].y);
                    rlVertex2f(points[i - 1].x, points[i - 1].y);
                }
                else
                {
                    rlVertex2f(points[i].x, points[i].y);
                    rlVertex2f(points[i - 1].x, points[i - 1].y);
                    rlVertex2f(points[i - 2].x, points[i - 2].y);
                }
            }
        rlEnd();
    }
}

// Draw a color-filled rectangle
void DrawRectangle(const i32 posX, const i32 posY, const i32 width, const i32 height, const Color color)
{
    DrawRectangleV(vec2{ static_cast<f32>(posX), static_cast<f32>(posY) }, vec2{ static_cast<f32>(width), static_cast<f32>(height) }, color);
}

// Draw a color-filled rectangle (Vector version)
// NOTE: On OpenGL 3.3 and ES2 using QUADS to avoid drawing order issues
void DrawRectangleV(const vec2 position, const vec2 size, const Color color)
{
    DrawRectanglePro(Rectangle{ position.x, position.y, size.x, size.y }, vec2{ 0.0f, 0.0f }, 0.0f, color);
}

// Draw a color-filled rectangle
void DrawRectangleRec(const Rectangle rec, const Color color)
{
    DrawRectanglePro(rec, vec2{ 0.0f, 0.0f }, 0.0f, color);
}

// Draw a color-filled rectangle with pro parameters
void DrawRectanglePro(const Rectangle rec, const vec2 origin, const f32 rotation, const Color color)
{
    vec2 topLeft{};
    vec2 topRight{};
    vec2 bottomLeft{};
    vec2 bottomRight{};

    // Only calculate rotation if needed
    if (rotation == 0.0f)
    {
        const f32 x = rec.x - origin.x;
        const f32 y = rec.y - origin.y;
        topLeft = vec2{ x, y };
        topRight = vec2{ x + rec.width, y };
        bottomLeft = vec2{ x, y + rec.height };
        bottomRight = vec2{ x + rec.width, y + rec.height };
    }
    else
    {
        const f32 sinRotation = sinf(rotation*DEG2RAD);
        const f32 cosRotation = cosf(rotation*DEG2RAD);
        const f32 x = rec.x;
        const f32 y = rec.y;
        const f32 dx = -origin.x;
        const f32 dy = -origin.y;

        topLeft.x = x + dx*cosRotation - dy*sinRotation;
        topLeft.y = y + dx*sinRotation + dy*cosRotation;

        topRight.x = x + (dx + rec.width)*cosRotation - dy*sinRotation;
        topRight.y = y + (dx + rec.width)*sinRotation + dy*cosRotation;

        bottomLeft.x = x + dx*cosRotation - (dy + rec.height)*sinRotation;
        bottomLeft.y = y + dx*sinRotation + (dy + rec.height)*cosRotation;

        bottomRight.x = x + (dx + rec.width)*cosRotation - (dy + rec.height)*sinRotation;
        bottomRight.y = y + (dx + rec.width)*sinRotation + (dy + rec.height)*cosRotation;
    }

#if SUPPORT_QUADS_DRAW_MODE
    rlSetTexture(GetShapesTexture().id);
    const auto shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);

        rlNormal3f(0.0f, 0.0f, 1.0f);
        rlColor4ub(color.r, color.g, color.b, color.a);

        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(topLeft.x, topLeft.y);

        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(bottomLeft.x, bottomLeft.y);

        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(bottomRight.x, bottomRight.y);

        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(topRight.x, topRight.y);

    rlEnd();

    rlSetTexture(0);
#else
    rlBegin(RL_TRIANGLES);

        rlColor4ub(color.r, color.g, color.b, color.a);

        rlVertex2f(topLeft.x, topLeft.y);
        rlVertex2f(bottomLeft.x, bottomLeft.y);
        rlVertex2f(topRight.x, topRight.y);

        rlVertex2f(topRight.x, topRight.y);
        rlVertex2f(bottomLeft.x, bottomLeft.y);
        rlVertex2f(bottomRight.x, bottomRight.y);

    rlEnd();
#endif
}

// Draw a vertical-gradient-filled rectangle
void DrawRectangleGradientV(const i32 posX, const i32 posY, const i32 width, const i32 height, const Color top, const Color bottom)
{
    DrawRectangleGradientEx(Rectangle{ static_cast<f32>(posX), static_cast<f32>(posY), static_cast<f32>(width), static_cast<f32>(height) }, top, bottom, bottom, top);
}

// Draw a horizontal-gradient-filled rectangle
void DrawRectangleGradientH(const i32 posX, const i32 posY, const i32 width, const i32 height, const Color left, const Color right)
{
    DrawRectangleGradientEx(Rectangle{ static_cast<f32>(posX), static_cast<f32>(posY), static_cast<f32>(width), static_cast<f32>(height) }, left, left, right, right);
}

// Draw a gradient-filled rectangle with custom vertex colors, counter-clockwise color order
void DrawRectangleGradientEx(const Rectangle rec, const Color col1, const Color col2, const Color col3, const Color col4)
{
    rlSetTexture(GetShapesTexture().id);
    const auto shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);
        rlNormal3f(0.0f, 0.0f, 1.0f);

        // NOTE: Default raylib font character 95 is a white square
        rlColor4ub(col1.r, col1.g, col1.b, col1.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(rec.x, rec.y);

        rlColor4ub(col2.r, col2.g, col2.b, col2.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(rec.x, rec.y + rec.height);

        rlColor4ub(col3.r, col3.g, col3.b, col3.a);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(rec.x + rec.width, rec.y + rec.height);

        rlColor4ub(col4.r, col4.g, col4.b, col4.a);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(rec.x + rec.width, rec.y);
    rlEnd();

    rlSetTexture(0);
}

// Draw rectangle outline
// WARNING: All Draw*Lines() functions use RL_LINES for drawing,
// it implies flushing the current batch and changing draw mode to RL_LINES
// but it solves another issue: https://github.com/raysan5/raylib/issues/3884
void DrawRectangleLines(const i32 posX, const i32 posY, const i32 width, const i32 height, const Color color)
{
    auto mat = rlGetMatrixTransform();
    const f32 xOffset = 0.5f/mat[0][0];
    const f32 yOffset = 0.5f/mat[1][1];

    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(static_cast<f32>(posX) + xOffset, static_cast<f32>(posY) + yOffset);
        rlVertex2f(static_cast<f32>(posX) + static_cast<f32>(width) - xOffset, static_cast<f32>(posY) + yOffset);

        rlVertex2f(static_cast<f32>(posX) + static_cast<f32>(width) - xOffset, static_cast<f32>(posY) + yOffset);
        rlVertex2f(static_cast<f32>(posX) + static_cast<f32>(width) - xOffset, static_cast<f32>(posY) + static_cast<f32>(height) - yOffset);

        rlVertex2f(static_cast<f32>(posX) + static_cast<f32>(width) - xOffset, static_cast<f32>(posY) + static_cast<f32>(height) - yOffset);
        rlVertex2f(static_cast<f32>(posX) + xOffset, static_cast<f32>(posY) + static_cast<f32>(height) - yOffset);

        rlVertex2f(static_cast<f32>(posX) + xOffset, static_cast<f32>(posY) + static_cast<f32>(height) - yOffset);
        rlVertex2f(static_cast<f32>(posX) + xOffset, static_cast<f32>(posY) + yOffset);
    rlEnd();

/*
// Previous implementation, it has issues... but it does not require view matrix...
#if SUPPORT_QUADS_DRAW_MODE
    DrawRectangle(posX, posY, width, 1, color);
    DrawRectangle(posX + width - 1, posY + 1, 1, height - 2, color);
    DrawRectangle(posX, posY + height - 1, width, 1, color);
    DrawRectangle(posX, posY + 1, 1, height - 2, color);
#else
    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f((float)posX, (float)posY);
        rlVertex2f((float)posX + (float)width, (float)posY + 1);

        rlVertex2f((float)posX + (float)width, (float)posY + 1);
        rlVertex2f((float)posX + (float)width, (float)posY + (float)height);

        rlVertex2f((float)posX + (float)width, (float)posY + (float)height);
        rlVertex2f((float)posX + 1, (float)posY + (float)height);

        rlVertex2f((float)posX + 1, (float)posY + (float)height);
        rlVertex2f((float)posX + 1, (float)posY + 1);
    rlEnd();
#endif
*/
}

// Draw rectangle outline with line thickness
void DrawRectangleLinesEx(const Rectangle rec, f32 thick, const Color color)
{
    if ((thick > rec.width/2) || (thick > rec.height/2))
    {
        if (rec.width >= rec.height) thick = rec.height/2;
        else if (rec.width <= rec.height) thick = rec.width/2;
    }

    if (thick > 0.0f)
    {
        // When rec = { x, y, 8.0f, 6.0f } and thick = 2, the following
        // four rectangles are drawn ([T]op, [B]ottom, [L]eft, [R]ight):
        //
        //   TTTTTTTT
        //   TTTTTTTT
        //   LL    RR
        //   LL    RR
        //   BBBBBBBB
        //   BBBBBBBB
        //

        const auto top = Rectangle{ rec.x, rec.y, rec.width, thick };
        const auto bottom = Rectangle{ rec.x, rec.y - thick + rec.height, rec.width, thick };
        const auto left = Rectangle{ rec.x, rec.y + thick, thick, rec.height - thick*2.0f };
        const auto right = Rectangle{ rec.x - thick + rec.width, rec.y + thick, thick, rec.height - thick*2.0f };

        DrawRectangleRec(top, color);
        DrawRectangleRec(bottom, color);
        DrawRectangleRec(left, color);
        DrawRectangleRec(right, color);
    }
    else
    {
        // When rec = { x, y, 8.0f, 6.0f } and thick = -2, the following
        // four rectangles are drawn ([T]op, [B]ottom, [L]eft, [R]ight):
        //
        //   TTTTTTTTTTTT
        //   TTTTTTTTTTTT
        //   LL        RR
        //   LL        RR
        //   LL        RR
        //   LL        RR
        //   LL        RR
        //   LL        RR
        //   BBBBBBBBBBBB
        //   BBBBBBBBBBBB
        //

        thick *= -1.0f;

        const auto top = Rectangle{ rec.x - thick, rec.y - thick, rec.width + thick*2.0f, thick };
        const auto bottom = Rectangle{ rec.x - thick, rec.y + rec.height, rec.width + thick*2.0f, thick};
        const auto left = Rectangle{ rec.x - thick, rec.y, thick, rec.height };
        const auto right = Rectangle{ rec.x + rec.width, rec.y, thick, rec.height };

        DrawRectangleRec(top, color);
        DrawRectangleRec(bottom, color);
        DrawRectangleRec(left, color);
        DrawRectangleRec(right, color);
    }
}

// Draw rectangle with rounded edges
void DrawRectangleRounded(const Rectangle rec, f32 roundness, i32 segments, const Color color)
{
    // Not a rounded rectangle
    if (roundness <= 0.0f)
    {
        DrawRectangleRec(rec, color);
        return;
    }

    if (roundness >= 1.0f) roundness = 1.0f;

    // Calculate corner radius
    const f32 radius = (rec.width > rec.height)? (rec.height*roundness)/2 : (rec.width*roundness)/2;
    if (radius <= 0.0f) return;

    // Calculate number of segments to use for the corners
    if (segments < 1)
    {
        // Calculate the maximum angle between segments based on the error rate (usually 0.5f)
        const f32 th = acosf(2*powf(1 - SMOOTH_CIRCLE_ERROR_RATE/radius, 2) - 1);
        segments = static_cast<i32>(ceilf((2*PI/th)/4.0f));
        if (segments <= 0) segments = 4;
    }

    const f32 stepLength = 90.0f/static_cast<f32>(segments);

    /*
    Quick sketch to make sense of all of this,
    there are 9 parts to draw, also mark the 12 points used

          P0____________________P1
          /|                    |\
         /1|          2         |3\
     P7 /__|____________________|__\ P2
       |   |P8                P9|   |
       | 8 |          9         | 4 |
       | __|____________________|__ |
     P6 \  |P11              P10|  / P3
         \7|          6         |5/
          \|____________________|/
          P5                    P4
    */

    // The x-coordinates used for the rounded rect
    f32 x0 = rec.x + radius;
    f32 x1 = (rec.x + rec.width) - radius;
    f32 x2 = rec.x + rec.width;
    f32 x3 = rec.x;

    // The y-coordinates used for the rounded rect
    f32 y0 = rec.y;
    f32 y1 = rec.y + radius;
    f32 y2 = (rec.y + rec.height) - radius;
    f32 y3 = rec.y + rec.height;

    const vec2 points[12] = {
        { x0, y0 }, // P0
        { x1, y0 }, // P1
        { x2, y1 }, // P2
        { x2, y2 }, // P3
        { x1, y3 }, // P4
        { x0, y3 }, // P5
        { x3, y2 }, // P6
        { x3, y1 }, // P7
        { x0, y1 }, // P8
        { x1, y1 }, // P9
        { x1, y2 }, // P10
        { x0, y2 }  // P11
    };

    const vec2 centers[4] = { points[8], points[9], points[10], points[11] };
    const f32 angles[4] = { 180.0f, 270.0f, 0.0f, 90.0f };

#if SUPPORT_QUADS_DRAW_MODE
    rlSetTexture(GetShapesTexture().id);
    const auto shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);
        // Draw all the 4 corners: [1] Upper Left Corner, [3] Upper Right Corner, [5] Lower Right Corner, [7] Lower Left Corner
        for (i32 k = 0; k < 4; k++) // Hope the compiler is smart enough to unroll this loop
        {
            f32 angle = angles[k];
            const vec2 center = centers[k];

            // NOTE: Every QUAD actually represents two segments
            for (i32 i = 0; i < segments/2; i++)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);
                rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                rlVertex2f(center.x, center.y);

                rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength*2))*radius, center.y + sinf(DEG2RAD*(angle + stepLength*2))*radius);

                rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*radius, center.y + sinf(DEG2RAD*(angle + stepLength))*radius);

                rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                rlVertex2f(center.x + cosf(DEG2RAD*angle)*radius, center.y + sinf(DEG2RAD*angle)*radius);

                angle += (stepLength*2);
            }

            // NOTE: In case number of segments is odd, adding one last piece to the cake
            if ((segments%2) == 1)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);
                rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                rlVertex2f(center.x, center.y);

                rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*radius, center.y + sinf(DEG2RAD*(angle + stepLength))*radius);

                rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                rlVertex2f(center.x + cosf(DEG2RAD*angle)*radius, center.y + sinf(DEG2RAD*angle)*radius);

                rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                rlVertex2f(center.x, center.y);
            }
        }

        // [2] Upper Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(points[0].x, points[0].y);
        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(points[8].x, points[8].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(points[9].x, points[9].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(points[1].x, points[1].y);

        // [4] Right Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(points[2].x, points[2].y);
        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(points[9].x, points[9].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(points[10].x, points[10].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(points[3].x, points[3].y);

        // [6] Bottom Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(points[11].x, points[11].y);
        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(points[5].x, points[5].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(points[4].x, points[4].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(points[10].x, points[10].y);

        // [8] Left Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(points[7].x, points[7].y);
        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(points[6].x, points[6].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(points[11].x, points[11].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(points[8].x, points[8].y);

        // [9] Middle Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(points[8].x, points[8].y);
        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(points[11].x, points[11].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(points[10].x, points[10].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(points[9].x, points[9].y);

    rlEnd();
    rlSetTexture(0);
#else
    rlBegin(RL_TRIANGLES);

        // Draw all of the 4 corners: [1] Upper Left Corner, [3] Upper Right Corner, [5] Lower Right Corner, [7] Lower Left Corner
        for (i32 k = 0; k < 4; k++) // Hope the compiler is smart enough to unroll this loop
        {
            f32 angle = angles[k];
            vec2 center = centers[k];
            for (i32 i = 0; i < segments; i++)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);
                rlVertex2f(center.x, center.y);
                rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*radius, center.y + sinf(DEG2RAD*(angle + stepLength))*radius);
                rlVertex2f(center.x + cosf(DEG2RAD*angle)*radius, center.y + sinf(DEG2RAD*angle)*radius);
                angle += stepLength;
            }
        }

        // [2] Upper Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(points[0].x, points[0].y);
        rlVertex2f(points[8].x, points[8].y);
        rlVertex2f(points[9].x, points[9].y);
        rlVertex2f(points[1].x, points[1].y);
        rlVertex2f(points[0].x, points[0].y);
        rlVertex2f(points[9].x, points[9].y);

        // [4] Right Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(points[9].x, points[9].y);
        rlVertex2f(points[10].x, points[10].y);
        rlVertex2f(points[3].x, points[3].y);
        rlVertex2f(points[2].x, points[2].y);
        rlVertex2f(points[9].x, points[9].y);
        rlVertex2f(points[3].x, points[3].y);

        // [6] Bottom Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(points[11].x, points[11].y);
        rlVertex2f(points[5].x, points[5].y);
        rlVertex2f(points[4].x, points[4].y);
        rlVertex2f(points[10].x, points[10].y);
        rlVertex2f(points[11].x, points[11].y);
        rlVertex2f(points[4].x, points[4].y);

        // [8] Left Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(points[7].x, points[7].y);
        rlVertex2f(points[6].x, points[6].y);
        rlVertex2f(points[11].x, points[11].y);
        rlVertex2f(points[8].x, points[8].y);
        rlVertex2f(points[7].x, points[7].y);
        rlVertex2f(points[11].x, points[11].y);

        // [9] Middle Rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(points[8].x, points[8].y);
        rlVertex2f(points[11].x, points[11].y);
        rlVertex2f(points[10].x, points[10].y);
        rlVertex2f(points[9].x, points[9].y);
        rlVertex2f(points[8].x, points[8].y);
        rlVertex2f(points[10].x, points[10].y);
    rlEnd();
#endif
}

// Draw rectangle with rounded edges
void DrawRectangleRoundedLines(const Rectangle rec, f32 roundness, i32 segments, const Color color)
{
    // Not a rounded rectangle
    if (roundness <= 0.0f)
    {
        DrawRectangleLines(static_cast<i32>(rec.x), static_cast<i32>(rec.y), static_cast<i32>(rec.width), static_cast<i32>(rec.height), color);
        return;
    }

    if (roundness >= 1.0f) roundness = 1.0f;

    // Calculate corner radius
    const f32 radius = (rec.width > rec.height)? (rec.height*roundness)/2 : (rec.width*roundness)/2;
    if (radius <= 0.0f) return;

    // Calculate number of segments to use for the corners
    if (segments < 1)
    {
        // Calculate the maximum angle between segments based on the error rate (usually 0.5f)
        const f32 th = acosf(2*powf(1 - SMOOTH_CIRCLE_ERROR_RATE/radius, 2) - 1);
        segments = static_cast<i32>(ceilf((2*PI/th)/4.0f));
        if (segments <= 0) segments = 4;
    }

    const f32 stepLength = 90.0f/static_cast<f32>(segments);

    /*
    Quick sketch to make sense of all of this,
    marks the 8 + 4 (corner centers P8-11) points used

           P0 ------------------ P1
          /                        \
         /                          \
     P7 /                            \ P2
       |    *P8               P9*     |
       |                              |
       |                              |
     P6 \   *P11             P10*    / P3
         \                          /
          \                        /
           P5 ------------------ P4
    */

    // The x-coordinates used for the outline
    f32 x0 = rec.x + radius + 0.5f;
    f32 x1 = (rec.x + rec.width) - radius - 0.5f;
    f32 x2 = (rec.x + rec.width) - 0.5f;
    f32 x3 = rec.x + 0.5f;

    // The y-coordinates used for the outline
    f32 y0 = rec.y + 0.5f;
    f32 y1 = rec.y + radius + 0.5f;
    f32 y2 = (rec.y + rec.height) - radius - 0.5f;
    f32 y3 = (rec.y + rec.height) - 0.5f;

    const vec2 points[8] = {
        { x0, y0 }, // P0
        { x1, y0 }, // P1
        { x2, y1 }, // P2
        { x2, y2 }, // P3
        { x1, y3 }, // P4
        { x0, y3 }, // P5
        { x3, y2 }, // P6
        { x3, y1 }, // P7
    };

    const vec2 centers[4] = {
        { x0, y1 }, // P8
        { x1, y1 }, // P9
        { x1, y2 }, // P10
        { x0, y2 }  // P11
    };

    const f32 angles[4] = { 180.0f, 270.0f, 0.0f, 90.0f };

    rlBegin(RL_LINES);
        // Draw all the 4 corners first: Upper Left Corner, Upper Right Corner, Lower Right Corner, Lower Left Corner
        for (i32 k = 0; k < 4; k++) // Hope the compiler is smart enough to unroll this loop
        {
            f32 angle = angles[k];
            const vec2 center = centers[k];

            for (i32 i = 0; i < segments; i++)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);
                rlVertex2f(center.x + cosf(DEG2RAD*angle)*radius, center.y + sinf(DEG2RAD*angle)*radius);
                rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*radius, center.y + sinf(DEG2RAD*(angle + stepLength))*radius);
                angle += stepLength;
            }
        }

        // And now the remaining 4 lines
        for (i32 i = 0; i < 8; i += 2)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(points[i].x, points[i].y);
            rlVertex2f(points[i + 1].x, points[i + 1].y);
        }
    rlEnd();
}

// Draw rectangle with rounded edges outline with line thickness
void DrawRectangleRoundedLinesEx(Rectangle rec, f32 roundness, i32 segments, const f32 thick, const Color color)
{
    // Not a rounded rectangle
    if (roundness <= 0.0f)
    {
        DrawRectangleLinesEx(rec, thick, color);
        return;
    }

    if (roundness >= 1.0f) roundness = 1.0f;

    auto roundedOutlineThick = 0.0f;
    auto outerRadius = 0.0f;
    auto innerRadius = 0.0f;
    if (thick >= 0.0f)
    {
        // Calculate corner radius
        const f32 radius = (rec.width > rec.height)? (rec.height*roundness)/2 : (rec.width*roundness)/2;
        if (radius <= 0.0f) return;

        outerRadius = radius;
        innerRadius = outerRadius - thick;

        // The maximum thickness the outline can have and still be rounded on the interior edge is equal to the corner radius
        // Put another way, when `innerRadius <= 0`, the interior of the outline is just a normal rectangle with no rounding
        if (innerRadius <= 0.0f)
        {
            innerRadius = 0.0f;
            roundedOutlineThick = outerRadius;

            // Draw the not-rounded portion of the outline
            DrawRectangleLinesEx(Rectangle{ rec.x + outerRadius, rec.y + outerRadius, rec.width - outerRadius*2.0f, rec.height - outerRadius*2.0f }, thick - outerRadius, color);
        }
        else
        {
            roundedOutlineThick = thick;
        }

        // Calculate number of segments to use for the corners
        if (segments < 1)
        {
            // Calculate the maximum angle between segments based on the error rate (usually 0.5f)
            const f32 th = acosf(2*powf(1 - SMOOTH_CIRCLE_ERROR_RATE/outerRadius, 2) - 1);
            segments = static_cast<i32>(ceilf((2*PI/th)/4.0f));
            if (segments <= 0) segments = 4;
        }
    }
    else
    {
        // Calculate corner radius
        const f32 radius = (rec.width > rec.height)? (rec.height*roundness)/2 : (rec.width*roundness)/2;
        if (radius <= 0.0f) return; // Only possible if the rectangle has 0 width or height

        // Expand the rectangle
        rec.x += thick;
        rec.y += thick;
        rec.width -= thick*2.0f;
        rec.height -= thick*2.0f;

        innerRadius = radius;
        outerRadius = innerRadius - thick;
        roundedOutlineThick = -thick;

        // Calculate number of segments to use for the corners
        if (segments < 1)
        {
            // Calculate the maximum angle between segments based on the error rate (usually 0.5f)
            const f32 th = acosf(2*powf(1 - SMOOTH_CIRCLE_ERROR_RATE/innerRadius, 2) - 1);
            segments = static_cast<i32>(ceilf((2*PI/th)/4.0f));
            if (segments <= 0) segments = 4;
        }
    }

    const f32 stepLength = 90.0f/static_cast<f32>(segments);

    /*
    Quick sketch to make sense of all of this,
    marks the 16 + 4 (corner centers P16-19) points used

           P0 ================== P1
          // P8                P9 \\
         //                        \\
     P7 // P15                  P10 \\ P2
       ||   *P16             P17*    ||
       ||                            ||
       || P14                   P11  ||
     P6 \\  *P19             P18*   // P3
         \\                        //
          \\ P13              P12 //
           P5 ================== P4
    */

    // The x-coordinates used for the outline
    f32 x0 = rec.x + outerRadius;
    f32 x1 = (rec.x + rec.width) - outerRadius;
    f32 x2 = rec.x + rec.width;
    f32 x3 = rec.x;
    f32 x4 = (rec.x + rec.width) - roundedOutlineThick;
    f32 x5 = rec.x + roundedOutlineThick;

    // The y-coordinates used for the outline
    f32 y0 = rec.y;
    f32 y1 = rec.y + outerRadius;
    f32 y2 = (rec.y + rec.height) - outerRadius;
    f32 y3 = rec.y + rec.height;
    f32 y4 = rec.y + roundedOutlineThick;
    f32 y5 = (rec.y + rec.height) - roundedOutlineThick;

    const vec2 points[16] = {
        { x0, y0 }, // P0
        { x1, y0 }, // P1
        { x2, y1 }, // P2
        { x2, y2 }, // P3
        { x1, y3 }, // P4
        { x0, y3 }, // P5
        { x3, y2 }, // P6
        { x3, y1 }, // P7
        { x0, y4 }, // P8
        { x1, y4 }, // P9
        { x4, y1 }, // P10
        { x4, y2 }, // P11
        { x1, y5 }, // P12
        { x0, y5 }, // P13
        { x5, y2 }, // P14
        { x5, y1 }  // P15
    };

    const vec2 centers[4] = {
        { x0, y1 }, // P16
        { x1, y1 }, // P17
        { x1, y2 }, // P18
        { x0, y2 }  // P19
    };

    const f32 angles[4] = { 180.0f, 270.0f, 0.0f, 90.0f };

#if SUPPORT_QUADS_DRAW_MODE
    rlSetTexture(GetShapesTexture().id);
    const auto shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);

        // Draw all the 4 corners first: Upper Left Corner, Upper Right Corner, Lower Right Corner, Lower Left Corner
        for (i32 k = 0; k < 4; k++) // Hope the compiler is smart enough to unroll this loop
        {
            f32 angle = angles[k];
            const vec2 center = centers[k];
            for (i32 i = 0; i < segments; i++)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);

                rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerRadius, center.y + sinf(DEG2RAD*angle)*innerRadius);

                rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerRadius);

                rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*outerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*outerRadius);

                rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);

                angle += stepLength;
            }
        }

        // Upper rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(points[0].x, points[0].y);
        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(points[8].x, points[8].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(points[9].x, points[9].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(points[1].x, points[1].y);

        // Right rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(points[2].x, points[2].y);
        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(points[10].x, points[10].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(points[11].x, points[11].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(points[3].x, points[3].y);

        // Lower rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(points[13].x, points[13].y);
        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(points[5].x, points[5].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(points[4].x, points[4].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(points[12].x, points[12].y);

        // Left rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(points[15].x, points[15].y);
        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(points[7].x, points[7].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
        rlVertex2f(points[6].x, points[6].y);
        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
        rlVertex2f(points[14].x, points[14].y);

    rlEnd();
    rlSetTexture(0);
#else
    rlBegin(RL_TRIANGLES);

        // Draw all of the 4 corners first: Upper Left Corner, Upper Right Corner, Lower Right Corner, Lower Left Corner
        for (i32 k = 0; k < 4; k++) // Hope the compiler is smart enough to unroll this loop
        {
            f32 angle = angles[k];
            vec2 center = centers[k];

            for (i32 i = 0; i < segments; i++)
            {
                rlColor4ub(color.r, color.g, color.b, color.a);

                rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerRadius, center.y + sinf(DEG2RAD*angle)*innerRadius);
                rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerRadius);
                rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);

                rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerRadius);
                rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*outerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*outerRadius);
                rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);

                angle += stepLength;
            }
        }

        // Upper rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(points[0].x, points[0].y);
        rlVertex2f(points[8].x, points[8].y);
        rlVertex2f(points[9].x, points[9].y);
        rlVertex2f(points[1].x, points[1].y);
        rlVertex2f(points[0].x, points[0].y);
        rlVertex2f(points[9].x, points[9].y);

        // Right rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(points[10].x, points[10].y);
        rlVertex2f(points[11].x, points[11].y);
        rlVertex2f(points[3].x, points[3].y);
        rlVertex2f(points[2].x, points[2].y);
        rlVertex2f(points[10].x, points[10].y);
        rlVertex2f(points[3].x, points[3].y);

        // Lower rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(points[13].x, points[13].y);
        rlVertex2f(points[5].x, points[5].y);
        rlVertex2f(points[4].x, points[4].y);
        rlVertex2f(points[12].x, points[12].y);
        rlVertex2f(points[13].x, points[13].y);
        rlVertex2f(points[4].x, points[4].y);

        // Left rectangle
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(points[7].x, points[7].y);
        rlVertex2f(points[6].x, points[6].y);
        rlVertex2f(points[14].x, points[14].y);
        rlVertex2f(points[15].x, points[15].y);
        rlVertex2f(points[7].x, points[7].y);
        rlVertex2f(points[14].x, points[14].y);
    rlEnd();
#endif
}

// Draw a polygon of n sides
void DrawPoly(const vec2 center, i32 sides, const f32 radius, const f32 rotation, const Color color)
{
    if (sides < 3) sides = 3;
    f32 centralAngle = rotation*DEG2RAD;
    const f32 angleStep = 360.0f/static_cast<f32>(sides)*DEG2RAD;

#if SUPPORT_QUADS_DRAW_MODE
    rlSetTexture(GetShapesTexture().id);
    const auto shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);
        for (i32 i = 0; i < sides; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            const f32 nextAngle = centralAngle + angleStep;

            rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x, center.y);

            rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(centralAngle)*radius, center.y + sinf(centralAngle)*radius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x + cosf(nextAngle)*radius, center.y + sinf(nextAngle)*radius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(centralAngle)*radius, center.y + sinf(centralAngle)*radius);

            centralAngle = nextAngle;
        }
    rlEnd();
    rlSetTexture(0);
#else
    rlBegin(RL_TRIANGLES);
        for (i32 i = 0; i < sides; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlVertex2f(center.x, center.y);
            rlVertex2f(center.x + cosf(centralAngle + angleStep)*radius, center.y + sinf(centralAngle + angleStep)*radius);
            rlVertex2f(center.x + cosf(centralAngle)*radius, center.y + sinf(centralAngle)*radius);

            centralAngle += angleStep;
        }
    rlEnd();
#endif
}

// Draw a polygon outline of n sides
void DrawPolyLines(const vec2 center, i32 sides, const f32 radius, const f32 rotation, const Color color)
{
    if (sides < 3) sides = 3;
    f32 centralAngle = rotation*DEG2RAD;
    const f32 angleStep = 360.0f/static_cast<f32>(sides)*DEG2RAD;

    rlBegin(RL_LINES);
        for (i32 i = 0; i < sides; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlVertex2f(center.x + cosf(centralAngle)*radius, center.y + sinf(centralAngle)*radius);
            rlVertex2f(center.x + cosf(centralAngle + angleStep)*radius, center.y + sinf(centralAngle + angleStep)*radius);

            centralAngle += angleStep;
        }
    rlEnd();
}

void DrawPolyLinesEx(const vec2 center, i32 sides, const f32 radius, const f32 rotation, f32 thick, const Color color)
{
    if (sides < 3) sides = 3;
    f32 centralAngle = rotation*DEG2RAD;
    const f32 exteriorAngle = 360.0f/static_cast<f32>(sides)*DEG2RAD;
    const f32 apothem = radius*cosf(DEG2RAD*180.0f/static_cast<f32>(sides));

    auto outerRadius = 0.0f;
    auto innerRadius = 0.0f;
    if (thick >= 0.0f)
    {
        outerRadius = radius;
        innerRadius = fmaxf(0.0f, radius - thick*(radius/apothem));
    }
    else
    {
        thick *= -1.0f;
        outerRadius = radius + thick*(radius/apothem);
        innerRadius = radius;
    }

#if SUPPORT_QUADS_DRAW_MODE
    rlSetTexture(GetShapesTexture().id);
    const auto shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);
        for (i32 i = 0; i < sides; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            const f32 nextAngle = centralAngle + exteriorAngle;

            rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(centralAngle)*outerRadius, center.y + sinf(centralAngle)*outerRadius);

            rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x + cosf(centralAngle)*innerRadius, center.y + sinf(centralAngle)*innerRadius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(nextAngle)*innerRadius, center.y + sinf(nextAngle)*innerRadius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x + cosf(nextAngle)*outerRadius, center.y + sinf(nextAngle)*outerRadius);

            centralAngle = nextAngle;
        }
    rlEnd();
    rlSetTexture(0);
#else
    rlBegin(RL_TRIANGLES);
        for (i32 i = 0; i < sides; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            f32 nextAngle = centralAngle + exteriorAngle;

            rlVertex2f(center.x + cosf(nextAngle)*outerRadius, center.y + sinf(nextAngle)*outerRadius);
            rlVertex2f(center.x + cosf(centralAngle)*outerRadius, center.y + sinf(centralAngle)*outerRadius);
            rlVertex2f(center.x + cosf(centralAngle)*innerRadius, center.y + sinf(centralAngle)*innerRadius);

            rlVertex2f(center.x + cosf(centralAngle)*innerRadius, center.y + sinf(centralAngle)*innerRadius);
            rlVertex2f(center.x + cosf(nextAngle)*innerRadius, center.y + sinf(nextAngle)*innerRadius);
            rlVertex2f(center.x + cosf(nextAngle)*outerRadius, center.y + sinf(nextAngle)*outerRadius);

            centralAngle = nextAngle;
        }
    rlEnd();
#endif
}

// Draw a color-filled circle
void DrawCircle(const i32 centerX, const i32 centerY, const f32 radius, const Color color)
{
    DrawCircleV(vec2{ static_cast<f32>(centerX), static_cast<f32>(centerY) }, radius, color);
}

// Draw a color-filled circle (Vector version)
// NOTE: On OpenGL 3.3 and ES2 using QUADS to avoid drawing order issues
void DrawCircleV(const vec2 center, const f32 radius, const Color color)
{
    DrawCircleSector(center, radius, 0, 360, 36, color);
}

// Draw a gradient-filled circle
void DrawCircleGradient(const vec2 center, const f32 radius, const Color inner, const Color outer)
{
    rlBegin(RL_TRIANGLES);
        for (i32 i = 0; i < 360; i += 10)
        {
            rlColor4ub(inner.r, inner.g, inner.b, inner.a);
            rlVertex2f(center.x, center.y);
            rlColor4ub(outer.r, outer.g, outer.b, outer.a);
            rlVertex2f(center.x + cosf(DEG2RAD*(i + 10))*radius, center.y + sinf(DEG2RAD*(i + 10))*radius);
            rlColor4ub(outer.r, outer.g, outer.b, outer.a);
            rlVertex2f(center.x + cosf(DEG2RAD*i)*radius, center.y + sinf(DEG2RAD*i)*radius);
        }
    rlEnd();
}

// Draw a piece of a circle
void DrawCircleSector(const vec2 center, const f32 radius, f32 startAngle, f32 endAngle, i32 segments, const Color color)
{
    if (startAngle == endAngle) return;
    if (radius <= 0.0f) return; // There's nothing to draw (also avoid div by zero)

    // Function expects (endAngle > startAngle)
    if (endAngle < startAngle)
    {
        // Swap values
        const f32 tmp = startAngle;
        startAngle = endAngle;
        endAngle = tmp;
    }

    // Drawing a whole circle, things get weird without limiting the circle to 360 degrees
    if (endAngle - startAngle >= 360.0f) endAngle = startAngle + 360.0f;

    const auto minSegments = static_cast<i32>(ceilf((endAngle - startAngle)/90));

    if (segments < minSegments)
    {
        // Calculate the maximum angle between segments based on the error rate (usually 0.5f)
        const f32 th = acosf(2*powf(1 - SMOOTH_CIRCLE_ERROR_RATE/radius, 2) - 1);
        segments = static_cast<i32>(ceilf((endAngle - startAngle)*(2*PI/th)/360.0f));

        if (segments <= 0) segments = minSegments;
    }

    const f32 stepLength = (endAngle - startAngle)/static_cast<f32>(segments);
    f32 angle = startAngle;

#if SUPPORT_QUADS_DRAW_MODE
    rlSetTexture(GetShapesTexture().id);
    const auto shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);

        // NOTE: Every QUAD actually represents two segments
        for (i32 i = 0; i < segments/2; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x, center.y);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength*2.0f))*radius, center.y + sinf(DEG2RAD*(angle + stepLength*2.0f))*radius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*radius, center.y + sinf(DEG2RAD*(angle + stepLength))*radius);

            rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*radius, center.y + sinf(DEG2RAD*angle)*radius);

            angle += (stepLength*2.0f);
        }

        // NOTE: In case number of segments is odd, adding one last piece to the cake
        if ((segments & 1) == 1)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x, center.y);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*radius, center.y + sinf(DEG2RAD*(angle + stepLength))*radius);

            rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*radius, center.y + sinf(DEG2RAD*angle)*radius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x, center.y);
        }

    rlEnd();

    rlSetTexture(0);
#else
    rlBegin(RL_TRIANGLES);
        for (i32 i = 0; i < segments; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlVertex2f(center.x, center.y);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*radius, center.y + sinf(DEG2RAD*(angle + stepLength))*radius);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*radius, center.y + sinf(DEG2RAD*angle)*radius);

            angle += stepLength;
        }
    rlEnd();
#endif
}

// Draw a piece of a circle outlines
void DrawCircleSectorLines(const vec2 center, const f32 radius, f32 startAngle, f32 endAngle, i32 segments, const Color color)
{
    if (startAngle == endAngle) return;
    if (radius <= 0.0f) return; // There's nothing to draw (also avoid div by zero)

    // Function expects (endAngle > startAngle)
    if (endAngle < startAngle)
    {
        // Swap values
        const f32 tmp = startAngle;
        startAngle = endAngle;
        endAngle = tmp;
    }

    auto showCapLines = true;
    // Drawing a whole circle, things get weird without limiting the circle to 360 degrees
    if (endAngle - startAngle >= 360.0f)
    {
        showCapLines = false;
        endAngle = startAngle + 360.0f;
    }

    const auto minSegments = static_cast<i32>(ceilf((endAngle - startAngle)/90));

    if (segments < minSegments)
    {
        // Calculate the maximum angle between segments based on the error rate (usually 0.5f)
        const f32 th = acosf(2*powf(1 - SMOOTH_CIRCLE_ERROR_RATE/radius, 2) - 1);
        segments = static_cast<i32>(ceilf((endAngle - startAngle)*(2*PI/th)/360.0f));

        if (segments <= 0) segments = minSegments;
    }

    const f32 stepLength = (endAngle - startAngle)/static_cast<f32>(segments);
    f32 angle = startAngle;

    rlBegin(RL_LINES);
        if (showCapLines)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(center.x, center.y);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*radius, center.y + sinf(DEG2RAD*angle)*radius);
        }

        for (i32 i = 0; i < segments; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlVertex2f(center.x + cosf(DEG2RAD*angle)*radius, center.y + sinf(DEG2RAD*angle)*radius);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*radius, center.y + sinf(DEG2RAD*(angle + stepLength))*radius);

            angle += stepLength;
        }

        if (showCapLines)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(center.x, center.y);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*radius, center.y + sinf(DEG2RAD*angle)*radius);
        }
    rlEnd();
}

// Draw a piece of a circle outlines with thickness
void DrawCircleSectorLinesEx(const vec2 center, const f32 radius, f32 startAngle, f32 endAngle, i32 segments, const f32 thick, const Color color)
{
    if (startAngle == endAngle) return;
    if (radius <= 0.0f) return; // There's nothing to draw (also avoid div by zero)

    // Function expects (endAngle > startAngle)
    if (endAngle < startAngle)
    {
        // Swap values
        const f32 tmp = startAngle;
        startAngle = endAngle;
        endAngle = tmp;
    }

    auto showCapLines = true;
    // Drawing a whole circle, things get weird without limiting the circle to 360 degrees
    if (endAngle - startAngle >= 360.0f)
    {
        showCapLines = thick >= 0.0f;
        endAngle = startAngle + 360.0f;
    }

    const auto minSegments = static_cast<i32>(ceilf((endAngle - startAngle)/90));

    if (segments < minSegments)
    {
        // Calculate the maximum angle between segments based on the error rate (usually 0.5f)
        const f32 th = acosf(2*powf(1 - SMOOTH_CIRCLE_ERROR_RATE/radius, 2) - 1);
        segments = static_cast<i32>(ceilf((endAngle - startAngle)*(2*PI/th)/360.0f));

        if (segments <= 0) segments = minSegments;
    }

    const f32 stepLength = (endAngle - startAngle)/static_cast<f32>(segments);
    f32 angle = startAngle;

    /*
    A sketch to help make things clearer

    NOTE: Some considerations are different when `thick` is negative
          The vertices used here are still relevant, but would instead be outside of the circle
          S0 is always the center

    The circle sector outline is drawn in 3 main pieces, the circle outline, cap 1, and cap 2
    The circle outline is self explanatory
    Cap 1 covers the `startAngle` edge and cap 2 covers the `endAngle` edge
    S0 is the first shared point between the caps, and also the circle's center
    S1 is the second shared point (sometimes not shared) between the caps
      S1 is also C0 and C3 in this sketch. In certain cases, S1 goes outside of
      the circle and C0 and C3 become different points
    C1 is one of cap 1's vertices that is on the inside edge of the circle outline
    C2 is like C1, but is also on the `startAngle` edge
    C4 is cap 2's vertex that corresponds with C1
    C5 is cap 2's vertex that corresponds with C2, except on the `endAngle` edge

                   [][][][][]
               [][]        []
             []      []C4[]C5
           []    [][]  {}  {}
         []    []      {}  {}
       []    []        {}C {} <- endAngle
       []  []          {}a {}
     []    []          {}p {}
     []  []            {}2 {}
     []  []            {}  {}
     []  []            {}  {}     startAngle
     []  []            {}  S0{}{}{}{}{}{}{}{}C2[][]
     []  []            {}{}       Cap1       []  []
     []  []            S1{}{}{}{}{}{}{}{}{}{}C1  []
     []  []                                  []  []
     []    []                              []    []
       []  []       Not filled in          []  []
       []    []                          []    []
         []    []                      []    []
           []    [][]              [][]    []
             []      [][][][][][][]      []
               [][]  Circle outline  [][]
                   [][][][][][][][][]

    [] = Circle outline edge pixel
    {} = Cap outline edge pixel
    */

    // We are not drawing a circle, we are drawing an n-sided polygon
    // So, we need to adjust the outline thickness of the "circle" for it to look correct with fewer segments
    const f32 apothem = radius*cosf(DEG2RAD*((endAngle - startAngle)/2.0f)/static_cast<f32>(segments));
    const f32 radiusThick = thick*(radius/apothem);

    f32 outerRadius = radius;
    f32 innerRadius = radius - radiusThick;

    if (thick >= 0.0f)
    {
        if (thick >= innerRadius)
        {
            DrawCircleSector(center, radius, startAngle, endAngle, segments, color);
            return;
        }
    }
    else
    {
        const f32 tmp = outerRadius;
        outerRadius = innerRadius;
        innerRadius = tmp;
    }

    // Cap 1 vertices
    vec2 c0{};
    vec2 c1{};
    vec2 c2{};

    // Cap 2 vertices
    vec2 c3{};
    vec2 c4{};
    vec2 c5{};

    // The number of angle steps that come before C1 (from `startAngle`, counter clockwise)
    auto stepsBeforeC1 = 0;
    auto s1OutsideOfCircle = false;
    // The number of angle steps that come before C0 (from `startAngle`, counter clockwise)
    // Only used if S1 is outside of the circle
    auto stepsBeforeC0 = 0;

    if (showCapLines)
    {
        if (thick >= 0.0f)
        {
            c2 = vec2{ center.x + cosf(DEG2RAD*startAngle)*innerRadius, center.y + sinf(DEG2RAD*startAngle)*innerRadius };
            c5 = vec2{ center.x + cosf(DEG2RAD*endAngle)*innerRadius, center.y + sinf(DEG2RAD*endAngle)*innerRadius };

            // For C1 and C4, we need to find the point that lies on the circle (n-sided polygon, actually)
            // We want C1 and C4 to be `thick` pixels perpendicularly from the `startAngle` and `endAngle` edges
            // and to be on the `innerRadius` edge

            const f32 c1Angle = RAD2DEG*asinf(thick/innerRadius);

            // There are more segments before C1 than there are segments being drawn,
            // so the whole circle sector must be covered
            if (c1Angle/stepLength >= static_cast<f32>(segments))
            {
                DrawCircleSector(center, radius, startAngle, endAngle, segments, color);
                return;
            }

            // Do this after the previous check just in case `stepLength` is really small and
            // dividing by it produces a very large number
            stepsBeforeC1 = static_cast<i32>(c1Angle/stepLength);

            // The angles of the vertices on the circle outline before and after C1
            const f32 vertexAngleBeforeC1 = stepLength*static_cast<f32>(stepsBeforeC1);
            const f32 vertexAngleAfterC1 = stepLength*(static_cast<f32>(stepsBeforeC1 + 1));

            /*
            Here is another sketch

            We know which outline line segment C1 is on (`vertexAngleBeforeC1` and `vertexAngleAfterC1`)
            Now we just need to know where on that line segment C1 is

            We can change our frame of reference so that `startAngle` is 0 degrees and `center` is at the origin (0, 0)
            This makes the math much simpler because now we can just go straight down by `thick` pixels and
            use the horizontal line that passes through that point to determine where C1 is on our line segment

            The line segment is defined by p1 and p2, we need C1, which is on that edge
            The 'y' axis of C1 is equal to `thick` (within this modified frame of reference)

                         p1
                         /|
                        / |
                       /  |
                      /   |
                     /    |
                    /     |
                   /      |
                  /       |
                C1---------  <-- y axis = `thick`
                /
               /
             p2
            */

            auto p1 = vec2{ cosf(DEG2RAD*vertexAngleBeforeC1)*innerRadius, sinf(DEG2RAD*vertexAngleBeforeC1)*innerRadius };
            auto p2 = vec2{ cosf(DEG2RAD*vertexAngleAfterC1)*innerRadius, sinf(DEG2RAD*vertexAngleAfterC1)*innerRadius };

            // Find the `t` of C1 between p1 and p2 ('t' as in `Lerp(start, end, t)`)
            // This is used to lerp between the actual vertices (outside of our modified frame of reference)
            // before and after C1
            f32 t = (p1.y - thick)/(p1.y - p2.y);

            auto vertexBeforeCap1Vertex = vec2{ center.x + cosf(DEG2RAD*(startAngle + vertexAngleBeforeC1))*innerRadius, center.y + sinf(DEG2RAD*(startAngle + vertexAngleBeforeC1))*innerRadius };
            auto vertexAfterCap1Vertex = vec2{ center.x + cosf(DEG2RAD*(startAngle + vertexAngleAfterC1))*innerRadius, center.y + sinf(DEG2RAD*(startAngle + vertexAngleAfterC1))*innerRadius };

            c1.x = vertexBeforeCap1Vertex.x + (vertexAfterCap1Vertex.x - vertexBeforeCap1Vertex.x)*t;
            c1.y = vertexBeforeCap1Vertex.y + (vertexAfterCap1Vertex.y - vertexBeforeCap1Vertex.y)*t;

            auto vertexBeforeCap2Vertex = vec2{ center.x + cosf(DEG2RAD*(endAngle - vertexAngleBeforeC1))*innerRadius, center.y + sinf(DEG2RAD*(endAngle - vertexAngleBeforeC1))*innerRadius };
            auto vertexAfterCap2Vertex = vec2{ center.x + cosf(DEG2RAD*(endAngle - vertexAngleAfterC1))*innerRadius, center.y + sinf(DEG2RAD*(endAngle - vertexAngleAfterC1))*innerRadius };

            c4.x = vertexBeforeCap2Vertex.x + (vertexAfterCap2Vertex.x - vertexBeforeCap2Vertex.x)*t;
            c4.y = vertexBeforeCap2Vertex.y + (vertexAfterCap2Vertex.y - vertexBeforeCap2Vertex.y)*t;

            /*
            Another sketch couldn't hurt

            This is a "zoomed in" view of the center of the circle sector
            You can see where S0 is, and where we want S1 to be
            `innerAngleBetweenCapEnds` is the angle of the diagonal line ('//') between the cap ends
            `S1Length` is the length of that line

            Since the caps are always parallel to `startAngle` and `endAngle`,
            we always have a right triangle we can use to determine where S1 is

             []      []
             []      []
             []  C   [] <- endAngle
             []  a   []
             []  p   []
             []  2   []  startAngle
             [][][][]S0[][][][][][]
                   //[]
                 //  [] Cap 1
               //    []
             S1      [][][][][][][]
            */

            const f32 innerAngleBetweenCapEnds = ((endAngle - 90.0f) - (startAngle + 90.0f))/2.0f;
            const f32 s1Length = thick/cosf(DEG2RAD*innerAngleBetweenCapEnds);

            // As `startAngle` and `endAngle` draw more of a circle, S1 goes further out from the center
            // It can go so far that it is outside of the circle, by a lot
            // This case needs to be detected and handled
            // If S1 is within the circle, nothing special needs to happen
            // But, if S1 is outside of the circle, we need to find the two points (C0 and C3) where
            // the line segments C0->C1 and C0->C3 intersect the circle outline,
            // using the same method we used to find C1 and C4
            if ((innerAngleBetweenCapEnds < 90.0f) && (s1Length <= innerRadius))
            {
                // S1 is inside of the circle

                const f32 betweenStartAndEndAngle = (endAngle + startAngle)/2.0f;
                c0 = vec2{ center.x + cosf(DEG2RAD*betweenStartAndEndAngle)*s1Length, center.y + sinf(DEG2RAD*betweenStartAndEndAngle)*s1Length };
                c3 = c0;

                p1 = vec2{ c1.x - center.x, c1.y - center.y };
                p2 = vec2{ c0.x - center.x, c0.y - center.y };

                // Copied from "raymath.h" Vector2Angle()
                const f32 dot = p1.x*p2.x + p1.y*p2.y;
                const f32 det = p1.x*p2.y - p1.y*p2.x;
                const f32 c1ToS1Angle = atan2f(det, dot);

                // If C1 and C4 are on the wrong side of S1, the whole circle sector is covered
                if (c1ToS1Angle < 0.0f)
                {
                    DrawCircleSector(center, radius, startAngle, endAngle, segments, color);
                    return;
                }
            }
            else
            {
                // S1 is outside of the circle

                if (endAngle - startAngle <= 180.0f)
                {
                    DrawCircleSector(center, radius, startAngle, endAngle, segments, color);
                    return;
                }

                s1OutsideOfCircle = true;

                stepsBeforeC0 = static_cast<i32>((180.0f + RAD2DEG*asinf(thick/-innerRadius))/stepLength);

                // Reuse the code for finding C1 and C4 to find C0 and C3

                const f32 vertexAngleBeforeC0 = stepLength*static_cast<f32>(stepsBeforeC0);
                const f32 vertexAngleAfterC0 = stepLength*(static_cast<f32>(stepsBeforeC0 + 1));

                p1 = vec2{ cosf(DEG2RAD*vertexAngleBeforeC0)*innerRadius, sinf(DEG2RAD*vertexAngleBeforeC0)*innerRadius };
                p2 = vec2{ cosf(DEG2RAD*vertexAngleAfterC0)*innerRadius, sinf(DEG2RAD*vertexAngleAfterC0)*innerRadius };

                t = (p1.y - thick)/(p1.y - p2.y);

                vertexBeforeCap1Vertex = vec2{ center.x + cosf(DEG2RAD*(startAngle + vertexAngleBeforeC0))*innerRadius, center.y + sinf(DEG2RAD*(startAngle + vertexAngleBeforeC0))*innerRadius };
                vertexAfterCap1Vertex = vec2{ center.x + cosf(DEG2RAD*(startAngle + vertexAngleAfterC0))*innerRadius, center.y + sinf(DEG2RAD*(startAngle + vertexAngleAfterC0))*innerRadius };

                c0.x = vertexBeforeCap1Vertex.x + (vertexAfterCap1Vertex.x - vertexBeforeCap1Vertex.x)*t;
                c0.y = vertexBeforeCap1Vertex.y + (vertexAfterCap1Vertex.y - vertexBeforeCap1Vertex.y)*t;

                vertexBeforeCap2Vertex = vec2{ center.x + cosf(DEG2RAD*(endAngle - vertexAngleBeforeC0))*innerRadius, center.y + sinf(DEG2RAD*(endAngle - vertexAngleBeforeC0))*innerRadius };
                vertexAfterCap2Vertex = vec2{ center.x + cosf(DEG2RAD*(endAngle - vertexAngleAfterC0))*innerRadius, center.y + sinf(DEG2RAD*(endAngle - vertexAngleAfterC0))*innerRadius };

                c3.x = vertexBeforeCap2Vertex.x + (vertexAfterCap2Vertex.x - vertexBeforeCap2Vertex.x)*t;
                c3.y = vertexBeforeCap2Vertex.y + (vertexAfterCap2Vertex.y - vertexBeforeCap2Vertex.y)*t;
            }
        }
        else
        {
            const f32 outerAngleBetweenCapEnds = ((endAngle + 90.0f) - (startAngle - 90.0f))/2.0f;
            const f32 s1Length = thick/cosf(DEG2RAD*outerAngleBetweenCapEnds);
            const f32 betweenStartAndEndAngle = 180.0f + (endAngle + startAngle)/2.0f;
            c0 = vec2{ center.x + cosf(DEG2RAD*betweenStartAndEndAngle)*s1Length, center.y + sinf(DEG2RAD*betweenStartAndEndAngle)*s1Length };
            c3 = c0;

            c2 = vec2{ center.x + cosf(DEG2RAD*startAngle)*outerRadius, center.y + sinf(DEG2RAD*startAngle)*outerRadius };
            c5 = vec2{ center.x + cosf(DEG2RAD*endAngle)*outerRadius, center.y + sinf(DEG2RAD*endAngle)*outerRadius };

            // Change the frame of reference so that `center` is the origin and `startAngle` is 0 degrees

            auto c0Translated = vec2{ c0.x - center.x, c0.y - center.y };
            auto circleVertex1 = vec2{ c2.x - center.x, c2.y - center.y };
            auto circleVertex2 = vec2{ cosf(DEG2RAD*(startAngle + stepLength))*outerRadius, sinf(DEG2RAD*(startAngle + stepLength))*outerRadius };

            // Copied from "raymath.h" Vector2Rotate()
            f32 tempX = c0Translated.x;
            c0Translated.x = cosf(-DEG2RAD*startAngle)*tempX - sinf(-DEG2RAD*startAngle)*c0Translated.y;
            c0Translated.y = sinf(-DEG2RAD*startAngle)*tempX + cosf(-DEG2RAD*startAngle)*c0Translated.y;

            tempX = circleVertex1.x;
            circleVertex1.x = cosf(-DEG2RAD*startAngle)*tempX - sinf(-DEG2RAD*startAngle)*circleVertex1.y;
            circleVertex1.y = sinf(-DEG2RAD*startAngle)*tempX + cosf(-DEG2RAD*startAngle)*circleVertex1.y;

            tempX = circleVertex2.x;
            circleVertex2.x = cosf(-DEG2RAD*startAngle)*tempX - sinf(-DEG2RAD*startAngle)*circleVertex2.y;
            circleVertex2.y = sinf(-DEG2RAD*startAngle)*tempX + cosf(-DEG2RAD*startAngle)*circleVertex2.y;

            // Figure out the line that `circleVertex1` and `circleVertex2` are on
            f32 rise = circleVertex1.y - circleVertex2.y;
            f32 run = circleVertex1.x - circleVertex2.x;
            // Get where that line intersects the horizontal line that `c0Translated` is on
            const f32 c1Rise = c0Translated.y - circleVertex1.y;
            const f32 c1Run = (c1Rise/rise)*run;
            const f32 c1DistanceFromC0 = (circleVertex1.x + c1Run) - c0Translated.x;

            c1 = vec2{ c0.x + cosf(DEG2RAD*startAngle)*c1DistanceFromC0, c0.y + sinf(DEG2RAD*startAngle)*c1DistanceFromC0 };
            c4 = vec2{ c0.x + cosf(DEG2RAD*endAngle)*c1DistanceFromC0, c0.y + sinf(DEG2RAD*endAngle)*c1DistanceFromC0 };

            if (c1DistanceFromC0 < 0.0f)
            {
                // The caps are intersecting each other

                auto circleVertex3 = vec2{ c5.x - center.x, c5.y - center.y };
                auto circleVertex4 = vec2{ cosf(DEG2RAD*(endAngle - stepLength))*outerRadius, sinf(DEG2RAD*(endAngle - stepLength))*outerRadius };

                tempX = circleVertex3.x;
                circleVertex3.x = cosf(-DEG2RAD*startAngle)*tempX - sinf(-DEG2RAD*startAngle)*circleVertex3.y;
                circleVertex3.y = sinf(-DEG2RAD*startAngle)*tempX + cosf(-DEG2RAD*startAngle)*circleVertex3.y;

                tempX = circleVertex4.x;
                circleVertex4.x = cosf(-DEG2RAD*startAngle)*tempX - sinf(-DEG2RAD*startAngle)*circleVertex4.y;
                circleVertex4.y = sinf(-DEG2RAD*startAngle)*tempX + cosf(-DEG2RAD*startAngle)*circleVertex4.y;

                // `startAngle` is 0 degrees within this frame of reference,
                // so C1 just goes horizontally out from C0
                auto c1Translated = vec2{ c0Translated.x + c1DistanceFromC0, c0Translated.y };

                // Make `circleVertex2` the origin
                circleVertex1.x -= circleVertex2.x;
                circleVertex1.y -= circleVertex2.y;
                circleVertex3.x -= circleVertex2.x;
                circleVertex3.y -= circleVertex2.y;
                circleVertex4.x -= circleVertex2.x;
                circleVertex4.y -= circleVertex2.y;
                c1Translated.x -= circleVertex2.x;
                c1Translated.y -= circleVertex2.y;

                // Make the line between `circleVertex1` and `circleVertex2` a horizontal line
                const f32 theta = atan2f(circleVertex1.y, circleVertex1.x);

                // Copied from "raymath.h" Vector2Rotate()
                tempX = circleVertex1.x;
                circleVertex1.x = cosf(-theta)*tempX - sinf(-theta)*circleVertex1.y;
                circleVertex1.y = sinf(-theta)*tempX + cosf(-theta)*circleVertex1.y;

                tempX = circleVertex3.x;
                circleVertex3.x = cosf(-theta)*tempX - sinf(-theta)*circleVertex3.y;
                circleVertex3.y = sinf(-theta)*tempX + cosf(-theta)*circleVertex3.y;

                tempX = circleVertex4.x;
                circleVertex4.x = cosf(-theta)*tempX - sinf(-theta)*circleVertex4.y;
                circleVertex4.y = sinf(-theta)*tempX + cosf(-theta)*circleVertex4.y;

                tempX = c1Translated.x;
                c1Translated.x = cosf(-theta)*tempX - sinf(-theta)*c1Translated.y;
                c1Translated.y = sinf(-theta)*tempX + cosf(-theta)*c1Translated.y;

                // Find where the line that `circleVertex3` and `circleVertex4` are on would intersect the
                // line segment defined by `circleVertex1` and `c1Translated`
                rise = circleVertex3.y - circleVertex4.y;
                run = circleVertex3.x - circleVertex4.x;
                const f32 targetRise = -circleVertex3.y;
                const f32 targetX = circleVertex3.x + (targetRise/rise)*run;

                const f32 t = (c1Translated.x - targetX)/(c1Translated.x - circleVertex1.x);

                c1 = vec2{ c1.x + (c2.x - c1.x)*t, c1.y + (c2.y - c1.y)*t };
                c4 = c1;
                c0 = c1;
                c3 = c1;
            }

            // Swap vertices to correct the winding order
            vec2 temp = c0;
            c0 = c2;
            c2 = temp;

            temp = c3;
            c3 = c5;
            c5 = temp;
        }
    }

#if SUPPORT_QUADS_DRAW_MODE
    rlSetTexture(GetShapesTexture().id);
    const auto shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);

        rlColor4ub(color.r, color.g, color.b, color.a);

        // Draw the circle outline
        for (i32 i = 0; i < segments; i++)
        {
            rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);

            rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerRadius, center.y + sinf(DEG2RAD*angle)*innerRadius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerRadius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*outerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*outerRadius);

            angle += stepLength;
        }

        // Draw the caps
        if (showCapLines)
        {
            rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x, center.y);

            rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(c0.x, c0.y);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(c1.x, c1.y);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(c2.x, c2.y);


            rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x, center.y);

            rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(c5.x, c5.y);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(c4.x, c4.y);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(c3.x, c3.y);

            // Some extra work may be needed when `thick` is positive
            if (thick >= 0.0f)
            {
                // Fill in the gaps between cap 1 and the circle outline and cap 2 and the circle outline
                if (stepsBeforeC1 > 0)
                {
                    // Draw quads using pairs of vertices on the circle outline
                    angle = 0;
                    for (i32 i = 0; i < stepsBeforeC1/2; i++)
                    {
                        // Cap1
                        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(c1.x, c1.y);

                        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + (angle + stepLength*2.0f)))*innerRadius, center.y + sinf(DEG2RAD*(startAngle + (angle + stepLength*2.0f)))*innerRadius);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + (angle + stepLength)))*innerRadius, center.y + sinf(DEG2RAD*(startAngle + (angle + stepLength)))*innerRadius);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle))*innerRadius, center.y + sinf(DEG2RAD*(startAngle + angle))*innerRadius);

                        // Cap2
                        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(c4.x, c4.y);

                        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle))*innerRadius, center.y + sinf(DEG2RAD*(endAngle - angle))*innerRadius);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - (angle + stepLength)))*innerRadius, center.y + sinf(DEG2RAD*(endAngle - (angle + stepLength)))*innerRadius);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - (angle + stepLength*2.0f)))*innerRadius, center.y + sinf(DEG2RAD*(endAngle - (angle + stepLength*2.0f)))*innerRadius);

                        angle += stepLength*2.0f;
                    }

                    // Draw the last segment if there's an odd number of them
                    if ((stepsBeforeC1%2) == 1)
                    {
                        // Cap1
                        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(c1.x, c1.y);

                        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(c1.x, c1.y);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + (angle + stepLength)))*innerRadius, center.y + sinf(DEG2RAD*(startAngle + (angle + stepLength)))*innerRadius);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle))*innerRadius, center.y + sinf(DEG2RAD*(startAngle + angle))*innerRadius);

                        // Cap2
                        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(c4.x, c4.y);

                        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(c4.x, c4.y);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle))*innerRadius, center.y + sinf(DEG2RAD*(endAngle - angle))*innerRadius);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - (angle + stepLength)))*innerRadius, center.y + sinf(DEG2RAD*(endAngle - (angle + stepLength)))*innerRadius);
                    }
                }

                // Fill in the gap between C0, C3 and the circle outline
                if (s1OutsideOfCircle)
                {
                    i32 verticesBetweenC0andC3 = (segments - stepsBeforeC0*2) - 1;

                    // No gap to fill
                    if (verticesBetweenC0andC3 == 0)
                    {
                        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(center.x, center.y);

                        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(center.x, center.y);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(c3.x, c3.y);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(c0.x, c0.y);
                    }
                    // There's a gap to fill
                    else
                    {
                        // Triangle touching C0
                        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(center.x, center.y);

                        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(center.x, center.y);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + stepLength*static_cast<f32>(stepsBeforeC0 + 1)))*innerRadius, center.y + sinf(DEG2RAD*(startAngle + stepLength*static_cast<f32>(stepsBeforeC0 + 1)))*innerRadius);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(c0.x, c0.y);

                        // Triangle touching C3
                        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(center.x, center.y);

                        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(center.x, center.y);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(c3.x, c3.y);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - stepLength*static_cast<f32>(stepsBeforeC0 + 1)))*innerRadius, center.y + sinf(DEG2RAD*(endAngle - stepLength*static_cast<f32>(stepsBeforeC0 + 1)))*innerRadius);

                        // Triangles between the previous two
                        verticesBetweenC0andC3 -= 1;
                        angle = startAngle + stepLength*(stepsBeforeC0 + 1);
                        for (i32 i = 0; i < verticesBetweenC0andC3/2; i++)
                        {
                            rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                            rlVertex2f(center.x, center.y);

                            rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength*2.0f))*innerRadius, center.y + sinf(DEG2RAD*(angle + stepLength*2.0f))*innerRadius);

                            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerRadius);

                            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                            rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerRadius, center.y + sinf(DEG2RAD*angle)*innerRadius);

                            angle += stepLength*2.0f;
                        }

                        if ((verticesBetweenC0andC3%2) == 1)
                        {
                            rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                            rlVertex2f(center.x, center.y);

                            rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                            rlVertex2f(center.x, center.y);

                            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerRadius);

                            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                            rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerRadius, center.y + sinf(DEG2RAD*angle)*innerRadius);
                        }
                    }
                }
            }
        }
    rlEnd();

    rlSetTexture(0);
#else
    rlBegin(RL_TRIANGLES);

        rlColor4ub(color.r, color.g, color.b, color.a);

        // Draw the circle outline
        for (i32 i = 0; i < segments; i++)
        {
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerRadius, center.y + sinf(DEG2RAD*angle)*innerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerRadius);

            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*outerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*outerRadius);

            angle += stepLength;
        }

        // Draw the caps
        if (showCapLines)
        {
            // Cap 1
            rlVertex2f(center.x, center.y);
            rlVertex2f(c0.x, c0.y);
            rlVertex2f(c1.x, c1.y);

            rlVertex2f(center.x, center.y);
            rlVertex2f(c1.x, c1.y);
            rlVertex2f(c2.x, c2.y);

            // Cap 2
            rlVertex2f(center.x, center.y);
            rlVertex2f(c5.x, c5.y);
            rlVertex2f(c4.x, c4.y);

            rlVertex2f(center.x, center.y);
            rlVertex2f(c4.x, c4.y);
            rlVertex2f(c3.x, c3.y);

            // Some extra work may be needed when `thick` is positive
            if (thick >= 0.0f)
            {
                // Fill in the gaps between cap 1 and the circle outline and cap 2 and the circle outline
                if (stepsBeforeC1 > 0)
                {
                    angle = 0;
                    for (i32 i = 0; i < stepsBeforeC1; i++)
                    {
                        // Cap 1
                        rlVertex2f(c1.x, c1.y);
                        rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + (angle + stepLength)))*innerRadius, center.y + sinf(DEG2RAD*(startAngle + (angle + stepLength)))*innerRadius);
                        rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle))*innerRadius, center.y + sinf(DEG2RAD*(startAngle + angle))*innerRadius);

                        // Cap 2
                        rlVertex2f(c4.x, c4.y);
                        rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle))*innerRadius, center.y + sinf(DEG2RAD*(endAngle - angle))*innerRadius);
                        rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - (angle + stepLength)))*innerRadius, center.y + sinf(DEG2RAD*(endAngle - (angle + stepLength)))*innerRadius);

                        angle += stepLength;
                    }
                }

                // Fill in the gap between C0, C3 and the circle outline
                if (s1OutsideOfCircle)
                {
                    i32 verticesBetweenC0andC3 = (segments - stepsBeforeC0*2) - 1;

                    // No gap to fill
                    if (verticesBetweenC0andC3 == 0)
                    {
                        rlVertex2f(center.x, center.y);
                        rlVertex2f(c3.x, c3.y);
                        rlVertex2f(c0.x, c0.y);
                    }
                    // There's a gap to fill
                    else
                    {
                        // Triangle touching C0
                        rlVertex2f(center.x, center.y);
                        rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + stepLength*static_cast<f32>((stepsBeforeC0 + 1))))*innerRadius, center.y + sinf(DEG2RAD*(startAngle + stepLength*static_cast<f32>((stepsBeforeC0 + 1))))*innerRadius);
                        rlVertex2f(c0.x, c0.y);

                        // Triangle touching C3
                        rlVertex2f(center.x, center.y);
                        rlVertex2f(c3.x, c3.y);
                        rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - stepLength*static_cast<f32>((stepsBeforeC0 + 1))))*innerRadius, center.y + sinf(DEG2RAD*(endAngle - stepLength*static_cast<f32>((stepsBeforeC0 + 1))))*innerRadius);

                        // Triangles between the previous two
                        verticesBetweenC0andC3 -= 1;
                        angle = startAngle + stepLength*(stepsBeforeC0 + 1);
                        for (i32 i = 0; i < verticesBetweenC0andC3; i++)
                        {
                            rlVertex2f(center.x, center.y);
                            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerRadius);
                            rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerRadius, center.y + sinf(DEG2RAD*angle)*innerRadius);

                            angle += stepLength;
                        }
                    }
                }
            }
        }

    rlEnd();
#endif
}

// Draw circle outline
void DrawCircleLines(const i32 centerX, const i32 centerY, const f32 radius, const Color color)
{
    DrawCircleLinesV(vec2{ static_cast<f32>(centerX), static_cast<f32>(centerY) }, radius, color);
}

// Draw circle outline (Vector version)
void DrawCircleLinesV(const vec2 center, const f32 radius, const Color color)
{
    rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);

        // NOTE: Circle outline is drawn as 36 line segments (one vertex every 10 degrees)
        for (i32 i = 0; i < 360; i += 10)
        {
            rlVertex2f(center.x + cosf(DEG2RAD*i)*radius, center.y + sinf(DEG2RAD*i)*radius);
            rlVertex2f(center.x + cosf(DEG2RAD*(i + 10))*radius, center.y + sinf(DEG2RAD*(i + 10))*radius);
        }
    rlEnd();
}

void DrawCircleLinesEx(const vec2 center, const f32 radius, const f32 thick, const Color color)
{
    DrawRing(center, radius - thick, radius, 0.0f, 360.0f, 36, color);
}

// Draw ellipse
void DrawEllipse(const i32 centerX, const i32 centerY, const f32 radiusH, const f32 radiusV, const Color color)
{
    DrawEllipseV(vec2{ static_cast<f32>(centerX), static_cast<f32>(centerY) }, radiusH, radiusV, color);
}

// Draw ellipse (Vector version)
void DrawEllipseV(const vec2 center, const f32 radiusH, const f32 radiusV, const Color color)
{
    rlBegin(RL_TRIANGLES);
        for (i32 i = 0; i < 360; i += 10)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(center.x,  center.y);
            rlVertex2f(center.x + cosf(DEG2RAD*(i + 10))*radiusH, center.y + sinf(DEG2RAD*(i + 10))*radiusV);
            rlVertex2f(center.x + cosf(DEG2RAD*i)*radiusH, center.y + sinf(DEG2RAD*i)*radiusV);
        }
    rlEnd();
}

// Draw ellipse outline
void DrawEllipseLines(const i32 centerX, const i32 centerY, const f32 radiusH, const f32 radiusV, const Color color)
{
    DrawEllipseLinesV(vec2{ static_cast<f32>(centerX), static_cast<f32>(centerY) }, radiusH, radiusV, color);
}

// Draw ellipse outline
void DrawEllipseLinesV(const vec2 center, const f32 radiusH, const f32 radiusV, const Color color)
{
    rlBegin(RL_LINES);
        for (i32 i = 0; i < 360; i += 10)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(center.x + cosf(DEG2RAD*(i + 10))*radiusH, center.y + sinf(DEG2RAD*(i + 10))*radiusV);
            rlVertex2f(center.x + cosf(DEG2RAD*i)*radiusH, center.y + sinf(DEG2RAD*i)*radiusV);
        }
    rlEnd();
}

// Draw ellipse outline with thickness
void DrawEllipseLinesEx(const vec2 center, const f32 radiusH, const f32 radiusV, const f32 thick, const Color color)
{
    f32 outerRadiusH = radiusH;
    f32 innerRadiusH = radiusH - thick;
    f32 outerRadiusV = radiusV;
    f32 innerRadiusV = radiusV - thick;

    if (thick >= 0.0f) {
        // Just a filled-in ellipse
        if ((innerRadiusH <= 0.0f) || (innerRadiusV <= 0.0f))
        {
            DrawEllipseV(center, radiusH, radiusV, color);
            return;
        }
    }
    else
    {
        // The outline is growing outside of the ellipse, so swap the inner and outer radius
        f32 tmp = outerRadiusH;
        outerRadiusH = innerRadiusH;
        innerRadiusH = tmp;

        tmp = outerRadiusV;
        outerRadiusV = innerRadiusV;
        innerRadiusV = tmp;
    }

#if SUPPORT_QUADS_DRAW_MODE
    rlSetTexture(GetShapesTexture().id);
    const auto shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);

        rlColor4ub(color.r, color.g, color.b, color.a);

        for (i32 i = 0; i < 360; i += 10)
        {
            rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*i)*innerRadiusH, center.y + sinf(DEG2RAD*i)*innerRadiusV);

            rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*(i + 10))*innerRadiusH, center.y + sinf(DEG2RAD*(i + 10))*innerRadiusV);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*(i + 10))*outerRadiusH, center.y + sinf(DEG2RAD*(i + 10))*outerRadiusV);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*i)*outerRadiusH, center.y + sinf(DEG2RAD*i)*outerRadiusV);
        }
    rlEnd();

    rlSetTexture(0);
#else
    rlBegin(RL_TRIANGLES);

        rlColor4ub(color.r, color.g, color.b, color.a);

        for (i32 i = 0; i < 360; i += 10)
        {
            rlVertex2f(center.x + cosf(DEG2RAD*i)*innerRadiusH, center.y + sinf(DEG2RAD*i)*innerRadiusV);
            rlVertex2f(center.x + cosf(DEG2RAD*(i + 10))*innerRadiusH, center.y + sinf(DEG2RAD*(i + 10))*innerRadiusV);
            rlVertex2f(center.x + cosf(DEG2RAD*(i + 10))*outerRadiusH, center.y + sinf(DEG2RAD*(i + 10))*outerRadiusV);

            rlVertex2f(center.x + cosf(DEG2RAD*i)*innerRadiusH, center.y + sinf(DEG2RAD*i)*innerRadiusV);
            rlVertex2f(center.x + cosf(DEG2RAD*(i + 10))*outerRadiusH, center.y + sinf(DEG2RAD*(i + 10))*outerRadiusV);
            rlVertex2f(center.x + cosf(DEG2RAD*i)*outerRadiusH, center.y + sinf(DEG2RAD*i)*outerRadiusV);
        }
    rlEnd();
#endif
}

// Draw ring
void DrawRing(const vec2 center, f32 innerRadius, f32 outerRadius, f32 startAngle, f32 endAngle, i32 segments, const Color color)
{
    if (startAngle == endAngle) return;

    // Function expects (outerRadius > innerRadius)
    if (outerRadius < innerRadius)
    {
        const f32 tmp = outerRadius;
        outerRadius = innerRadius;
        innerRadius = tmp;
    }

    if (outerRadius <= 0.0f) return; // There's nothing to draw (also avoid div by zero)

    // Function expects (endAngle > startAngle)
    if (endAngle < startAngle)
    {
        // Swap values
        const f32 tmp = startAngle;
        startAngle = endAngle;
        endAngle = tmp;
    }

    // Drawing a whole circle, things get weird without limiting the circle to 360 degrees
    if (endAngle - startAngle >= 360.0f) endAngle = startAngle + 360.0f;

    const auto minSegments = static_cast<i32>(ceilf((endAngle - startAngle)/90));

    if (segments < minSegments)
    {
        // Calculate the maximum angle between segments based on the error rate (usually 0.5f)
        const f32 th = acosf(2*powf(1 - SMOOTH_CIRCLE_ERROR_RATE/outerRadius, 2) - 1);
        segments = static_cast<i32>(ceilf((endAngle - startAngle)*(2*PI/th)/360.0f));

        if (segments <= 0) segments = minSegments;
    }

    // Not a ring
    if (innerRadius <= 0.0f)
    {
        DrawCircleSector(center, outerRadius, startAngle, endAngle, segments, color);
        return;
    }

    const f32 stepLength = (endAngle - startAngle)/static_cast<f32>(segments);
    f32 angle = startAngle;

#if SUPPORT_QUADS_DRAW_MODE
    rlSetTexture(GetShapesTexture().id);
    const auto shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);
        for (i32 i = 0; i < segments; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);

            rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerRadius, center.y + sinf(DEG2RAD*angle)*innerRadius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerRadius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*outerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*outerRadius);

            angle += stepLength;
        }
    rlEnd();

    rlSetTexture(0);
#else
    rlBegin(RL_TRIANGLES);
        for (i32 i = 0; i < segments; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerRadius, center.y + sinf(DEG2RAD*angle)*innerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);

            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*outerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*outerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);

            angle += stepLength;
        }
    rlEnd();
#endif
}

// Draw ring outline
void DrawRingLines(const vec2 center, f32 innerRadius, f32 outerRadius, f32 startAngle, f32 endAngle, i32 segments, const Color color)
{
    if (startAngle == endAngle) return;

    // Function expects (outerRadius > innerRadius)
    if (outerRadius < innerRadius)
    {
        const f32 tmp = outerRadius;
        outerRadius = innerRadius;
        innerRadius = tmp;
    }

    if (outerRadius <= 0.0f) return; // There's nothing to draw (also avoid div by zero)

    // Function expects (endAngle > startAngle)
    if (endAngle < startAngle)
    {
        // Swap values
        const f32 tmp = startAngle;
        startAngle = endAngle;
        endAngle = tmp;
    }

    auto showCapLines = true;
    // Drawing a whole circle, things get weird without limiting the circle to 360 degrees
    if (endAngle - startAngle >= 360.0f)
    {
        showCapLines = false;
        endAngle = startAngle + 360.0f;
    }

    const auto minSegments = static_cast<i32>(ceilf((endAngle - startAngle)/90));

    if (segments < minSegments)
    {
        // Calculate the maximum angle between segments based on the error rate (usually 0.5f)
        const f32 th = acosf(2*powf(1 - SMOOTH_CIRCLE_ERROR_RATE/outerRadius, 2) - 1);
        segments = static_cast<i32>(ceilf((endAngle - startAngle)*(2*PI/th)/360.0f));

        if (segments <= 0) segments = minSegments;
    }

    if (innerRadius <= 0.0f)
    {
        DrawCircleSectorLines(center, outerRadius, startAngle, endAngle, segments, color);
        return;
    }

    const f32 stepLength = (endAngle - startAngle)/static_cast<f32>(segments);
    f32 angle = startAngle;

    rlBegin(RL_LINES);
        if (showCapLines)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerRadius, center.y + sinf(DEG2RAD*angle)*innerRadius);
        }

        for (i32 i = 0; i < segments; i++)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*outerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*outerRadius);

            rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerRadius, center.y + sinf(DEG2RAD*angle)*innerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerRadius);

            angle += stepLength;
        }

        if (showCapLines)
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerRadius, center.y + sinf(DEG2RAD*angle)*outerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerRadius, center.y + sinf(DEG2RAD*angle)*innerRadius);
        }
    rlEnd();
}

// Draw ring outline with line thickness
void DrawRingLinesEx(const vec2 center, f32 innerRadius, f32 outerRadius, f32 startAngle, f32 endAngle, i32 segments, const f32 thick, const Color color)
{
    if (startAngle == endAngle) return;

    // Function expects (outerRadius > innerRadius)
    if (outerRadius < innerRadius)
    {
        const f32 tmp = outerRadius;
        outerRadius = innerRadius;
        innerRadius = tmp;
    }

    if (outerRadius <= 0.0f) return; // There's nothing to draw (also avoid div by zero)

    // Function expects (endAngle > startAngle)
    if (endAngle < startAngle)
    {
        // Swap values
        const f32 tmp = startAngle;
        startAngle = endAngle;
        endAngle = tmp;
    }

    auto showCapLines = true;
    // Drawing a whole circle, things get weird without limiting the circle to 360 degrees
    if (endAngle - startAngle >= 360.0f)
    {
        showCapLines = thick >= 0.0f;
        endAngle = startAngle + 360.0f;
    }

    const auto minSegments = static_cast<i32>(ceilf((endAngle - startAngle)/90));

    if (segments < minSegments)
    {
        // Calculate the maximum angle between segments based on the error rate (usually 0.5f)
        const f32 th = acosf(2*powf(1 - SMOOTH_CIRCLE_ERROR_RATE/outerRadius, 2) - 1);
        segments = static_cast<i32>(ceilf((endAngle - startAngle)*(2*PI/th)/360.0f));

        if (segments <= 0) segments = minSegments;
    }

    const f32 stepLength = (endAngle - startAngle)/static_cast<f32>(segments);

    // We are not drawing a circle, we are drawing an n-sided polygon
    // So, we need to adjust the outline thickness of the "circle" for it to look correct with fewer segments
    const f32 apothem = outerRadius*cosf(DEG2RAD*((endAngle - startAngle)/2.0f)/static_cast<f32>(segments));
    const f32 radiusThick = thick*(outerRadius/apothem);

    // These names can be confusing, but they are useful
    // Since 2 rings are being drawn, there are 4 radiuses (or radii)
    // "Inner" means closer to the center, "outer" means farther from the center
    // Sorted from farthest to closest you get:
    //   1. outerOuterRadius (farthest)
    //   2. innerOuterRadius
    //   3. outerInnerRadius
    //   4. innerInnerRadius (closest)
    auto innerOuterRadius = 0.0f;
    auto outerOuterRadius = 0.0f;
    auto innerInnerRadius = 0.0f;
    auto outerInnerRadius = 0.0f;

    if (thick >= 0.0f)
    {
        innerRadius = fmaxf(0.0f, innerRadius);

        // Just a filled-in ring
        if (radiusThick > (outerRadius - innerRadius)/2.0f)
        {
            DrawRing(center, innerRadius, outerRadius, startAngle, endAngle, segments, color);
            return;
        }

        innerInnerRadius = innerRadius;
        outerInnerRadius = innerInnerRadius + radiusThick;

        outerOuterRadius = outerRadius;
        innerOuterRadius = outerOuterRadius - radiusThick;
    }
    else
    {
        // Just a circle sector outline
        if (innerRadius <= 0.0f)
        {
            DrawCircleSectorLinesEx(center, outerRadius, startAngle, endAngle, segments, thick, color);
            return;
        }

        outerInnerRadius = innerRadius;
        innerInnerRadius = fmaxf(0.0f, outerInnerRadius + radiusThick);

        innerOuterRadius = outerRadius;
        outerOuterRadius = innerOuterRadius - radiusThick;
    }

    // For positive `thick` values
    auto stepsBeforeInner = 0;
    auto stepsBeforeOuter = 0;
    auto tInner = 0.0f;
    auto tOuter = 0.0f;
    auto innerAnglesCrossEachOther = false;

    // For negative `thick` values
    vec2 cap1SecondInnerVertex{};
    vec2 cap1SecondOuterVertex{};
    vec2 cap2SecondInnerVertex{};
    vec2 cap2SecondOuterVertex{};
    auto capsIntersect = false;
    vec2 capIntersectionVertex{};

    if (showCapLines)
    {
        if (thick >= 0.0f)
        {
            // Get the angle of the arc that has `thick` length along the inner and outer radii
            const f32 cap1InnerAngleEnd = RAD2DEG*(thick/outerInnerRadius);
            const f32 cap1OuterAngleEnd = RAD2DEG*(thick/innerOuterRadius);

            // Just a filled-in ring
            if (endAngle - startAngle < cap1OuterAngleEnd*2.0f)
            {
                DrawRing(center, innerRadius, outerRadius, startAngle, endAngle, segments, color);
                return;
            }

            if (endAngle - startAngle < cap1InnerAngleEnd*2.0f) innerAnglesCrossEachOther = true;

            stepsBeforeInner = static_cast<i32>(cap1InnerAngleEnd/stepLength);
            stepsBeforeOuter = static_cast<i32>(cap1OuterAngleEnd/stepLength);

            // We need to find where `cap1InnerAngleEnd` intersects the edge defined
            // by `beforeInnerVertex` and `afterInnerVertex`
            //
            // We can make this easy by making `center` the origin (0, 0) and
            // making `cap1InnerAngleEnd` 0 degrees (a horizontal line)
            //
            // With that, we know these lines intersect when 'y' equals 0,
            // so we just need to solve for 't' (as in `Lerp(start, end, t)`)
            const auto beforeInnerVertex = vec2{ cosf(DEG2RAD*(static_cast<f32>(stepsBeforeInner)*stepLength - cap1InnerAngleEnd))*outerInnerRadius, sinf(DEG2RAD*(static_cast<f32>(stepsBeforeInner)*stepLength - cap1InnerAngleEnd))*outerInnerRadius };
            const auto afterInnerVertex = vec2{ cosf(DEG2RAD*(static_cast<f32>(stepsBeforeInner + 1)*stepLength - cap1InnerAngleEnd))*outerInnerRadius, sinf(DEG2RAD*(static_cast<f32>(stepsBeforeInner + 1)*stepLength - cap1InnerAngleEnd))*outerInnerRadius };
            tInner = beforeInnerVertex.y/(beforeInnerVertex.y - afterInnerVertex.y);

            // The same as above, but for the outer edge
            const auto beforeOuterVertex = vec2{ cosf(DEG2RAD*(static_cast<f32>(stepsBeforeOuter)*stepLength - cap1OuterAngleEnd))*innerOuterRadius, sinf(DEG2RAD*(static_cast<f32>(stepsBeforeOuter)*stepLength - cap1OuterAngleEnd))*innerOuterRadius };
            const auto afterOuterVertex = vec2{ cosf(DEG2RAD*(static_cast<f32>(stepsBeforeOuter + 1)*stepLength - cap1OuterAngleEnd))*innerOuterRadius, sinf(DEG2RAD*(static_cast<f32>(stepsBeforeOuter + 1)*stepLength - cap1OuterAngleEnd))*innerOuterRadius };
            tOuter = beforeOuterVertex.y/(beforeOuterVertex.y - afterOuterVertex.y);
        }
        else
        {
            // "Cap 1" is the outline on `startAngle` and "Cap 2" is the outline on `endAngle`

            /*
            A sketch to help make all this a little more understandable
            (This is an overly simplified representation of cap 1)

             I2[][][][][]O2  <- y = thick
             []          []
             []          []
             I0----------O0  <- angle = 0 degrees, y = 0
             []          []
             I1          O1  <- angle = stepLength

            Cap 2 is a mirror copy of cap 1, the inside and outside vertices switch sides

            We're using a frame of reference where `center` is (0, 0) and `startAngle` is 0 degrees

            I0 is `innerInnerRadius` distance from `center` at `starAngle`
            I1 is `innerInnerRadius` distance from `center` at `starAngle + stepLength`
            I2 goes out from I0 perpendicular to `startAngle`
            O0 is the same as I0, except using `outerOuterRadius` instead of `innerInnerRadius`
            O1 is the same as I1, except using `outerOuterRadius` instead of `innerInnerRadius`
            O2 is the same as I2, except goes out from O0

            The intersection cases between the caps edges are:
              1. No intersections, easy
              2. The I0->I2 and I2->O2 edges intersect between the caps
              3. The I0->I2 and O0->O2 edges intersect between the caps

            Notice that cap 1 and 2's I2->O2 and O0->O2 edges can't intersect at the same time,
            and, if there's any intersection, I0->I2 is one of the edges
            */

            auto cap1O0 = vec2{ outerOuterRadius, 0.0f };
            auto cap1O1 = vec2{ cosf(DEG2RAD*stepLength)*outerOuterRadius, sinf(DEG2RAD*stepLength)*outerOuterRadius };

            // Assuming a linear interpolation such as `value = Lerp(start, end, t)`
            // We can find O2 by getting its 't' between O1.y and O0.y (which is always greater than 1)
            // We can solve for `t` using `t = (start - value)/(start - end)`
            // Since we know `end = 0` we can simplify it to `t = (start - value)/start`
            const f32 tOuter = (cap1O1.y - thick)/cap1O1.y;
            auto cap1O2 = vec2{ cap1O1.x + (cap1O0.x - cap1O1.x)*tOuter, thick };

            //vec2 cap1I0 = { innerInnerRadius, 0.0f }; // Not used

            const f32 capLongEdgeLength = outerOuterRadius - innerInnerRadius;
            auto cap1I2 = vec2{ cap1O2.x - capLongEdgeLength, thick };

            auto cap2O0 = vec2{ cosf(DEG2RAD*(endAngle - startAngle))*outerOuterRadius, sinf(DEG2RAD*(endAngle - startAngle))*outerOuterRadius };
            auto cap2O1 = vec2{ cosf(DEG2RAD*(endAngle - startAngle - stepLength))*outerOuterRadius, sinf(DEG2RAD*(endAngle - startAngle - stepLength))*outerOuterRadius };
            auto cap2O2 = vec2{ cap2O1.x + (cap2O0.x - cap2O1.x)*tOuter, cap2O1.y + (cap2O0.y - cap2O1.y)*tOuter };

            auto cap2I0 = vec2{ cosf(DEG2RAD*(endAngle - startAngle))*innerInnerRadius, sinf(DEG2RAD*(endAngle - startAngle))*innerInnerRadius };
            auto cap2I2 = vec2{ cap2O2.x - cosf(DEG2RAD*(endAngle - startAngle))*capLongEdgeLength, cap2O2.y - sinf(DEG2RAD*(endAngle - startAngle))*capLongEdgeLength};

            // The 't' of the intersection between I2 and O2 (`Lerp(I2, O2, t)`)
            auto tCapLongEdgeCross = -1.0f;
            // Avoid division by zero
            if (cap2I2.y - cap2O2.y != 0.0f)
            {
                // Find where the long edge of cap 2 intersects the long edge of cap 1
                tCapLongEdgeCross = (cap2I2.y - thick)/(cap2I2.y - cap2O2.y);
                if ((tCapLongEdgeCross >= 0.0f) && (tCapLongEdgeCross <= 1.0f)) capsIntersect = true;
            }

            // Rotate the frame of reference so that cap 1's I0->I2 edge is a vertical line
            const f32 rotateBy = -DEG2RAD*stepLength/2.0f;

            // Copied from "raymath.h" Vector2Rotate()
            // Though we only use the x axis, so we ignore the y axis
            const f32 cosres = cosf(rotateBy);
            const f32 sinres = sinf(rotateBy);

            cap1I2.x = cap1I2.x*cosres - cap1I2.y*sinres;
            cap1O2.x = cap1O2.x*cosres - cap1O2.y*sinres;
            cap2I0.x = cap2I0.x*cosres - cap2I0.y*sinres;
            cap2I2.x = cap2I2.x*cosres - cap2I2.y*sinres;
            cap2O0.x = cap2O0.x*cosres - cap2O0.y*sinres;
            cap2O2.x = cap2O2.x*cosres - cap2O2.y*sinres;

            // The 't' of the intersection between I0 and I2 (`Lerp(I0, I2, t)`)
            auto tCrossInner = -1.0f;
            // Avoid division by zero
            if (cap2I0.x - cap2I2.x != 0.0f) tCrossInner = (cap2I0.x - cap1I2.x)/(cap2I0.x - cap2I2.x);
            // Make sure `tCrossInner` is 0 when it should be (mitigate floating-point rounding woes)
            if (innerInnerRadius <= 0.0f) tCrossInner = 0.0f;

            // The 't' of the intersection between O0 and O2 (`Lerp(O0, O2, t)`)
            auto tCrossOuter = -1.0f;
            // Avoid division by zero
            if (cap2O0.x - cap2O2.x != 0.0f) tCrossOuter = (cap2O0.x - cap1O2.x)/(cap2O0.x - cap2O2.x);

            // With our additional information, calculate the vertices we need
            // outside of our modified frame of reference

            cap1O0 = vec2{ center.x + cosf(DEG2RAD*startAngle)*outerOuterRadius, center.y + sinf(DEG2RAD*startAngle)*outerOuterRadius };
            cap1O1 = vec2{ center.x + cosf(DEG2RAD*(startAngle + stepLength))*outerOuterRadius, center.y + sinf(DEG2RAD*(startAngle + stepLength))*outerOuterRadius };
            cap1O2 = vec2{ cap1O1.x + (cap1O0.x - cap1O1.x)*tOuter, cap1O1.y + (cap1O0.y - cap1O1.y)*tOuter };

            cap2O0 = vec2{ center.x + cosf(DEG2RAD*endAngle)*outerOuterRadius, center.y + sinf(DEG2RAD*endAngle)*outerOuterRadius };
            cap2O1 = vec2{ center.x + cosf(DEG2RAD*(endAngle - stepLength))*outerOuterRadius, center.y + sinf(DEG2RAD*(endAngle - stepLength))*outerOuterRadius };
            cap2O2 = vec2{ cap2O1.x + (cap2O0.x - cap2O1.x)*tOuter, cap2O1.y + (cap2O0.y - cap2O1.y)*tOuter };

            //cap1I0 = vec2{ center.x + cosf(DEG2RAD*startAngle)*innerInnerRadius, center.y + sinf(DEG2RAD*startAngle)*innerInnerRadius };
            cap1I2 = vec2{ cap1O2.x - cosf(DEG2RAD*startAngle)*capLongEdgeLength, cap1O2.y - sinf(DEG2RAD*startAngle)*capLongEdgeLength };

            cap2I0 = vec2{ center.x + cosf(DEG2RAD*endAngle)*innerInnerRadius, center.y + sinf(DEG2RAD*endAngle)*innerInnerRadius };
            cap2I2 = vec2{ cap2O2.x - cosf(DEG2RAD*endAngle)*capLongEdgeLength, cap2O2.y - sinf(DEG2RAD*endAngle)*capLongEdgeLength };

            if (capsIntersect)
            {
                capIntersectionVertex = vec2{ cap2I2.x + (cap2O2.x - cap2I2.x)*tCapLongEdgeCross, cap2I2.y + (cap2O2.y - cap2I2.y)*tCapLongEdgeCross };

                cap2I2 = vec2{ cap2I0.x + (cap2I2.x - cap2I0.x)*tCrossInner, cap2I0.y + (cap2I2.y - cap2I0.y)*tCrossInner };
                cap1I2 = cap2I2;
            }
            else if ((tCrossOuter >= 0.0f) && (tCrossOuter <= 1.0f))
            {
                cap2O2 = vec2{ cap2O0.x + (cap2O2.x - cap2O0.x)*tCrossOuter, cap2O0.y + (cap2O2.y - cap2O0.y)*tCrossOuter };
                cap1O2 = cap2O2;

                cap2I2 = vec2{ cap2I0.x + (cap2I2.x - cap2I0.x)*tCrossInner, cap2I0.y + (cap2I2.y - cap2I0.y)*tCrossInner };
                cap1I2 = cap2I2;
            }

            cap1SecondInnerVertex = cap1I2;
            cap1SecondOuterVertex = cap1O2;
            cap2SecondInnerVertex = cap2I2;
            cap2SecondOuterVertex = cap2O2;
        }
    }

    f32 angle = startAngle;

#if SUPPORT_QUADS_DRAW_MODE
    rlSetTexture(GetShapesTexture().id);
    const auto shapeRect = GetShapesTextureRectangle();

    rlBegin(RL_QUADS);

        rlColor4ub(color.r, color.g, color.b, color.a);

        for (i32 i = 0; i < segments; i++)
        {
            // `innerRadius` outline
            rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerInnerRadius, center.y + sinf(DEG2RAD*angle)*outerInnerRadius);

            rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerInnerRadius, center.y + sinf(DEG2RAD*angle)*innerInnerRadius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerInnerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerInnerRadius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*outerInnerRadius);

            // `outerRadius` outline
            rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerOuterRadius, center.y + sinf(DEG2RAD*angle)*outerOuterRadius);

            rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerOuterRadius, center.y + sinf(DEG2RAD*angle)*innerOuterRadius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerOuterRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerOuterRadius);

            rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*outerOuterRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*outerOuterRadius);

            angle += stepLength;
        }

        if (showCapLines)
        {
            if (thick >= 0.0f)
            {
                angle = 0.0f;

                for (i32 i = 0; i < stepsBeforeOuter; i++)
                {
                    // Cap 1
                    rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle))*outerInnerRadius);

                    rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle + stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle + stepLength))*outerInnerRadius);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle + stepLength))*innerOuterRadius, center.y + sinf(DEG2RAD*(startAngle + angle + stepLength))*innerOuterRadius);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle))*innerOuterRadius, center.y + sinf(DEG2RAD*(startAngle + angle))*innerOuterRadius);

                    // Cap 2
                    rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle))*outerInnerRadius, center.y + sinf(DEG2RAD*(endAngle - angle))*outerInnerRadius);

                    rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle))*innerOuterRadius, center.y + sinf(DEG2RAD*(endAngle - angle))*innerOuterRadius);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle - stepLength))*innerOuterRadius, center.y + sinf(DEG2RAD*(endAngle - angle - stepLength))*innerOuterRadius);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle - stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(endAngle - angle - stepLength))*outerInnerRadius);

                    angle += stepLength;
                }

                // We've already moved `stepsBeforeOuter` steps from each end
                const i32 totalStepsLeft = segments - stepsBeforeOuter*2;
                const i32 innerStepsLeft = stepsBeforeInner - stepsBeforeOuter;

                // Cap 1
                const auto cap1OuterVertexBeforeEnd = vec2{ center.x + cosf(DEG2RAD*(startAngle + angle))*innerOuterRadius, center.y + sinf(DEG2RAD*(startAngle + angle))*innerOuterRadius };
                const auto cap1OuterVertexAfterEnd = vec2{ center.x + cosf(DEG2RAD*(startAngle + angle + stepLength))*innerOuterRadius, center.y + sinf(DEG2RAD*(startAngle + angle + stepLength))*innerOuterRadius };
                const auto cap1InnerVertexBeforeEnd = vec2{ center.x + cosf(DEG2RAD*(startAngle + angle + static_cast<f32>(innerStepsLeft)*stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle + static_cast<f32>(innerStepsLeft)*stepLength))*outerInnerRadius };
                const auto cap1InnerVertexAfterEnd = vec2{ center.x + cosf(DEG2RAD*(startAngle + angle + static_cast<f32>(innerStepsLeft + 1)*stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle + static_cast<f32>(innerStepsLeft + 1)*stepLength))*outerInnerRadius };
                const auto cap1InnerVertexEnd = vec2{ cap1InnerVertexBeforeEnd.x + (cap1InnerVertexAfterEnd.x - cap1InnerVertexBeforeEnd.x)*tInner, cap1InnerVertexBeforeEnd.y + (cap1InnerVertexAfterEnd.y - cap1InnerVertexBeforeEnd.y)*tInner };
                const auto cap1OuterVertexEnd = vec2{ cap1OuterVertexBeforeEnd.x + (cap1OuterVertexAfterEnd.x - cap1OuterVertexBeforeEnd.x)*tOuter, cap1OuterVertexBeforeEnd.y + (cap1OuterVertexAfterEnd.y - cap1OuterVertexBeforeEnd.y)*tOuter };

                // Cap 2
                const auto cap2OuterVertexBeforeEnd = vec2{ center.x + cosf(DEG2RAD*(endAngle - angle))*innerOuterRadius, center.y + sinf(DEG2RAD*(endAngle - angle))*innerOuterRadius };
                const auto cap2OuterVertexAfterEnd = vec2{ center.x + cosf(DEG2RAD*(endAngle - angle - stepLength))*innerOuterRadius, center.y + sinf(DEG2RAD*(endAngle - angle - stepLength))*innerOuterRadius };
                const auto cap2InnerVertexBeforeEnd = vec2{ center.x + cosf(DEG2RAD*(endAngle - angle - static_cast<f32>(innerStepsLeft)*stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(endAngle - angle - static_cast<f32>(innerStepsLeft)*stepLength))*outerInnerRadius };
                const auto cap2InnerVertexAfterEnd = vec2{ center.x + cosf(DEG2RAD*(endAngle - angle - static_cast<f32>(innerStepsLeft + 1)*stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(endAngle - angle - static_cast<f32>(innerStepsLeft + 1)*stepLength))*outerInnerRadius };
                const auto cap2InnerVertexEnd = vec2{ cap2InnerVertexBeforeEnd.x + (cap2InnerVertexAfterEnd.x - cap2InnerVertexBeforeEnd.x)*tInner, cap2InnerVertexBeforeEnd.y + (cap2InnerVertexAfterEnd.y - cap2InnerVertexBeforeEnd.y)*tInner };
                const auto cap2OuterVertexEnd = vec2{ cap2OuterVertexBeforeEnd.x + (cap2OuterVertexAfterEnd.x - cap2OuterVertexBeforeEnd.x)*tOuter, cap2OuterVertexBeforeEnd.y + (cap2OuterVertexAfterEnd.y - cap2OuterVertexBeforeEnd.y)*tOuter };

                const i32 stepsCount = (innerAnglesCrossEachOther)? totalStepsLeft/2 : innerStepsLeft;

                // Iterate over pairs of steps
                for (i32 i = 0; i < stepsCount/2; i++)
                {
                    // Cap 1
                    rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(cap1OuterVertexBeforeEnd.x, cap1OuterVertexBeforeEnd.y);

                    rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle))*outerInnerRadius);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle + stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle + stepLength))*outerInnerRadius);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle + stepLength*2.0f))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle + stepLength*2.0f))*outerInnerRadius);

                    // Cap 2
                    rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(cap2OuterVertexBeforeEnd.x, cap2OuterVertexBeforeEnd.y);

                    rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle - stepLength*2.0f))*outerInnerRadius, center.y + sinf(DEG2RAD*(endAngle - angle - stepLength*2.0f))*outerInnerRadius);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle - stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(endAngle - angle - stepLength))*outerInnerRadius);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle))*outerInnerRadius, center.y + sinf(DEG2RAD*(endAngle - angle))*outerInnerRadius);

                    angle += stepLength*2.0f;
                }

                // Handle the last step if there's an odd amount
                if ((stepsCount%2) == 1)
                {
                    // Cap 1
                    rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(cap1OuterVertexBeforeEnd.x, cap1OuterVertexBeforeEnd.y);

                    rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(cap1OuterVertexBeforeEnd.x, cap1OuterVertexBeforeEnd.y);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle))*outerInnerRadius);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle + stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle + stepLength))*outerInnerRadius);

                    // Cap 2
                    rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(cap2OuterVertexBeforeEnd.x, cap2OuterVertexBeforeEnd.y);

                    rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(cap2OuterVertexBeforeEnd.x, cap2OuterVertexBeforeEnd.y);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle - stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(endAngle - angle - stepLength))*outerInnerRadius);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle))*outerInnerRadius, center.y + sinf(DEG2RAD*(endAngle - angle))*outerInnerRadius);

                    angle += stepLength;
                }

                // When the inner angles coming from `startAngle` and `endAngle` cross each other,
                // the `*innerVertexEnd` vertices go past each other and cause the geometry to intersect itself
                if (innerAnglesCrossEachOther)
                {
                    // We need to find where the line defined by `cap1InnerVertexEnd` and `cap1OuterVertexEnd` intersects
                    // the line defined by `cap2InnerVertexEnd` and `cap2OuterVertexEnd`
                    // That point is then used instead to prevent the outline from intersecting itself

                    // Make `cap1InnerVertexEnd` the origin and the angle to `cap1OuterVertexEnd` 0 degrees
                    const auto tempCap1OuterVertexEnd = vec2{ cap1OuterVertexEnd.x - cap1InnerVertexEnd.x, cap1OuterVertexEnd.y - cap1InnerVertexEnd.y };
                    const auto tempCap2InnerVertexEnd = vec2{ cap2InnerVertexEnd.x - cap1InnerVertexEnd.x, cap2InnerVertexEnd.y - cap1InnerVertexEnd.y };
                    const auto tempCap2OuterVertexEnd = vec2{ cap2OuterVertexEnd.x - cap1InnerVertexEnd.x, cap2OuterVertexEnd.y - cap1InnerVertexEnd.y };

                    const f32 rotateBy = -atan2f(tempCap1OuterVertexEnd.y, tempCap1OuterVertexEnd.x);
                    // We only need the y coordinates, so only rotate the y coordinates
                    const f32 start = sinf(rotateBy)*tempCap2InnerVertexEnd.x + cosf(rotateBy)*tempCap2InnerVertexEnd.y;
                    const f32 end = sinf(rotateBy)*tempCap2OuterVertexEnd.x + cosf(rotateBy)*tempCap2OuterVertexEnd.y;
                    const f32 tCross = start/(start - end);

                    const auto intersection = vec2{ cap2InnerVertexEnd.x + (cap2OuterVertexEnd.x - cap2InnerVertexEnd.x)*tCross, cap2InnerVertexEnd.y + (cap2OuterVertexEnd.y - cap2InnerVertexEnd.y)*tCross };

                    if ((segments%2) == 0)
                    {
                        // There are an even number of segments (which means there's an odd number of vertices), so there's 1 vertex exactly in the middle

                        const auto middleInnerVertex = vec2{ center.x + cosf(DEG2RAD*(startAngle + angle))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle))*outerInnerRadius };

                        // Cap 1
                        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(intersection.x, intersection.y);

                        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(cap1OuterVertexEnd.x, cap1OuterVertexEnd.y);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(cap1OuterVertexBeforeEnd.x, cap1OuterVertexBeforeEnd.y);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(middleInnerVertex.x, middleInnerVertex.y);

                        // Cap 2
                        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(intersection.x, intersection.y);

                        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(middleInnerVertex.x, middleInnerVertex.y);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(cap2OuterVertexBeforeEnd.x, cap2OuterVertexBeforeEnd.y);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(cap2OuterVertexEnd.x, cap2OuterVertexEnd.y);
                    }
                    else
                    {
                        // There are an odd number of segments (which means there's an even number of vertices), so there are 2 vertices in the middle

                        const auto middleInnerVertex1 = vec2{ center.x + cosf(DEG2RAD*(startAngle + angle))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle))*outerInnerRadius };
                        const auto middleInnerVertex2 = vec2{ center.x + cosf(DEG2RAD*(endAngle - angle))*outerInnerRadius, center.y + sinf(DEG2RAD*(endAngle - angle))*outerInnerRadius };

                        // Cap 1
                        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(intersection.x, intersection.y);

                        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(cap1OuterVertexEnd.x, cap1OuterVertexEnd.y);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(cap1OuterVertexBeforeEnd.x, cap1OuterVertexBeforeEnd.y);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(middleInnerVertex1.x, middleInnerVertex1.y);

                        // Cap 2
                        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(intersection.x, intersection.y);

                        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(middleInnerVertex2.x, middleInnerVertex2.y);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(cap2OuterVertexBeforeEnd.x, cap2OuterVertexBeforeEnd.y);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(cap2OuterVertexEnd.x, cap2OuterVertexEnd.y);

                        // Triangle between the caps
                        rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(intersection.x, intersection.y);

                        rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(intersection.x, intersection.y);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                        rlVertex2f(middleInnerVertex1.x, middleInnerVertex1.y);

                        rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                        rlVertex2f(middleInnerVertex2.x, middleInnerVertex2.y);
                    }
                }
                else
                {
                    // Cap 1
                    rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(cap1OuterVertexBeforeEnd.x, cap1OuterVertexBeforeEnd.y);

                    rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(cap1InnerVertexBeforeEnd.x, cap1InnerVertexBeforeEnd.y);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(cap1InnerVertexEnd.x, cap1InnerVertexEnd.y);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(cap1OuterVertexEnd.x, cap1OuterVertexEnd.y);

                    // Cap 2
                    rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(cap2OuterVertexBeforeEnd.x, cap2OuterVertexBeforeEnd.y);

                    rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(cap2OuterVertexEnd.x, cap2OuterVertexEnd.y);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(cap2InnerVertexEnd.x, cap2InnerVertexEnd.y);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(cap2InnerVertexBeforeEnd.x, cap2InnerVertexBeforeEnd.y);
                }
            }
            else
            {
                // Cap 1
                const auto cap1FirstInnerVertex = vec2{ center.x + cosf(DEG2RAD*startAngle)*innerInnerRadius, center.y + sinf(DEG2RAD*startAngle)*innerInnerRadius };
                const auto cap1FirstOuterVertex = vec2{ center.x + cosf(DEG2RAD*startAngle)*outerOuterRadius, center.y + sinf(DEG2RAD*startAngle)*outerOuterRadius };

                rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                rlVertex2f(cap1FirstInnerVertex.x, cap1FirstInnerVertex.y);

                rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                rlVertex2f(cap1FirstOuterVertex.x, cap1FirstOuterVertex.y);

                rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                rlVertex2f(cap1SecondOuterVertex.x, cap1SecondOuterVertex.y);

                rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                rlVertex2f(cap1SecondInnerVertex.x, cap1SecondInnerVertex.y);

                // Cap 2
                const auto cap2FirstInnerVertex = vec2{ center.x + cosf(DEG2RAD*endAngle)*innerInnerRadius, center.y + sinf(DEG2RAD*endAngle)*innerInnerRadius };
                const auto cap2FirstOuterVertex = vec2{ center.x + cosf(DEG2RAD*endAngle)*outerOuterRadius, center.y + sinf(DEG2RAD*endAngle)*outerOuterRadius };

                rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                rlVertex2f(cap2FirstInnerVertex.x, cap2FirstInnerVertex.y);

                rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                rlVertex2f(cap2SecondInnerVertex.x, cap2SecondInnerVertex.y);

                rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                rlVertex2f(cap2SecondOuterVertex.x, cap2SecondOuterVertex.y);

                rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                rlVertex2f(cap2FirstOuterVertex.x, cap2FirstOuterVertex.y);

                if (capsIntersect)
                {
                    // Cap 1
                    rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(cap1SecondInnerVertex.x, cap1SecondInnerVertex.y);

                    rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(cap1SecondInnerVertex.x, cap1SecondInnerVertex.y);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(cap1SecondOuterVertex.x, cap1SecondOuterVertex.y);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(capIntersectionVertex.x, capIntersectionVertex.y);

                    // Cap 2
                    rlTexCoord2f(shapeRect.x/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(cap2SecondInnerVertex.x, cap2SecondInnerVertex.y);

                    rlTexCoord2f(shapeRect.x/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(capIntersectionVertex.x, capIntersectionVertex.y);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, shapeRect.y/texShapes.height);
                    rlVertex2f(cap2SecondOuterVertex.x, cap2SecondOuterVertex.y);

                    rlTexCoord2f((shapeRect.x + shapeRect.width)/texShapes.width, (shapeRect.y + shapeRect.height)/texShapes.height);
                    rlVertex2f(cap2SecondInnerVertex.x, cap2SecondInnerVertex.y);
                }
            }
        }
    rlEnd();
#else
    rlBegin(RL_TRIANGLES);

        rlColor4ub(color.r, color.g, color.b, color.a);

        for (i32 i = 0; i < segments; i++)
        {
            // `innerRadius` outline
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerInnerRadius, center.y + sinf(DEG2RAD*angle)*outerInnerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerInnerRadius, center.y + sinf(DEG2RAD*angle)*innerInnerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerInnerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerInnerRadius);

            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerInnerRadius, center.y + sinf(DEG2RAD*angle)*outerInnerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerInnerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerInnerRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*outerInnerRadius);

            // `outerRadius` outline
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerOuterRadius, center.y + sinf(DEG2RAD*angle)*outerOuterRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*angle)*innerOuterRadius, center.y + sinf(DEG2RAD*angle)*innerOuterRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerOuterRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerOuterRadius);

            rlVertex2f(center.x + cosf(DEG2RAD*angle)*outerOuterRadius, center.y + sinf(DEG2RAD*angle)*outerOuterRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*innerOuterRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*innerOuterRadius);
            rlVertex2f(center.x + cosf(DEG2RAD*(angle + stepLength))*outerOuterRadius, center.y + sinf(DEG2RAD*(angle + stepLength))*outerOuterRadius);

            angle += stepLength;
        }

        if (showCapLines)
        {
            if (thick >= 0.0f)
            {
                angle = 0.0f;

                for (i32 i = 0; i < stepsBeforeOuter; i++)
                {
                    // Cap 1
                    rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle))*outerInnerRadius);
                    rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle + stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle + stepLength))*outerInnerRadius);
                    rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle + stepLength))*innerOuterRadius, center.y + sinf(DEG2RAD*(startAngle + angle + stepLength))*innerOuterRadius);

                    rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle))*outerInnerRadius);
                    rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle + stepLength))*innerOuterRadius, center.y + sinf(DEG2RAD*(startAngle + angle + stepLength))*innerOuterRadius);
                    rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle))*innerOuterRadius, center.y + sinf(DEG2RAD*(startAngle + angle))*innerOuterRadius);

                    // Cap 2
                    rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle))*outerInnerRadius, center.y + sinf(DEG2RAD*(endAngle - angle))*outerInnerRadius);
                    rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle))*innerOuterRadius, center.y + sinf(DEG2RAD*(endAngle - angle))*innerOuterRadius);
                    rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle - stepLength))*innerOuterRadius, center.y + sinf(DEG2RAD*(endAngle - angle - stepLength))*innerOuterRadius);

                    rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle))*outerInnerRadius, center.y + sinf(DEG2RAD*(endAngle - angle))*outerInnerRadius);
                    rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle - stepLength))*innerOuterRadius, center.y + sinf(DEG2RAD*(endAngle - angle - stepLength))*innerOuterRadius);
                    rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle - stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(endAngle - angle - stepLength))*outerInnerRadius);

                    angle += stepLength;
                }

                // We've already moved `stepsBeforeOuter` steps from each end
                i32 totalStepsLeft = segments - stepsBeforeOuter*2;
                i32 innerStepsLeft = stepsBeforeInner - stepsBeforeOuter;

                // Cap 1
                auto cap1OuterVertexBeforeEnd = vec2{ center.x + cosf(DEG2RAD*(startAngle + angle))*innerOuterRadius, center.y + sinf(DEG2RAD*(startAngle + angle))*innerOuterRadius };
                auto cap1OuterVertexAfterEnd = vec2{ center.x + cosf(DEG2RAD*(startAngle + angle + stepLength))*innerOuterRadius, center.y + sinf(DEG2RAD*(startAngle + angle + stepLength))*innerOuterRadius };
                auto cap1InnerVertexBeforeEnd = vec2{ center.x + cosf(DEG2RAD*(startAngle + angle + static_cast<f32>(innerStepsLeft)*stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle + static_cast<f32>(innerStepsLeft)*stepLength))*outerInnerRadius };
                auto cap1InnerVertexAfterEnd = vec2{ center.x + cosf(DEG2RAD*(startAngle + angle + static_cast<f32>((innerStepsLeft + 1))*stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle + static_cast<f32>((innerStepsLeft + 1))*stepLength))*outerInnerRadius };
                auto cap1InnerVertexEnd = vec2{ cap1InnerVertexBeforeEnd.x + (cap1InnerVertexAfterEnd.x - cap1InnerVertexBeforeEnd.x)*tInner, cap1InnerVertexBeforeEnd.y + (cap1InnerVertexAfterEnd.y - cap1InnerVertexBeforeEnd.y)*tInner };
                auto cap1OuterVertexEnd = vec2{ cap1OuterVertexBeforeEnd.x + (cap1OuterVertexAfterEnd.x - cap1OuterVertexBeforeEnd.x)*tOuter, cap1OuterVertexBeforeEnd.y + (cap1OuterVertexAfterEnd.y - cap1OuterVertexBeforeEnd.y)*tOuter };

                // Cap 2
                auto cap2OuterVertexBeforeEnd = vec2{ center.x + cosf(DEG2RAD*(endAngle - angle))*innerOuterRadius, center.y + sinf(DEG2RAD*(endAngle - angle))*innerOuterRadius };
                auto cap2OuterVertexAfterEnd = vec2{ center.x + cosf(DEG2RAD*(endAngle - angle - stepLength))*innerOuterRadius, center.y + sinf(DEG2RAD*(endAngle - angle - stepLength))*innerOuterRadius };
                auto cap2InnerVertexBeforeEnd = vec2{ center.x + cosf(DEG2RAD*(endAngle - angle - static_cast<f32>(innerStepsLeft)*stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(endAngle - angle - static_cast<f32>(innerStepsLeft)*stepLength))*outerInnerRadius };
                auto cap2InnerVertexAfterEnd = vec2{ center.x + cosf(DEG2RAD*(endAngle - angle - static_cast<f32>((innerStepsLeft + 1))*stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(endAngle - angle - static_cast<f32>((innerStepsLeft + 1))*stepLength))*outerInnerRadius };
                auto cap2InnerVertexEnd = vec2{ cap2InnerVertexBeforeEnd.x + (cap2InnerVertexAfterEnd.x - cap2InnerVertexBeforeEnd.x)*tInner, cap2InnerVertexBeforeEnd.y + (cap2InnerVertexAfterEnd.y - cap2InnerVertexBeforeEnd.y)*tInner };
                auto cap2OuterVertexEnd = vec2{ cap2OuterVertexBeforeEnd.x + (cap2OuterVertexAfterEnd.x - cap2OuterVertexBeforeEnd.x)*tOuter, cap2OuterVertexBeforeEnd.y + (cap2OuterVertexAfterEnd.y - cap2OuterVertexBeforeEnd.y)*tOuter };

                i32 stepsCount = (innerAnglesCrossEachOther)? totalStepsLeft/2 : innerStepsLeft;

                for (i32 i = 0; i < stepsCount; i++)
                {
                    // Cap 1
                    rlVertex2f(cap1OuterVertexBeforeEnd.x, cap1OuterVertexBeforeEnd.y);
                    rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle))*outerInnerRadius);
                    rlVertex2f(center.x + cosf(DEG2RAD*(startAngle + angle + stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle + stepLength))*outerInnerRadius);

                    // Cap 2
                    rlVertex2f(cap2OuterVertexBeforeEnd.x, cap2OuterVertexBeforeEnd.y);
                    rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle - stepLength))*outerInnerRadius, center.y + sinf(DEG2RAD*(endAngle - angle - stepLength))*outerInnerRadius);
                    rlVertex2f(center.x + cosf(DEG2RAD*(endAngle - angle))*outerInnerRadius, center.y + sinf(DEG2RAD*(endAngle - angle))*outerInnerRadius);

                    angle += stepLength;
                }

                // When the inner angles coming from `startAngle` and `endAngle` cross each other,
                // the `*innerVertexEnd` vertices go past each other and cause the geometry to intersect itself
                if (innerAnglesCrossEachOther)
                {
                    // We need to find where the line defined by `cap1InnerVertexEnd` and `cap1OuterVertexEnd` intersects
                    // the line defined by `cap2InnerVertexEnd` and `cap2OuterVertexEnd`
                    // That point is then used instead to prevent the outline from intersecting itself

                    // Make `cap1InnerVertexEnd` the origin and the angle to `cap1OuterVertexEnd` 0 degrees
                    auto tempCap1OuterVertexEnd = vec2{ cap1OuterVertexEnd.x - cap1InnerVertexEnd.x, cap1OuterVertexEnd.y - cap1InnerVertexEnd.y };
                    auto tempCap2InnerVertexEnd = vec2{ cap2InnerVertexEnd.x - cap1InnerVertexEnd.x, cap2InnerVertexEnd.y - cap1InnerVertexEnd.y };
                    auto tempCap2OuterVertexEnd = vec2{ cap2OuterVertexEnd.x - cap1InnerVertexEnd.x, cap2OuterVertexEnd.y - cap1InnerVertexEnd.y };

                    f32 rotateBy = -atan2f(tempCap1OuterVertexEnd.y, tempCap1OuterVertexEnd.x);
                    // We only need the y coordinates, so only rotate the y coordinates
                    f32 start = sinf(rotateBy)*tempCap2InnerVertexEnd.x + cosf(rotateBy)*tempCap2InnerVertexEnd.y;
                    f32 end = sinf(rotateBy)*tempCap2OuterVertexEnd.x + cosf(rotateBy)*tempCap2OuterVertexEnd.y;
                    f32 tCross = start/(start - end);

                    auto intersection = vec2{ cap2InnerVertexEnd.x + (cap2OuterVertexEnd.x - cap2InnerVertexEnd.x)*tCross, cap2InnerVertexEnd.y + (cap2OuterVertexEnd.y - cap2InnerVertexEnd.y)*tCross };

                    if (segments%2 == 0)
                    {
                        // There are an even number of segments (which means there's an odd number of vertices), so there's 1 vertex exactly in the middle

                        auto middleInnerVertex = vec2{ center.x + cosf(DEG2RAD*(startAngle + angle))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle))*outerInnerRadius };

                        // Cap 1
                        rlVertex2f(intersection.x, intersection.y);
                        rlVertex2f(cap1OuterVertexEnd.x, cap1OuterVertexEnd.y);
                        rlVertex2f(cap1OuterVertexBeforeEnd.x, cap1OuterVertexBeforeEnd.y);

                        rlVertex2f(intersection.x, intersection.y);
                        rlVertex2f(cap1OuterVertexBeforeEnd.x, cap1OuterVertexBeforeEnd.y);
                        rlVertex2f(middleInnerVertex.x, middleInnerVertex.y);

                        // Cap 2
                        rlVertex2f(intersection.x, intersection.y);
                        rlVertex2f(middleInnerVertex.x, middleInnerVertex.y);
                        rlVertex2f(cap2OuterVertexBeforeEnd.x, cap2OuterVertexBeforeEnd.y);

                        rlVertex2f(intersection.x, intersection.y);
                        rlVertex2f(cap2OuterVertexBeforeEnd.x, cap2OuterVertexBeforeEnd.y);
                        rlVertex2f(cap2OuterVertexEnd.x, cap2OuterVertexEnd.y);
                    }
                    else
                    {
                        // There are an odd number of segments (which means there's an even number of vertices), so there are 2 vertices in the middle

                        auto middleInnerVertex1 = vec2{ center.x + cosf(DEG2RAD*(startAngle + angle))*outerInnerRadius, center.y + sinf(DEG2RAD*(startAngle + angle))*outerInnerRadius };
                        auto middleInnerVertex2 = vec2{ center.x + cosf(DEG2RAD*(endAngle - angle))*outerInnerRadius, center.y + sinf(DEG2RAD*(endAngle - angle))*outerInnerRadius };

                        // Cap 1
                        rlVertex2f(intersection.x, intersection.y);
                        rlVertex2f(cap1OuterVertexEnd.x, cap1OuterVertexEnd.y);
                        rlVertex2f(cap1OuterVertexBeforeEnd.x, cap1OuterVertexBeforeEnd.y);

                        rlVertex2f(intersection.x, intersection.y);
                        rlVertex2f(cap1OuterVertexBeforeEnd.x, cap1OuterVertexBeforeEnd.y);
                        rlVertex2f(middleInnerVertex1.x, middleInnerVertex1.y);

                        // Cap 2
                        rlVertex2f(intersection.x, intersection.y);
                        rlVertex2f(middleInnerVertex2.x, middleInnerVertex2.y);
                        rlVertex2f(cap2OuterVertexBeforeEnd.x, cap2OuterVertexBeforeEnd.y);

                        rlVertex2f(intersection.x, intersection.y);
                        rlVertex2f(cap2OuterVertexBeforeEnd.x, cap2OuterVertexBeforeEnd.y);
                        rlVertex2f(cap2OuterVertexEnd.x, cap2OuterVertexEnd.y);

                        // Triangle between the caps
                        rlVertex2f(intersection.x, intersection.y);
                        rlVertex2f(intersection.x, intersection.y);
                        rlVertex2f(middleInnerVertex1.x, middleInnerVertex1.y);

                        rlVertex2f(intersection.x, intersection.y);
                        rlVertex2f(middleInnerVertex1.x, middleInnerVertex1.y);
                        rlVertex2f(middleInnerVertex2.x, middleInnerVertex2.y);
                    }
                }
                else
                {
                    // Cap 1
                    rlVertex2f(cap1OuterVertexBeforeEnd.x, cap1OuterVertexBeforeEnd.y);
                    rlVertex2f(cap1InnerVertexBeforeEnd.x, cap1InnerVertexBeforeEnd.y);
                    rlVertex2f(cap1InnerVertexEnd.x, cap1InnerVertexEnd.y);

                    rlVertex2f(cap1OuterVertexBeforeEnd.x, cap1OuterVertexBeforeEnd.y);
                    rlVertex2f(cap1InnerVertexEnd.x, cap1InnerVertexEnd.y);
                    rlVertex2f(cap1OuterVertexEnd.x, cap1OuterVertexEnd.y);

                    // Cap 2
                    rlVertex2f(cap2OuterVertexBeforeEnd.x, cap2OuterVertexBeforeEnd.y);
                    rlVertex2f(cap2OuterVertexEnd.x, cap2OuterVertexEnd.y);
                    rlVertex2f(cap2InnerVertexEnd.x, cap2InnerVertexEnd.y);

                    rlVertex2f(cap2OuterVertexBeforeEnd.x, cap2OuterVertexBeforeEnd.y);
                    rlVertex2f(cap2InnerVertexEnd.x, cap2InnerVertexEnd.y);
                    rlVertex2f(cap2InnerVertexBeforeEnd.x, cap2InnerVertexBeforeEnd.y);
                }
            }
            else
            {
                // Cap 1
                auto cap1FirstInnerVertex = vec2{ center.x + cosf(DEG2RAD*startAngle)*innerInnerRadius, center.y + sinf(DEG2RAD*startAngle)*innerInnerRadius };
                auto cap1FirstOuterVertex = vec2{ center.x + cosf(DEG2RAD*startAngle)*outerOuterRadius, center.y + sinf(DEG2RAD*startAngle)*outerOuterRadius };

                rlVertex2f(cap1FirstInnerVertex.x, cap1FirstInnerVertex.y);
                rlVertex2f(cap1FirstOuterVertex.x, cap1FirstOuterVertex.y);
                rlVertex2f(cap1SecondOuterVertex.x, cap1SecondOuterVertex.y);

                rlVertex2f(cap1FirstInnerVertex.x, cap1FirstInnerVertex.y);
                rlVertex2f(cap1SecondOuterVertex.x, cap1SecondOuterVertex.y);
                rlVertex2f(cap1SecondInnerVertex.x, cap1SecondInnerVertex.y);

                // Cap 2
                auto cap2FirstInnerVertex = vec2{ center.x + cosf(DEG2RAD*endAngle)*innerInnerRadius, center.y + sinf(DEG2RAD*endAngle)*innerInnerRadius };
                auto cap2FirstOuterVertex = vec2{ center.x + cosf(DEG2RAD*endAngle)*outerOuterRadius, center.y + sinf(DEG2RAD*endAngle)*outerOuterRadius };

                rlVertex2f(cap2FirstInnerVertex.x, cap2FirstInnerVertex.y);
                rlVertex2f(cap2SecondInnerVertex.x, cap2SecondInnerVertex.y);
                rlVertex2f(cap2SecondOuterVertex.x, cap2SecondOuterVertex.y);

                rlVertex2f(cap2FirstInnerVertex.x, cap2FirstInnerVertex.y);
                rlVertex2f(cap2SecondOuterVertex.x, cap2SecondOuterVertex.y);
                rlVertex2f(cap2FirstOuterVertex.x, cap2FirstOuterVertex.y);

                if (capsIntersect)
                {
                    // Cap 1
                    rlVertex2f(cap1SecondInnerVertex.x, cap1SecondInnerVertex.y);
                    rlVertex2f(cap1SecondOuterVertex.x, cap1SecondOuterVertex.y);
                    rlVertex2f(capIntersectionVertex.x, capIntersectionVertex.y);

                    // Cap 2
                    rlVertex2f(cap2SecondInnerVertex.x, cap2SecondInnerVertex.y);
                    rlVertex2f(capIntersectionVertex.x, capIntersectionVertex.y);
                    rlVertex2f(cap2SecondOuterVertex.x, cap2SecondOuterVertex.y);
                }
            }
        }

    rlEnd();
#endif
}

//----------------------------------------------------------------------------------
// Module Functions Definition - Splines functions
//----------------------------------------------------------------------------------

// Draw spline: linear, minimum 2 points
void DrawSplineLinear(const vec2 *points, const i32 pointCount, const f32 thick, const Color color)
{
    if (pointCount < 2) return;

#if SUPPORT_SPLINE_MITERS
    auto prevNormal = vec2{-(points[1].y - points[0].y), (points[1].x - points[0].x)};
    f32 prevLength = sqrtf(prevNormal.x*prevNormal.x + prevNormal.y*prevNormal.y);

    if (prevLength > 0.0f)
    {
        prevNormal.x /= prevLength;
        prevNormal.y /= prevLength;
    }
    else
    {
        prevNormal.x = 0.0f;
        prevNormal.y = 0.0f;
    }

    auto prevRadius = vec2{ 0.5f*thick*prevNormal.x, 0.5f*thick*prevNormal.y };

    for (i32 i = 0; i < pointCount - 1; i++)
    {
        vec2 normal{};

        if (i < pointCount - 2)
        {
            normal = vec2{-(points[i + 2].y - points[i + 1].y), (points[i + 2].x - points[i + 1].x)};
            f32 normalLength = sqrtf(normal.x*normal.x + normal.y*normal.y);

            if (normalLength > 0.0f)
            {
                normal.x /= normalLength;
                normal.y /= normalLength;
            }
            else
            {
                normal.x = 0.0f;
                normal.y = 0.0f;
            }
        }
        else normal = prevNormal;

        auto radius = vec2{ prevNormal.x + normal.x, prevNormal.y + normal.y };
        f32 radiusLength = sqrtf(radius.x*radius.x + radius.y*radius.y);

        if (radiusLength > 0.0f)
        {
            radius.x /= radiusLength;
            radius.y /= radiusLength;
        }
        else
        {
            radius.x = 0.0f;
            radius.y = 0.0f;
        }

        f32 cosTheta = radius.x*normal.x + radius.y*normal.y;

        if (cosTheta != 0.0f)
        {
            radius.x *= (thick*0.5f/cosTheta);
            radius.y *= (thick*0.5f/cosTheta);
        }
        else
        {
            radius.x = 0.0f;
            radius.y = 0.0f;
        }

        vec2 strip[4] = {
            { points[i].x - prevRadius.x, points[i].y - prevRadius.y },
            { points[i].x + prevRadius.x, points[i].y + prevRadius.y },
            { points[i + 1].x - radius.x, points[i + 1].y - radius.y },
            { points[i + 1].x + radius.x, points[i + 1].y + radius.y }
        };

        DrawTriangleStrip(strip, 4, color);

        prevRadius = radius;
        prevNormal = normal;
    }

#else   // !SUPPORT_SPLINE_MITERS

    vec2 delta{};
    auto length = 0.0f;
    auto scale = 0.0f;

    for (i32 i = 0; i < pointCount - 1; i++)
    {
        delta = vec2{ points[i + 1].x - points[i].x, points[i + 1].y - points[i].y };
        length = sqrtf(delta.x*delta.x + delta.y*delta.y);

        if (length > 0) scale = thick/(2*length);

        const auto radius = vec2{ -scale*delta.y, scale*delta.x };
        vec2 strip[4] = {
            { points[i].x - radius.x, points[i].y - radius.y },
            { points[i].x + radius.x, points[i].y + radius.y },
            { points[i + 1].x - radius.x, points[i + 1].y - radius.y },
            { points[i + 1].x + radius.x, points[i + 1].y + radius.y }
        };

        DrawTriangleStrip(strip, 4, color);
    }
#endif

#if SUPPORT_SPLINE_SEGMENT_CAPS
    // TODO: Add spline segment rounded caps at the begin/end of the spline?
#endif
}

// Draw spline: B-Spline, minimum 4 points
void DrawSplineBasis(const vec2 *points, const i32 pointCount, const f32 thick, const Color color)
{
    if (pointCount < 4) return;

    f32 a[4] = { 0 };
    f32 b[4] = { 0 };
    auto dy = 0.0f;
    auto dx = 0.0f;
    auto size = 0.0f;

    vec2 currentPoint{};
    vec2 nextPoint{};
    vec2 vertices[2*SPLINE_SEGMENT_DIVISIONS + 2]{};

    for (i32 i = 0; i < (pointCount - 3); i++)
    {
        auto t = 0.0f;
        const vec2 p1 = points[i], p2 = points[i + 1], p3 = points[i + 2], p4 = points[i + 3];

        a[0] = (-p1.x + 3.0f*p2.x - 3.0f*p3.x + p4.x)/6.0f;
        a[1] = (3.0f*p1.x - 6.0f*p2.x + 3.0f*p3.x)/6.0f;
        a[2] = (-3.0f*p1.x + 3.0f*p3.x)/6.0f;
        a[3] = (p1.x + 4.0f*p2.x + p3.x)/6.0f;

        b[0] = (-p1.y + 3.0f*p2.y - 3.0f*p3.y + p4.y)/6.0f;
        b[1] = (3.0f*p1.y - 6.0f*p2.y + 3.0f*p3.y)/6.0f;
        b[2] = (-3.0f*p1.y + 3.0f*p3.y)/6.0f;
        b[3] = (p1.y + 4.0f*p2.y + p3.y)/6.0f;

        currentPoint.x = a[3];
        currentPoint.y = b[3];

        if (i == 0) DrawCircleV(currentPoint, thick/2.0f, color);   // Draw init line circle-cap

        if (i > 0)
        {
            vertices[0].x = currentPoint.x + dy*size;
            vertices[0].y = currentPoint.y - dx*size;
            vertices[1].x = currentPoint.x - dy*size;
            vertices[1].y = currentPoint.y + dx*size;
        }

        for (i32 j = 1; j <= SPLINE_SEGMENT_DIVISIONS; j++)
        {
            t = (static_cast<f32>(j))/(static_cast<f32>(SPLINE_SEGMENT_DIVISIONS));

            nextPoint.x = a[3] + t*(a[2] + t*(a[1] + t*a[0]));
            nextPoint.y = b[3] + t*(b[2] + t*(b[1] + t*b[0]));

            dy = nextPoint.y - currentPoint.y;
            dx = nextPoint.x - currentPoint.x;
            size = 0.5f*thick/sqrtf(dx*dx+dy*dy);

            if ((i == 0) && (j == 1))
            {
                vertices[0].x = currentPoint.x + dy*size;
                vertices[0].y = currentPoint.y - dx*size;
                vertices[1].x = currentPoint.x - dy*size;
                vertices[1].y = currentPoint.y + dx*size;
            }

            vertices[2*j + 1].x = nextPoint.x - dy*size;
            vertices[2*j + 1].y = nextPoint.y + dx*size;
            vertices[2*j].x = nextPoint.x + dy*size;
            vertices[2*j].y = nextPoint.y - dx*size;

            currentPoint = nextPoint;
        }

        DrawTriangleStrip(vertices, 2*SPLINE_SEGMENT_DIVISIONS + 2, color);
    }

    // Cap circle drawing at the end of every segment
    DrawCircleV(currentPoint, thick/2.0f, color);
}

// Draw spline: Catmull-Rom, minimum 4 points
void DrawSplineCatmullRom(const vec2 *points, const i32 pointCount, const f32 thick, const Color color)
{
    if (pointCount < 4) return;

    auto dy = 0.0f;
    auto dx = 0.0f;
    auto size = 0.0f;

    vec2 currentPoint = points[1];
    vec2 nextPoint{};
    vec2 vertices[2*SPLINE_SEGMENT_DIVISIONS + 2]{};

    DrawCircleV(currentPoint, thick/2.0f, color);   // Draw init line circle-cap

    for (i32 i = 0; i < (pointCount - 3); i++)
    {
        auto t = 0.0f;
        const vec2 p1 = points[i], p2 = points[i + 1], p3 = points[i + 2], p4 = points[i + 3];

        if (i > 0)
        {
            vertices[0].x = currentPoint.x + dy*size;
            vertices[0].y = currentPoint.y - dx*size;
            vertices[1].x = currentPoint.x - dy*size;
            vertices[1].y = currentPoint.y + dx*size;
        }

        for (i32 j = 1; j <= SPLINE_SEGMENT_DIVISIONS; j++)
        {
            t = (static_cast<f32>(j))/(static_cast<f32>(SPLINE_SEGMENT_DIVISIONS));

            const f32 q0 = (-1.0f*t*t*t) + (2.0f*t*t) + (-1.0f*t);
            const f32 q1 = (3.0f*t*t*t) + (-5.0f*t*t) + 2.0f;
            const f32 q2 = (-3.0f*t*t*t) + (4.0f*t*t) + t;
            const f32 q3 = t*t*t - t*t;

            nextPoint.x = 0.5f*((p1.x*q0) + (p2.x*q1) + (p3.x*q2) + (p4.x*q3));
            nextPoint.y = 0.5f*((p1.y*q0) + (p2.y*q1) + (p3.y*q2) + (p4.y*q3));

            dy = nextPoint.y - currentPoint.y;
            dx = nextPoint.x - currentPoint.x;
            size = (0.5f*thick)/sqrtf(dx*dx + dy*dy);

            if ((i == 0) && (j == 1))
            {
                vertices[0].x = currentPoint.x + dy*size;
                vertices[0].y = currentPoint.y - dx*size;
                vertices[1].x = currentPoint.x - dy*size;
                vertices[1].y = currentPoint.y + dx*size;
            }

            vertices[2*j + 1].x = nextPoint.x - dy*size;
            vertices[2*j + 1].y = nextPoint.y + dx*size;
            vertices[2*j].x = nextPoint.x + dy*size;
            vertices[2*j].y = nextPoint.y - dx*size;

            currentPoint = nextPoint;
        }

        DrawTriangleStrip(vertices, 2*SPLINE_SEGMENT_DIVISIONS + 2, color);
    }

    // Cap circle drawing at the end of every segment
    DrawCircleV(currentPoint, thick/2.0f, color);
}

// Draw spline: Quadratic Bezier, minimum 3 points (1 control point): [p1, c2, p3, c4...]
void DrawSplineBezierQuadratic(const vec2 *points, const i32 pointCount, const f32 thick, const Color color)
{
    if (pointCount >= 3)
    {
        for (i32 i = 0; i < pointCount - 2; i += 2) DrawSplineSegmentBezierQuadratic(points[i], points[i + 1], points[i + 2], thick, color);

        // Cap circle drawing at the end of every segment
        //for (int i = 2; i < pointCount - 2; i += 2) DrawCircleV(points[i], thick/2.0f, color);
    }
}

// Draw spline: Cubic Bezier, minimum 4 points (2 control points): [p1, c2, c3, p4, c5, c6...]
void DrawSplineBezierCubic(const vec2 *points, const i32 pointCount, const f32 thick, const Color color)
{
    if (pointCount >= 4)
    {
        for (i32 i = 0; i < pointCount - 3; i += 3) DrawSplineSegmentBezierCubic(points[i], points[i + 1], points[i + 2], points[i + 3], thick, color);

        // Cap circle drawing at the end of every segment
        //for (int i = 3; i < pointCount - 3; i += 3) DrawCircleV(points[i], thick/2.0f, color);
    }
}

// Draw spline segment: Linear, 2 points
void DrawSplineSegmentLinear(const vec2 p1, const vec2 p2, const f32 thick, const Color color)
{
    // NOTE: For the linear spline no subdivisions are used, only a single quad

    const auto delta = vec2{ p2.x - p1.x, p2.y - p1.y };
    const f32 length = sqrtf(delta.x*delta.x + delta.y*delta.y);

    if ((length > 0) && (thick > 0))
    {
        const f32 scale = thick/(2*length);

        const auto radius = vec2{ -scale*delta.y, scale*delta.x };
        vec2 strip[4] = {
            { p1.x - radius.x, p1.y - radius.y },
            { p1.x + radius.x, p1.y + radius.y },
            { p2.x - radius.x, p2.y - radius.y },
            { p2.x + radius.x, p2.y + radius.y }
        };

        DrawTriangleStrip(strip, 4, color);
    }
}

// Draw spline segment: B-Spline, 4 points
void DrawSplineSegmentBasis(const vec2 p1, const vec2 p2, const vec2 p3, const vec2 p4, const f32 thick, const Color color)
{
    const f32 step = 1.0f/SPLINE_SEGMENT_DIVISIONS;

    vec2 currentPoint{};
    vec2 nextPoint{};
    auto t = 0.0f;

    vec2 points[2*SPLINE_SEGMENT_DIVISIONS + 2]{};

    f32 a[4] = { 0 };
    f32 b[4] = { 0 };

    a[0] = (-p1.x + 3*p2.x - 3*p3.x + p4.x)/6.0f;
    a[1] = (3*p1.x - 6*p2.x + 3*p3.x)/6.0f;
    a[2] = (-3*p1.x + 3*p3.x)/6.0f;
    a[3] = (p1.x + 4*p2.x + p3.x)/6.0f;

    b[0] = (-p1.y + 3*p2.y - 3*p3.y + p4.y)/6.0f;
    b[1] = (3*p1.y - 6*p2.y + 3*p3.y)/6.0f;
    b[2] = (-3*p1.y + 3*p3.y)/6.0f;
    b[3] = (p1.y + 4*p2.y + p3.y)/6.0f;

    currentPoint.x = a[3];
    currentPoint.y = b[3];

    for (i32 i = 0; i <= SPLINE_SEGMENT_DIVISIONS; i++)
    {
        t = step*static_cast<f32>(i);

        nextPoint.x = a[3] + t*(a[2] + t*(a[1] + t*a[0]));
        nextPoint.y = b[3] + t*(b[2] + t*(b[1] + t*b[0]));

        const f32 dy = nextPoint.y - currentPoint.y;
        const f32 dx = nextPoint.x - currentPoint.x;
        const f32 size = (0.5f*thick)/sqrtf(dx*dx + dy*dy);

        if (i == 1)
        {
            points[0].x = currentPoint.x + dy*size;
            points[0].y = currentPoint.y - dx*size;
            points[1].x = currentPoint.x - dy*size;
            points[1].y = currentPoint.y + dx*size;
        }

        points[2*i + 1].x = nextPoint.x - dy*size;
        points[2*i + 1].y = nextPoint.y + dx*size;
        points[2*i].x = nextPoint.x + dy*size;
        points[2*i].y = nextPoint.y - dx*size;

        currentPoint = nextPoint;
    }

    DrawTriangleStrip(points, 2*SPLINE_SEGMENT_DIVISIONS+2, color);
}

// Draw spline segment: Catmull-Rom, 4 points
void DrawSplineSegmentCatmullRom(const vec2 p1, const vec2 p2, const vec2 p3, const vec2 p4, const f32 thick, const Color color)
{
    const f32 step = 1.0f/SPLINE_SEGMENT_DIVISIONS;

    vec2 currentPoint = p1;
    vec2 nextPoint{};
    auto t = 0.0f;

    vec2 points[2*SPLINE_SEGMENT_DIVISIONS + 2]{};

    for (i32 i = 0; i <= SPLINE_SEGMENT_DIVISIONS; i++)
    {
        t = step*static_cast<f32>(i);

        const f32 q0 = (-1*t*t*t) + (2*t*t) + (-1*t);
        const f32 q1 = (3*t*t*t) + (-5*t*t) + 2;
        const f32 q2 = (-3*t*t*t) + (4*t*t) + t;
        const f32 q3 = t*t*t - t*t;

        nextPoint.x = 0.5f*((p1.x*q0) + (p2.x*q1) + (p3.x*q2) + (p4.x*q3));
        nextPoint.y = 0.5f*((p1.y*q0) + (p2.y*q1) + (p3.y*q2) + (p4.y*q3));

        const f32 dy = nextPoint.y - currentPoint.y;
        const f32 dx = nextPoint.x - currentPoint.x;
        const f32 size = (0.5f*thick)/sqrtf(dx*dx + dy*dy);

        if (i == 1)
        {
            points[0].x = currentPoint.x + dy*size;
            points[0].y = currentPoint.y - dx*size;
            points[1].x = currentPoint.x - dy*size;
            points[1].y = currentPoint.y + dx*size;
        }

        points[2*i + 1].x = nextPoint.x - dy*size;
        points[2*i + 1].y = nextPoint.y + dx*size;
        points[2*i].x = nextPoint.x + dy*size;
        points[2*i].y = nextPoint.y - dx*size;

        currentPoint = nextPoint;
    }

    DrawTriangleStrip(points, 2*SPLINE_SEGMENT_DIVISIONS + 2, color);
}

// Draw spline segment: Quadratic Bezier, 2 points, 1 control point
void DrawSplineSegmentBezierQuadratic(const vec2 p1, const vec2 c2, const vec2 p3, const f32 thick, const Color color)
{
    const f32 step = 1.0f/SPLINE_SEGMENT_DIVISIONS;

    vec2 previous = p1;
    vec2 current{};
    auto t = 0.0f;

    vec2 points[2*SPLINE_SEGMENT_DIVISIONS + 2]{};

    for (i32 i = 1; i <= SPLINE_SEGMENT_DIVISIONS; i++)
    {
        t = step*static_cast<f32>(i);

        const f32 a = powf(1.0f - t, 2);
        const f32 b = 2.0f*(1.0f - t)*t;
        const f32 c = powf(t, 2);

        // NOTE: The easing functions aren't suitable here because they don't take a control point
        current.y = a*p1.y + b*c2.y + c*p3.y;
        current.x = a*p1.x + b*c2.x + c*p3.x;

        const f32 dy = current.y - previous.y;
        const f32 dx = current.x - previous.x;
        const f32 size = 0.5f*thick/sqrtf(dx*dx+dy*dy);

        if (i == 1)
        {
            points[0].x = previous.x + dy*size;
            points[0].y = previous.y - dx*size;
            points[1].x = previous.x - dy*size;
            points[1].y = previous.y + dx*size;
        }

        points[2*i + 1].x = current.x - dy*size;
        points[2*i + 1].y = current.y + dx*size;
        points[2*i].x = current.x + dy*size;
        points[2*i].y = current.y - dx*size;

        previous = current;
    }

    DrawTriangleStrip(points, 2*SPLINE_SEGMENT_DIVISIONS + 2, color);
}

// Draw spline segment: Cubic Bezier, 2 points, 2 control points
void DrawSplineSegmentBezierCubic(const vec2 p1, const vec2 c2, const vec2 c3, const vec2 p4, const f32 thick, const Color color)
{
    const f32 step = 1.0f/SPLINE_SEGMENT_DIVISIONS;

    vec2 previous = p1;
    vec2 current{};
    auto t = 0.0f;

    vec2 points[2*SPLINE_SEGMENT_DIVISIONS + 2]{};

    for (i32 i = 1; i <= SPLINE_SEGMENT_DIVISIONS; i++)
    {
        t = step*static_cast<f32>(i);

        const f32 a = powf(1.0f - t, 3);
        const f32 b = 3.0f*powf(1.0f - t, 2)*t;
        const f32 c = 3.0f*(1.0f - t)*powf(t, 2);
        const f32 d = powf(t, 3);

        current.y = a*p1.y + b*c2.y + c*c3.y + d*p4.y;
        current.x = a*p1.x + b*c2.x + c*c3.x + d*p4.x;

        const f32 dy = current.y - previous.y;
        const f32 dx = current.x - previous.x;
        const f32 size = 0.5f*thick/sqrtf(dx*dx+dy*dy);

        if (i == 1)
        {
            points[0].x = previous.x + dy*size;
            points[0].y = previous.y - dx*size;
            points[1].x = previous.x - dy*size;
            points[1].y = previous.y + dx*size;
        }

        points[2*i + 1].x = current.x - dy*size;
        points[2*i + 1].y = current.y + dx*size;
        points[2*i].x = current.x + dy*size;
        points[2*i].y = current.y - dx*size;

        previous = current;
    }

    DrawTriangleStrip(points, 2*SPLINE_SEGMENT_DIVISIONS + 2, color);
}

// Get spline point for a given t [0.0f .. 1.0f], Linear
vec2 GetSplinePointLinear(const vec2 startPos, const vec2 endPos, const f32 t)
{
    vec2 point{};

    point.x = startPos.x*(1.0f - t) + endPos.x*t;
    point.y = startPos.y*(1.0f - t) + endPos.y*t;

    return point;
}

// Get spline point for a given t [0.0f .. 1.0f], B-Spline
vec2 GetSplinePointBasis(const vec2 p1, const vec2 p2, const vec2 p3, const vec2 p4, const f32 t)
{
    vec2 point{};

    f32 a[4] = { 0 };
    f32 b[4] = { 0 };

    a[0] = (-p1.x + 3*p2.x - 3*p3.x + p4.x)/6.0f;
    a[1] = (3*p1.x - 6*p2.x + 3*p3.x)/6.0f;
    a[2] = (-3*p1.x + 3*p3.x)/6.0f;
    a[3] = (p1.x + 4*p2.x + p3.x)/6.0f;

    b[0] = (-p1.y + 3*p2.y - 3*p3.y + p4.y)/6.0f;
    b[1] = (3*p1.y - 6*p2.y + 3*p3.y)/6.0f;
    b[2] = (-3*p1.y + 3*p3.y)/6.0f;
    b[3] = (p1.y + 4*p2.y + p3.y)/6.0f;

    point.x = a[3] + t*(a[2] + t*(a[1] + t*a[0]));
    point.y = b[3] + t*(b[2] + t*(b[1] + t*b[0]));

    return point;
}

// Get spline point for a given t [0.0f .. 1.0f], Catmull-Rom
vec2 GetSplinePointCatmullRom(const vec2 p1, const vec2 p2, const vec2 p3, const vec2 p4, const f32 t)
{
    vec2 point{};

    const f32 q0 = (-1*t*t*t) + (2*t*t) + (-1*t);
    const f32 q1 = (3*t*t*t) + (-5*t*t) + 2;
    const f32 q2 = (-3*t*t*t) + (4*t*t) + t;
    const f32 q3 = t*t*t - t*t;

    point.x = 0.5f*((p1.x*q0) + (p2.x*q1) + (p3.x*q2) + (p4.x*q3));
    point.y = 0.5f*((p1.y*q0) + (p2.y*q1) + (p3.y*q2) + (p4.y*q3));

    return point;
}

// Get spline point for a given t [0.0f .. 1.0f], Quadratic Bezier
vec2 GetSplinePointBezierQuadratic(const vec2 startPos, const vec2 controlPos, const vec2 endPos, const f32 t)
{
    vec2 point{};

    const f32 a = powf(1.0f - t, 2);
    const f32 b = 2.0f*(1.0f - t)*t;
    const f32 c = powf(t, 2);

    point.y = a*startPos.y + b*controlPos.y + c*endPos.y;
    point.x = a*startPos.x + b*controlPos.x + c*endPos.x;

    return point;
}

// Get spline point for a given t [0.0f .. 1.0f], Cubic Bezier
vec2 GetSplinePointBezierCubic(const vec2 startPos, const vec2 startControlPos, const vec2 endControlPos, const vec2 endPos, const f32 t)
{
    vec2 point{};

    const f32 a = powf(1.0f - t, 3);
    const f32 b = 3.0f*powf(1.0f - t, 2)*t;
    const f32 c = 3.0f*(1.0f - t)*powf(t, 2);
    const f32 d = powf(t, 3);

    point.y = a*startPos.y + b*startControlPos.y + c*endControlPos.y + d*endPos.y;
    point.x = a*startPos.x + b*startControlPos.x + c*endControlPos.x + d*endPos.x;

    return point;
}

//----------------------------------------------------------------------------------
// Module Functions Definition - Collision Detection functions
//----------------------------------------------------------------------------------

// Check if point is inside rectangle
bool CheckCollisionPointRec(const vec2 point, const Rectangle rec)
{
    auto collision = false;

    if ((point.x >= rec.x) && (point.x < (rec.x + rec.width)) && (point.y >= rec.y) && (point.y < (rec.y + rec.height))) collision = true;

    return collision;
}

// Check if point is inside circle
bool CheckCollisionPointCircle(const vec2 point, const vec2 center, const f32 radius)
{
    auto collision = false;

    const f32 distanceSquared = (point.x - center.x)*(point.x - center.x) + (point.y - center.y)*(point.y - center.y);

    if (distanceSquared <= radius*radius) collision = true;

    return collision;
}

// Check if point is inside a triangle defined by three points (p1, p2, p3)
bool CheckCollisionPointTriangle(const vec2 point, const vec2 p1, const vec2 p2, const vec2 p3)
{
    auto collision = false;

    const f32 alpha = ((p2.y - p3.y)*(point.x - p3.x) + (p3.x - p2.x)*(point.y - p3.y)) /
                  ((p2.y - p3.y)*(p1.x - p3.x) + (p3.x - p2.x)*(p1.y - p3.y));

    const f32 beta = ((p3.y - p1.y)*(point.x - p3.x) + (p1.x - p3.x)*(point.y - p3.y)) /
                 ((p2.y - p3.y)*(p1.x - p3.x) + (p3.x - p2.x)*(p1.y - p3.y));

    const f32 gamma = 1.0f - alpha - beta;

    if ((alpha > 0) && (beta > 0) && (gamma > 0)) collision = true;

    return collision;
}

// Check if point is within a polygon described by array of vertices
// NOTE: Based on http://jeffreythompson.org/collision-detection/poly-point.php
bool CheckCollisionPointPoly(const vec2 point, const vec2 *points, const i32 pointCount)
{
    auto collision = false;

    if (pointCount > 2)
    {
        for (i32 i = 0, j = pointCount - 1; i < pointCount; j = i++)
        {
            if ((points[i].y > point.y) != (points[j].y > point.y) &&
                (point.x < (points[j].x - points[i].x)*(point.y - points[i].y)/(points[j].y - points[i].y) + points[i].x))
            {
                collision = !collision;
            }
        }
    }

    return collision;
}

// Check collision between two rectangles
bool CheckCollisionRecs(const Rectangle rec1, const Rectangle rec2)
{
    auto collision = false;

    if ((rec1.x < (rec2.x + rec2.width) && (rec1.x + rec1.width) > rec2.x) &&
        (rec1.y < (rec2.y + rec2.height) && (rec1.y + rec1.height) > rec2.y)) collision = true;

    return collision;
}

// Check collision between two circles
bool CheckCollisionCircles(const vec2 center1, const f32 radius1, const vec2 center2, const f32 radius2)
{
    auto collision = false;

    const f32 dx = center2.x - center1.x;      // X distance between centers
    const f32 dy = center2.y - center1.y;      // Y distance between centers

    const f32 distanceSquared = dx*dx + dy*dy; // Distance between centers squared
    const f32 radiusSum = radius1 + radius2;

    collision = (distanceSquared <= (radiusSum*radiusSum));

    return collision;
}

// Check collision between circle and rectangle
// NOTE: Reviewed version to take into account corner limit case
bool CheckCollisionCircleRec(const vec2 center, const f32 radius, const Rectangle rec)
{
    auto collision = false;

    const f32 recCenterX = rec.x + rec.width/2.0f;
    const f32 recCenterY = rec.y + rec.height/2.0f;

    const f32 dx = fabsf(center.x - recCenterX);
    const f32 dy = fabsf(center.y - recCenterY);

    if ((dx <= (rec.width/2.0f + radius)) && (dy <= (rec.height/2.0f + radius)))
    {
        if (dx <= (rec.width/2.0f)) collision = true;
        else if (dy <= (rec.height/2.0f)) collision = true;
        else
        {
            const f32 cornerDistanceSq = (dx - rec.width/2.0f)*(dx - rec.width/2.0f) +
                (dy - rec.height/2.0f)*(dy - rec.height/2.0f);

            collision = (cornerDistanceSq <= (radius*radius));
        }
    }

    return collision;
}

// Check the collision between two lines defined by two points each, returns collision point by reference
// REF: https://en.wikipedia.org/wiki/Line–line_intersection#Given_two_points_on_each_line_segment
bool CheckCollisionLines(const vec2 startPos1, const vec2 endPos1, const vec2 startPos2, const vec2 endPos2, vec2 *collisionPoint)
{
    auto collision = false;

    const f32 rx = endPos1.x - startPos1.x;
    const f32 ry = endPos1.y - startPos1.y;
    const f32 sx = endPos2.x - startPos2.x;
    const f32 sy = endPos2.y - startPos2.y;

    const f32 div = rx*sy - ry*sx;

    if (fabsf(div) >= FLT_EPSILON)
    {
        const f32 s12x = startPos2.x - startPos1.x;
        const f32 s12y = startPos2.y - startPos1.y;

        const f32 t = (s12x*sy - s12y*sx)/div;
        const f32 u = (s12x*ry - s12y*rx)/div;

        if ((0.0f <= t) && (t <= 1.0f) && (0.0f <= u) && (u <= 1.0f))
        {
            if (collisionPoint)
            {
                collisionPoint->x = startPos1.x + t*rx;
                collisionPoint->y = startPos1.y + t*ry;
            }

            collision = true;
        }
    }

    return collision;
}

// Check if point belongs to line created between two points [p1] and [p2] with defined margin in pixels [threshold]
bool CheckCollisionPointLine(const vec2 point, const vec2 p1, const vec2 p2, const i32 threshold)
{
    auto collision = false;

    const f32 dxc = point.x - p1.x;
    const f32 dyc = point.y - p1.y;
    const f32 dxl = p2.x - p1.x;
    const f32 dyl = p2.y - p1.y;
    const f32 cross = dxc*dyl - dyc*dxl;

    if (fabsf(cross) < (threshold*fmaxf(fabsf(dxl), fabsf(dyl))))
    {
        if (fabsf(dxl) >= fabsf(dyl)) collision = (dxl > 0)? ((p1.x <= point.x) && (point.x <= p2.x)) : ((p2.x <= point.x) && (point.x <= p1.x));
        else collision = (dyl > 0)? ((p1.y <= point.y) && (point.y <= p2.y)) : ((p2.y <= point.y) && (point.y <= p1.y));
    }

    return collision;
}

// Check if circle collides with a line created between two points [p1] and [p2]
bool CheckCollisionCircleLine(const vec2 center, const f32 radius, const vec2 p1, const vec2 p2)
{
    auto collision = false;

    const f32 dx = p1.x - p2.x;
    const f32 dy = p1.y - p2.y;

    if ((fabsf(dx) + fabsf(dy)) <= FLT_EPSILON)
    {
        collision = CheckCollisionCircles(p1, 0, center, radius);
    }
    else
    {
        const f32 lengthSQ = ((dx*dx) + (dy*dy));
        f32 dotProduct = (((center.x - p1.x)*(p2.x - p1.x)) + ((center.y - p1.y)*(p2.y - p1.y)))/(lengthSQ);

        if (dotProduct > 1.0f) dotProduct = 1.0f;
        else if (dotProduct < 0.0f) dotProduct = 0.0f;

        const f32 dx2 = (p1.x - (dotProduct*(dx))) - center.x;
        const f32 dy2 = (p1.y - (dotProduct*(dy))) - center.y;
        const f32 distanceSQ = ((dx2*dx2) + (dy2*dy2));

        if (distanceSQ <= radius*radius) collision = true;
    }

    return collision;
}

// Get collision rectangle for two rectangles collision
Rectangle GetCollisionRec(const Rectangle rec1, const Rectangle rec2)
{
    auto overlap = Rectangle{ 0 };

    const f32 left = (rec1.x > rec2.x)? rec1.x : rec2.x;
    const f32 right1 = rec1.x + rec1.width;
    const f32 right2 = rec2.x + rec2.width;
    const f32 right = (right1 < right2)? right1 : right2;
    const f32 top = (rec1.y > rec2.y)? rec1.y : rec2.y;
    const f32 bottom1 = rec1.y + rec1.height;
    const f32 bottom2 = rec2.y + rec2.height;
    const f32 bottom = (bottom1 < bottom2)? bottom1 : bottom2;

    if ((left < right) && (top < bottom))
    {
        overlap.x = left;
        overlap.y = top;
        overlap.width = right - left;
        overlap.height = bottom - top;
    }

    return overlap;
}

//----------------------------------------------------------------------------------
// Module Internal Functions Definition
//----------------------------------------------------------------------------------

// Cubic easing in-out
// NOTE: Used by DrawLineBezier() only
static f32 EaseCubicInOut(f32 t, const f32 b, const f32 c, const f32 d)
{
    auto result = 0.0f;
    t /= 0.5f*d;
    if (t < 1) result = 0.5f*c*t*t*t + b;
    else
    {
        t -= 2;
        result = 0.5f*c*(t*t*t + 2.0f) + b;
    }

    return result;
}

#endif // SUPPORT_MODULE_RSHAPES
