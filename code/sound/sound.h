#if !defined(SOUND_H)
#define SOUND_H

#if HANDMADE_GAME_LAYER
internal void
GameOutputSound(game_state *GameState, game_sound_output_buffer *SoundBuffer, int ToneHz);
#endif

#if HANDMADE_PLATFORM_LAYER
struct win32_sound_debug_state
{
    int MarkerIndex;
    win32_debug_time_marker Markers[30];
};

struct win32_sound_state
{
    win32_sound_output Output;
    int16 *Samples;
    DWORD AudioLatencyBytes;
    real32 AudioLatencySeconds;
    bool32 IsValid;
    win32_sound_debug_state Debug;
};

internal bool32 Win32LoadSoundModule(void);
internal void Win32UnloadSoundModule(void);
internal bool32 Win32SoundIsReady(win32_sound_state *SoundState);
internal void Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize);
internal void Win32ClearBuffer(win32_sound_output *SoundOutput);
internal void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite,
                                   game_sound_output_buffer *SourceBuffer);
internal void Win32InitializeSound(HWND Window, real32 GameUpdateHz, win32_sound_state *SoundState);
internal void Win32UpdateSound(thread_context *Thread, game_memory *GameMemory, win32_game_code *Game,
                               win32_sound_state *SoundState,
                               real32 GameUpdateHz, real32 TargetSecondsPerFrame,
                               real32 FromBeginToAudioSeconds);
internal void Win32RecordSoundDebugFlip(win32_sound_state *SoundState);
internal void Win32AdvanceSoundDebugMarker(win32_sound_state *SoundState);
#endif

#endif
