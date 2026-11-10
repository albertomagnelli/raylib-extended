# raylib-extended C++ Style Guide

Locked decisions for the C→C++ migration. Applies to `src/`, `examples/` and `extensions/`.

## Language & build
- Library, examples and extensions compile as **C++20 or later** (the top-level CMake project sets C++23, standalone builds fall back to C++20). The bundled GLFW sources stay **C**.
- `raylib.h` is a **C++ only** header: it requires C++17 or later and exposes glm types. `raymath.h`,
  `rcamera.h` and `rlgl.h` follow the same rule (they can still be used standalone, but as C++).
- The exported API keeps the `extern "C"` ABI block. `raymath_impl.c` no longer exists: the math is
  header-only glm code, compiled into each translation unit that needs it.
- Vendored `src/external/*` compile as C++ with warning fixes only — no rewrites.

## Names
- Global PascalCase API stays: `DrawTexture()`, `Vector3`, `rlEnableDepthTest()` … no namespaces, no classes-as-API.
- Plain `enum`s stay (not `enum class`).

## Types & math
- The math types are glm: `Vector2 = glm::vec2`, `Vector3 = glm::vec3`, `Vector4 = glm::vec4`,
  `Quaternion = glm::quat`, `Matrix = glm::mat4`. `Color` and `Rectangle` are plain structs with
  constructors (defaulted components keep the old C partial-init semantics: `Color c = { 255 }`).
- New code uses glm vocabulary directly: `a + b`, `glm::normalize(v)`, `glm::cross(a, b)`,
  `glm::lookAt(...)`, `q * v`, `m * v`, `glm::inverse(m)`.
- `raymath.h` is a small residue of helpers that glm does not express directly: raylib's Euler
  conventions (`MatrixRotateXYZ/ZYX`, `QuaternionFromEuler/ToEuler`), `MoveTowards`,
  `OrthoNormalize`, `Project/Reject`, `Barycenter`, `Unproject`, `CubicHermite`, `MatrixCompose/
  Decompose`, `QuaternionNlerp`, 2D helpers (`Vector2Rotate`, `Vector2Angle`, …) and the
  `float3`/`float16` upload buffers.
- **Matrix layout**: matrices are column-major and multiply column vectors. `glm::mat4` and raylib's
  former `Matrix` describe the same transform, so `mN` (old field) maps to `mat[N/4][N%4]`.
- **Multiplication order**: raylib's `MatrixMultiply(A, B)` means "apply A, then B", i.e. glm
  `B * A`. Do not translate it to `A * B`.
- Quaternions are glm `(w, x, y, z)`. glTF/binary data is `(x, y, z, w)` and must be reordered on load.
- No compound literals in `.cpp` files: `(Vector2){ 1, 2 }` → `Vector2{ 1, 2 }`.
  `CLITERAL(Color){ ... }` still expands to `Color{ ... }` for the color macros.

## Memory & resources
- Internal buffers: `std::vector` / `std::array` instead of `RL_MALLOC/RL_CALLOC/RL_FREE`.
- RAII handles (move-only, deleted copies) for `Image`, `Texture2D`, `RenderTexture2D`, `Shader`,
  `Mesh`, `Model`, `Font`, `Wave`, `Sound`, `Music`, `AudioStream` — **next phase**, after style review.
  `Model` owns its `Mesh` list; copies deleted, moves transfer ownership.
- Text APIs are C++: `std::string_view` parameters, `std::string` return values and
  `std::vector<std::string>` for lists (`LoadTextLines`, `TextSplit`, `FilePathList`).
  `TextFormat`/`TraceLog` keep their printf-style variadic signature, so `std::string`
  arguments passed through them need `.c_str()`.

## Examples
- `int main()` (no `(void)`), constructor-init for structs, `Vector2{}` literals,
  `const` locals kept as-is. One file per example, `.cpp` extension.
- Text is passed as string literals or `std::string`; APIs accept `std::string_view`, so no
  `.c_str()` is needed for normal calls. Only varargs (`TextFormat`, `TraceLog`) need it.
- Array arguments that used C compound literals (`(float[4]){ ... }`) become
  `std::initializer_list<float>{ ... }.begin()`, which keeps the temporary alive for the call.

## Deliberately NOT done
- No `raylib::` namespace, no class-based API, no `enum class`, no exceptions for resource loading
  (raylib returns zero-initialized handles on failure).
- The legacy `src/Makefile` is not maintained; CMake is the supported build.
