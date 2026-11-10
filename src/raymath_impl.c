/**********************************************************************************************
*
*   raymath implementation translation unit
*
*   raymath.h is compiled as C in this translation unit on purpose:
*   the C99 'extern inline' semantics of RAYMATH_IMPLEMENTATION force GCC to emit
*   out-of-line external definitions for ALL raymath functions into the library.
*   In C++ 'extern inline' is not available (plain 'inline' instead), so functions
*   unused inside the library's own C++ translation units would not be emitted
*   and C (or precompiled) consumers could not link against them.
*
*   LICENSE: zlib/libpng
*
*   Copyright (c) 2013-2026 Ramon Santamaria (@raysan5)
*
**********************************************************************************************/

#include "raylib.h"                 // Required for: Vector2, Vector3, Vector4, Quaternion, Matrix, Color, Rectangle types

#define RAYMATH_IMPLEMENTATION
#ifdef RAYMATH_STATIC_INLINE
    #undef RAYMATH_STATIC_INLINE  // This translation unit must emit the external out-of-line definitions
#endif
#include "raymath.h"                // Vector2, Vector3, Quaternion and Matrix functionality
