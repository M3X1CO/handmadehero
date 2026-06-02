#include "sound.h"

#if HANDMADE_GAME_LAYER
internal void
GameOutputSound(game_state *GameState, game_sound_output_buffer *SoundBuffer, int ToneHz)
{
    int16 ToneVolume = 3000;
    int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

    int16 *SampleOut = SoundBuffer->Samples;
    for(int SampleIndex = 0;
        SampleIndex < SoundBuffer->SampleCount;
        ++SampleIndex)
    {
#if 1
        real32 SineValue = sinf(GameState->tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
#else
        int16 SampleValue = 0;
#endif
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

        GameState->tSine += 2.0f * Pi32 * 1.0f / (real32)WavePeriod;
        if(GameState->tSine > 2.0f * Pi32)
        {
            GameState->tSine -= 2.0f * Pi32;
        }
    }
}
#endif

#if HANDMADE_PLATFORM_LAYER
internal bool32
Win32LoadSoundModule(void)
{
    OutputDebugStringA("sound loaded\n");
    return(true);
}

internal void
Win32UnloadSoundModule(void)
{
    OutputDebugStringA("sound unloaded\n");
}

internal bool32
Win32SoundIsReady(win32_sound_state *SoundState)
{
    return(SoundState->Samples != 0);
}

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    if(DSoundLibrary)
    {
        direct_sound_create *DirectSoundCreate =
            (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        LPDIRECTSOUND DirectSound;
        if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;

            if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC PrimaryDesc = {};
                PrimaryDesc.dwSize = sizeof(PrimaryDesc);
                PrimaryDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;

                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if(SUCCEEDED(DirectSound->CreateSoundBuffer(&PrimaryDesc, &PrimaryBuffer, 0)))
                {
                    if(SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
                    {
                        OutputDebugStringA("Primary buffer format was set.\n");
                    }
                }
            }

            DSBUFFERDESC SecondaryDesc = {};
            SecondaryDesc.dwSize = sizeof(SecondaryDesc);
            SecondaryDesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
            SecondaryDesc.dwBufferBytes = BufferSize;
            SecondaryDesc.lpwfxFormat = &WaveFormat;
            if(SUCCEEDED(DirectSound->CreateSoundBuffer(&SecondaryDesc, &GlobalSecondaryBuffer, 0)))
            {
                OutputDebugStringA("Secondary buffer created successfully.\n");
            }
        }
    }
}

