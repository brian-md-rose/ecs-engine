# stb_vorbis (Sean Barrett, public domain or MIT): decodes Ogg Vorbis files. One C file, vendored as
# downloaded from https://github.com/nothings/stb. The licence is at the end of stb_vorbis.c.
#
# Deliberately NOT linked to engine::warnings: it is someone else's C code, and the engine's
# warnings-as-errors flags, analysis and sanitizer instrumentation don't belong on it. (The sanitizers
# still see it when it is linked into a test, which is what we want for the data it is given.)
enable_language(C)

add_library(stb_vorbis STATIC stb_vorbis.c)
add_library(stb_vorbis::stb_vorbis ALIAS stb_vorbis)
# The engine reads files itself and hands stb_vorbis memory, so it needs no stdio.
target_compile_definitions(stb_vorbis PUBLIC STB_VORBIS_NO_STDIO)
target_include_directories(stb_vorbis SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
    target_compile_options(stb_vorbis PRIVATE /W0)
else()
    target_compile_options(stb_vorbis PRIVATE -w)
endif()
