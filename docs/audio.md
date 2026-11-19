# Audio

`AudioDevice` (in `engine::platform`) owns SDL's audio subsystem and the default playback device. With no device (a CI
runner, a machine without sound) the reason is logged once and every call does nothing, so a game runs the same either way.
The decisions it makes live in `audio/mixing.hpp`, which has no SDL and is tested directly.

## Sounds

A `Sound` is float samples, interleaved by channel, with its own `sampleRate` and `channels`.

- `Synthesize(tones)` makes mono 44.1 kHz effects from tones, so a game can have original sounds with no files.
- `LoadSound(path)` (`audio/soundfile.hpp`) reads a `.wav` (through `SDL_LoadWAV`) or an `.ogg` (through the vendored
  stb_vorbis) and keeps the file's own rate and channel count. It returns a `SoundResult`: check `Ok()`, and `error` says
  what was wrong. A missing, empty, truncated or wrong file is an error, never a crash.

Nothing is resampled ahead of time. `AudioDevice::Play` gives the voice's stream the sound's format and SDL converts it to
the device's (a stereo device, so a stereo file keeps its sides and a mono sound plays on both).

## Buses and voices

```cpp
audio.Play(sound, {.bus = Bus::Ui, .volume = 0.8f, .pitchVariance = 0.05f, .priority = 2});
audio.SetBusVolume(Bus::Music, 0.5f);   // live: applied to what is already playing
audio.SetMasterVolume(0.8f);
```

- Three buses, `Music`, `Sfx` and `Ui`, each with a volume from 0 to 1, and a master volume over all of them. A voice's
  gain is its own volume times its bus's times the master (`VoiceGain`), set with `SDL_SetAudioStreamGain`. Volumes outside
  0 to 1, and NaN, are clamped.
- `pitchVariance` of 0.1 plays each sound at a random speed between 0.9 and 1.1 of normal (`SDL_SetAudioStreamFrequencyRatio`),
  so a repeated effect doesn't sound mechanical.
- There are 32 voices. A new sound takes a free voice. If there is none it replaces the oldest of the voices with the lowest
  priority, unless every voice has a higher priority than the new sound, in which case the new sound is dropped
  (`ChooseVoice`).
- Per-sound cooldowns ("don't play the pickup sound twice in 50 ms") are not here: they are a rule about which sounds a game
  plays, so they belong in the game's own table of sounds.

## Music

```cpp
std::string error;
audio.PlayMusic("music/level1.ogg", 1.5f, error);   // fades in over 1.5 seconds; any music playing fades out
audio.PlayMusic("music/boss.ogg", 1.5f, error);     // a crossfade
audio.StopMusic(2.0f);
audio.Update();                                      // once a frame: lets go of tracks that finished fading
```

Music is Ogg Vorbis only. The track reads the compressed file into memory and decodes it a few thousand samples at a time
inside SDL's audio callback, on the audio thread, so a slow frame on the main thread can't starve it. At the end of the file
the decoder goes back to the start within the same buffer, so the loop has no gap (a file with encoder padding at its ends
can still have a tiny seam; trim it when exporting).

Each track fades sample by sample on the audio thread along equal-power curves (`EqualPowerFade`): the one going out follows
`cos` and the one coming in `sin`, so over a crossfade the loudness stays level and neither end clicks. `PlayMusic` of the
music already playing does nothing. A file that can't be played returns false with the reason, and the old music carries on.
The Music bus volume applies to all tracks, fading ones included.

## Checking by ear and by file

The tests cover the choices (`tests/audio/mixing_tests.cpp`), the loaders (`soundfile_tests.cpp`) and that the device takes
every call without crashing, on SDL's dummy audio driver (`audiodevice_tests.cpp`). To check what comes out, record the
mix: run a small program with `SDL_AUDIO_DRIVER=disk`, which writes the device's output to `sdlaudio.raw` (32-bit float,
stereo, 44.1 kHz), and look at it. A 1.5 s crossfade between a 440 Hz and an 880 Hz tone gave a constant RMS through the
fade, and the largest step between samples matched the sine's own slope, so no click.