internal void
Win32ClearBuffer(win32_sound_output *SoundOutput)
{
    VOID *Region1; DWORD Region1Size;
    VOID *Region2; DWORD Region2Size;
    if(SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
                                             &Region1, &Region1Size,
                                             &Region2, &Region2Size, 0)))
    {
        uint8 *Dest = (uint8 *)Region1;
        for(DWORD i = 0; i < Region1Size; ++i) { *Dest++ = 0; }
        Dest = (uint8 *)Region2;
        for(DWORD i = 0; i < Region2Size; ++i) { *Dest++ = 0; }
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite,
                     game_sound_output_buffer *SourceBuffer)
{
    VOID *Region1; DWORD Region1Size;
    VOID *Region2; DWORD Region2Size;
    if(SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
                                             &Region1, &Region1Size,
                                             &Region2, &Region2Size, 0)))
    {
        DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        int16 *DestSample = (int16 *)Region1;
        int16 *SourceSample = SourceBuffer->Samples;

        for(DWORD i = 0; i < Region1SampleCount; ++i)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }

        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        DestSample = (int16 *)Region2;
        for(DWORD i = 0; i < Region2SampleCount; ++i)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }

        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32InitializeSound(HWND Window, real32 GameUpdateHz, win32_sound_state *SoundState)
{
    *SoundState = {};
    win32_sound_output *SoundOutput = &SoundState->Output;
    *SoundOutput = {};
    SoundOutput->SamplesPerSecond = 48000;
    SoundOutput->BytesPerSample = sizeof(int16) * 2;
    SoundOutput->SecondaryBufferSize = SoundOutput->SamplesPerSecond * SoundOutput->BytesPerSample;
    SoundOutput->SafetyBytes =
        (int)(((real32)SoundOutput->SamplesPerSecond * (real32)SoundOutput->BytesPerSample / GameUpdateHz) / 3.0f);

    Win32InitDSound(Window, SoundOutput->SamplesPerSecond, SoundOutput->SecondaryBufferSize);
    Win32ClearBuffer(SoundOutput);
    GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

    SoundState->Samples = (int16 *)VirtualAlloc(0, SoundOutput->SecondaryBufferSize,
                                                MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
}

internal void
Win32UpdateSound(thread_context *Thread, game_memory *GameMemory, win32_game_code *Game,
                 win32_sound_state *SoundState,
                 real32 GameUpdateHz, real32 TargetSecondsPerFrame,
                 real32 FromBeginToAudioSeconds)
{
    win32_sound_output *SoundOutput = &SoundState->Output;

    DWORD PlayCursor;
    DWORD WriteCursor;
    if(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
    {
        if(!SoundState->IsValid)
        {
            SoundOutput->RunningSampleIndex = WriteCursor / SoundOutput->BytesPerSample;
            SoundState->IsValid = true;
        }

        DWORD ByteToLock = (SoundOutput->RunningSampleIndex * SoundOutput->BytesPerSample)
                         % SoundOutput->SecondaryBufferSize;

        DWORD ExpectedSoundBytesPerFrame =
            (int)((real32)(SoundOutput->SamplesPerSecond * SoundOutput->BytesPerSample) / GameUpdateHz);

        real32 SecondsLeftUntilFlip = TargetSecondsPerFrame - FromBeginToAudioSeconds;
        DWORD ExpectedBytesUntilFlip =
            (DWORD)((SecondsLeftUntilFlip / TargetSecondsPerFrame)
                    * (real32)ExpectedSoundBytesPerFrame);

        DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedBytesUntilFlip;
        DWORD SafeWriteCursor = WriteCursor;
        if(SafeWriteCursor < PlayCursor) { SafeWriteCursor += SoundOutput->SecondaryBufferSize; }
        SafeWriteCursor += SoundOutput->SafetyBytes;

        bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);
        DWORD TargetCursor;
        if(AudioCardIsLowLatency)
        {
            TargetCursor = ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame;
        }
        else
        {
            TargetCursor = WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput->SafetyBytes;
        }
        TargetCursor %= SoundOutput->SecondaryBufferSize;

        DWORD BytesToWrite;
        if(ByteToLock > TargetCursor)
        {
            BytesToWrite = SoundOutput->SecondaryBufferSize - ByteToLock;
            BytesToWrite += TargetCursor;
        }
        else
        {
            BytesToWrite = TargetCursor - ByteToLock;
        }

        game_sound_output_buffer SoundBuffer = {};
        SoundBuffer.SamplesPerSecond = SoundOutput->SamplesPerSecond;
        SoundBuffer.SampleCount = BytesToWrite / SoundOutput->BytesPerSample;
        SoundBuffer.Samples = SoundState->Samples;
        if(Game->GetSoundSamples)
        {
            Game->GetSoundSamples(Thread, GameMemory, &SoundBuffer);
        }

#if HANDMADE_INTERNAL
        {
            win32_sound_debug_state *DebugState = &SoundState->Debug;
            Assert(DebugState->MarkerIndex < ArrayCount(DebugState->Markers));
            win32_debug_time_marker *DebugMarker = &DebugState->Markers[DebugState->MarkerIndex];
            DebugMarker->OutputPlayCursor = PlayCursor;
            DebugMarker->OutputWriteCursor = WriteCursor;
            DebugMarker->OutputLocation = ByteToLock;
            DebugMarker->OutputByteCount = BytesToWrite;
            DebugMarker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;
        }
#endif

        DWORD UnwrappedWriteCursor = WriteCursor;
        if(UnwrappedWriteCursor < PlayCursor) { UnwrappedWriteCursor += SoundOutput->SecondaryBufferSize; }
        SoundState->AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
        SoundState->AudioLatencySeconds = ((real32)SoundState->AudioLatencyBytes / (real32)SoundOutput->BytesPerSample)
                                        / (real32)SoundOutput->SamplesPerSecond;

        Win32FillSoundBuffer(SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
    }
    else
    {
        SoundState->IsValid = false;
    }
}

internal void
Win32RecordSoundDebugFlip(win32_sound_state *SoundState)
{
#if HANDMADE_INTERNAL
    if(SoundState)
    {
        win32_sound_debug_state *DebugState = &SoundState->Debug;
        DWORD PlayCursor;
        DWORD WriteCursor;
        if(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
        {
            Assert(DebugState->MarkerIndex < ArrayCount(DebugState->Markers));
            win32_debug_time_marker *Marker = &DebugState->Markers[DebugState->MarkerIndex];
            Marker->FlipPlayCursor = PlayCursor;
            Marker->FlipWriteCursor = WriteCursor;
        }
    }
#endif
}

internal void
Win32AdvanceSoundDebugMarker(win32_sound_state *SoundState)
{
#if HANDMADE_INTERNAL
    if(SoundState)
    {
        win32_sound_debug_state *DebugState = &SoundState->Debug;
        ++DebugState->MarkerIndex;
        if(DebugState->MarkerIndex == ArrayCount(DebugState->Markers))
        {
            DebugState->MarkerIndex = 0;
        }
    }
#endif
}
#endif
