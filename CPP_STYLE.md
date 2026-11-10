# raylib-extended C++ Style Guide

Locked decisions for the C→C++ migration. Applies to `src/` modules and `examples/`.

## Language & build
- Library and examples compile as **C++23** (CMake); `rglfw.c` and `raymath_impl.c` stay **C** on purpose.
- Public headers (`raylib.h`, `raymath.h`, `rlgl.h`, `rcamera.h`, `rgestures.h`) stay **C/C++ polyglot**
  (`#if defined(__cplusplus)` dual-mode) while `.c` examples exist, and keep the `extern "C"` ABI.
- Vendored `src/external/*` compile as C++ with warning fixes only — no rewrites.

## Names
- Global PascalCase API stays: `DrawTexture()`, `Vector3`, `rlEnableDepthTest()` … no namespaces, no classes-as-API.
- Plain `enum`s stay (not `enum class`).

## Types & math
- `Vector2/3/4`, `Color`, `Rectangle` have constructors with **defaulted components**
  (`Vector2 v = { 0 }` → `{ 0, 0 }`, exactly the old C partial-init semantics). `Matrix` stays a pure aggregate.
- Binary/comparison operators come from **raymath.h** (`a + b`, `m1 * m2`, `2.0f * v`);
  raylib.h provides unary `-` and compound assignment (`+= -= *= /=`) plus `==`/`!=` for `Color`/`Rectangle`.
  Named functions (`Vector3Normalize()` …) are kept.
- In C++ TUs raymath functions are `static inline` (header-only); the exported C ABI symbols
  are emitted once by `src/raymath_impl.c` (compiled as C).
- No compound literals in `.cpp` files: `(Vector2){ 1, 2 }` → `Vector2{ 1, 2 }`.
  Headers keep `CLITERAL()`/compound literals where C compatibility requires it.

## Memory & resources
- Internal buffers: `std::vector` / `std::array` instead of `RL_MALLOC/RL_CALLOC/RL_FREE`.
- RAII handles (move-only, deleted copies) for `Image`, `Texture2D`, `RenderTexture2D`, `Shader`,
  `Mesh`, `Model`, `Font`, `Wave`, `Sound`, `Music`, `AudioStream` — **next phase**, after style review.
  `Model` owns its `Mesh` list; copies deleted, moves transfer ownership.
- Text APIs: `std::string_view` parameters; `TextFormat` stays printf-style for now.

## Examples
- `int main()` (no `(void)`), constructor-init for structs, `Vector2{}` literals,
  `const` locals kept as-is. One file per example, `.cpp` extension.

## Deliberately NOT done
- No `raylib::` namespace, no class-based API, no `enum class`, no exceptions for resource loading
  (raylib returns zero-initialized handles on failure).
- `src/Makefile` and `projects/` still reference `.c` names (CMake-only migration).
