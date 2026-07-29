# cmake/webfont.cmake
#
# TheSuperHackers @build githubawn 29/07/2026 A font for the web build's text.
#
# On non-Apple Unix the engine resolves fonts through Fontconfig, which asks the
# operating system where its fonts live. A browser has no such thing: the wasm
# filesystem starts empty, so every string would render blank. The web build
# therefore embeds one font in the module and points FreeType straight at it
# (see render2dsentence.cpp, __EMSCRIPTEN__ branch).
#
# Fetched from upstream at configure time rather than checked in, pinned by
# release URL and hash. DejaVu is the maintained descendant of Bitstream Vera and
# ships under a permissive license (see LICENSE inside the archive). Droid Sans,
# the font the original web bring-up used, has been retired by its publisher and
# no longer has an upstream release to pull from.
#
# Sets GGC_WEB_FONT_REGULAR and GGC_WEB_FONT_BOLD to the two .ttf files.

include(FetchContent)

FetchContent_Declare(
    ggc_web_font
    URL      "https://github.com/dejavu-fonts/dejavu-fonts/releases/download/version_2_37/dejavu-fonts-ttf-2.37.zip"
    URL_HASH SHA256=7576310b219e04159d35ff61dd4a4ec4cdba4f35c00e002a136f00e96a908b0a
)

FetchContent_MakeAvailable(ggc_web_font)

set(GGC_WEB_FONT_REGULAR "${ggc_web_font_SOURCE_DIR}/ttf/DejaVuSans.ttf" CACHE INTERNAL "")
set(GGC_WEB_FONT_BOLD    "${ggc_web_font_SOURCE_DIR}/ttf/DejaVuSans-Bold.ttf" CACHE INTERNAL "")
set(GGC_WEB_FONT_LICENSE "${ggc_web_font_SOURCE_DIR}/LICENSE" CACHE INTERNAL "")

foreach(_font "${GGC_WEB_FONT_REGULAR}" "${GGC_WEB_FONT_BOLD}")
    if(NOT EXISTS "${_font}")
        message(FATAL_ERROR "webfont: expected ${_font} in the fetched font archive.")
    endif()
endforeach()
