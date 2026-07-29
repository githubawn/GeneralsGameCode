# FORCE is required to guarantee cross-platform bit-exact determinism.
# Intrinsics would use platform-specific SIMD, breaking CRC parity between architectures.
set(GM_ENABLE_INTRINSICS OFF CACHE BOOL "Disable intrinsics for cross-arch determinism" FORCE)
set(GM_ENABLE_TESTS OFF CACHE BOOL "Disable GameMath tests" FORCE)

FetchContent_Declare(
    gamemath
    GIT_REPOSITORY https://github.com/TheSuperHackers/GameMath.git
    GIT_TAG        59f7ccd494f7e7c916a784ac26ef266f9f09d78d
)

FetchContent_MakeAvailable(gamemath)

# TheSuperHackers @build githubawn 29/07/2026 WebAssembly has no floating-point
# exception flags, so Emscripten's <fenv.h> defines FE_ALL_EXCEPT as 0 and omits the
# individual FE_* flags entirely. GameMath's musl-derived rounding helpers name those
# flags when they raise inexact/invalid, so define them as the empty set: feraiseexcept
# and fetestexcept are already no-ops there. No effect on the computed results, which
# is what determinism depends on.
if(EMSCRIPTEN AND TARGET gamemath)
    target_compile_definitions(gamemath PRIVATE
        FE_INVALID=0
        FE_DIVBYZERO=0
        FE_OVERFLOW=0
        FE_UNDERFLOW=0
        FE_INEXACT=0
    )
endif()

# Ensure GameMath includes are available to ALL targets
# to prevent one-definition-rule violations and ensure USE_DETERMINISTIC_MATH activates consistently.
include_directories(${gamemath_SOURCE_DIR}/include)
