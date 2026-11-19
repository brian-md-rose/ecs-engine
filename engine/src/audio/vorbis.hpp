#pragma once
// stb_vorbis is C: this includes its declarations (not its code, which is in the stb_vorbis library) for the
// engine's own .cpp files. Private to engine/src/audio.
extern "C"
{
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c" // NOLINT(bugprone-suspicious-include): declarations only, see HEADER_ONLY above
}
