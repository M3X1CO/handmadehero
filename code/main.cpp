/* ========================================================================
   main.cpp - Win32 platform layer.

   Responsibilities:
     - Window creation and WndProc
     - Pixel buffer management (DIB section / StretchDIBits)
     - DirectSound initialisation and ring-buffer fill
     - XInput controller polling
     - Keyboard and mouse input processing
     - Game DLL hot-reload
     - Input record/playback
     - Fixed-timestep loop with Sleep-based frame pacing

   TODO(felix): Outstanding work before this is a "real" platform layer:
     - Saved game locations
     - Handle to our own executable
     - Asset loading paths
     - Worker thread launch
     - Raw Input (multi-keyboard support)
     - Sleep/timeBeginPeriod tuning
     - ClipCursor() for multi-monitor
     - Fullscreen toggle
     - WM_SETCURSOR (hide/show cursor)
     - QueryCancelAutoplay
     - WM_ACTIVATEAPP (dim window when not focused)
     - BitBlt speed investigation
     - OpenGL / Direct3D acceleration
     - GetKeyboardLayout (AZERTY / international WASD)
   ======================================================================== */

#include "handmade.h"       // NOTE(felix): Game-side types and DLL contract
#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include <xinput.h>
#include <dsound.h>
#include "win32_handmade.h" // NOTE(felix): Win32-private struct definitions
#define HANDMADE_GAME_LAYER 0
#define HANDMADE_PLATFORM_LAYER 1
#include "hot_reload/hot_reload.h"
#include "file_io/file_io.h"
#include "sound/sound.h"
#include "render/render.h"
#include "recording/recording.h"
#include "input/input.h"
#include "game_lobby/game_lobby.h"
#include "launcher/launcher.h"
#include "login/login.h"
#include "logout/logout.h"

/* -----------------------------------------------------------------------
   Globals
   Kept to a minimum — only things that must be visible to the WndProc
   or that represent single-instance hardware state.
   ----------------------------------------------------------------------- */
global_variable bool32                 GlobalRunning;           // NOTE(felix): Main loop sentinel
global_variable bool32                 GlobalPause;             // NOTE(felix): Dev-build pause toggle (P key)
global_variable win32_offscreen_buffer GlobalBackbuffer;        // NOTE(felix): Single back-buffer, recreated on resize
global_variable LPDIRECTSOUNDBUFFER    GlobalSecondaryBuffer;   // NOTE(felix): DirectSound ring buffer
global_variable int64                  GlobalPerfCountFrequency;// NOTE(felix): QPC ticks per second, cached at startup

#include "hot_reload/hot_reload.c"
#include "file_io/file_io.c"
#include "sound/sound.c"
#include "render/render.c"
#include "recording/recording.c"
#include "input/input.c"
#include "game_lobby/game_lobby.c"
#include "launcher/launcher.c"
#include "login/login.c"
#include "logout/logout.c"
#undef HANDMADE_PLATFORM_LAYER
#undef HANDMADE_GAME_LAYER

/* ========================================================================
   String utilities
   We avoid CRT string.h in the platform layer; these are the only helpers
   we need.
   ======================================================================== */

// NOTE(felix): Appends SourceA (SourceACount chars) then SourceB (SourceBCount chars)
// into Dest, null-terminates. TODO(felix): Add bounds checking on DestCount.
internal void
CatStrings(size_t SourceACount, char *SourceA,
           size_t SourceBCount, char *SourceB,
           size_t DestCount,    char *Dest)
{
    for(int Index = 0; Index < (int)SourceACount; ++Index) { *Dest++ = *SourceA++; }
    for(int Index = 0; Index < (int)SourceBCount; ++Index) { *Dest++ = *SourceB++; }
    *Dest++ = 0;
}

// NOTE(felix): Returns byte-length of null-terminated string (not counting the null).
internal int
StringLength(char *String)
{
    int Count = 0;
    while(*String++) { ++Count; }
    return(Count);
}

/* ========================================================================
   EXE path helpers
   We need sibling paths (handmade.dll, handmade_temp.dll, replay files)
   relative to the EXE so the game works regardless of the working directory.
   ======================================================================== */

// NOTE(felix): Fills State->EXEFileName via GetModuleFileNameA, then scans
// for the last backslash and stores a pointer one past it in
// OnePastLastEXEFileNameSlash. That pointer is used by Win32BuildEXEPathFileName
// to derive the directory prefix length.
internal void
Win32GetEXEFileName(win32_state *State)
{
    // NOTE(felix): Never use MAX_PATH for user-visible paths — it can truncate.
    // Here we control the filename internally so it is safe.
    GetModuleFileNameA(0, State->EXEFileName, sizeof(State->EXEFileName));
    State->OnePastLastEXEFileNameSlash = State->EXEFileName;
    for(char *Scan = State->EXEFileName; *Scan; ++Scan)
    {
        if(*Scan == '\\') { State->OnePastLastEXEFileNameSlash = Scan + 1; }
    }
}

// NOTE(felix): Builds a full path by concatenating:
//   [EXEFileName up to (not including) the leaf name] + FileName
// Result written into Dest (DestCount bytes). Used for DLL and replay paths.
internal void
Win32BuildEXEPathFileName(win32_state *State, char *FileName,
                          int DestCount, char *Dest)
{
    CatStrings(State->OnePastLastEXEFileNameSlash - State->EXEFileName, State->EXEFileName,
               StringLength(FileName), FileName,
               DestCount, Dest);
}


/* ========================================================================
   Window procedure
   ======================================================================== */

// NOTE(felix): Handles the minimal set of Win32 messages needed to keep the
// window alive and respond to close/destroy. Keyboard messages are routed
// through the input module's PeekMessage pass so they carry the
// transition-count bits; if they somehow arrive here we assert-fail.
internal LRESULT CALLBACK
Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;

    switch(Message)
    {
        case WM_CLOSE:
        {
            // TODO(felix): Show "are you sure?" dialog
            GlobalRunning = false;
        } break;

        case WM_ACTIVATEAPP:
        {
            // NOTE(felix): Could dim the window when not focused.
            // Left as dead code for now.
#if 0
            if(WParam == TRUE) { SetLayeredWindowAttributes(Window, RGB(0,0,0), 255, LWA_ALPHA); }
            else               { SetLayeredWindowAttributes(Window, RGB(0,0,0),  64, LWA_ALPHA); }
#endif
        } break;

        case WM_DESTROY:
        {
            // TODO(felix): Recreate window instead of quitting?
            GlobalRunning = false;
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            // NOTE(felix): Keyboard input must only arrive via PeekMessage so we get
            // the extended lParam bits. If it reaches WndProc something is wrong.
            Assert(!"Keyboard input came in through a non-dispatch message!");
        } break;

        case WM_PAINT:
        {
            Win32PaintWindow(Window);
        } break;

        default:
        {
            Result = DefWindowProcA(Window, Message, WParam, LParam);
        } break;
    }

    return(Result);
}

/* ========================================================================
/* ========================================================================


/* ========================================================================
   WinMain — entry point and main game loop
   ======================================================================== */

// NOTE(felix): High-level sequence:
//   1. Cache QPC frequency, resolve EXE paths.
//   2. Set scheduler granularity to 1ms for accurate Sleep().
//   3. Load XInput, register window class, create window.
//   4. Query monitor refresh rate, derive target frame time.
//   5. Init DirectSound, allocate sample scratch buffer.
//   6. VirtualAlloc the combined game memory block (64 MB permanent + 1 GB transient).
//   7. Set up 4 memory-mapped replay buffers.
//   8. Main loop:
//        a. Hot-reload DLL if the file timestamp changed.
//        b. Let input, recording, game, sound, and render modules run one frame.
int CALLBACK
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    win32_state Win32State = {};

    // NOTE(felix): Cache the QPC frequency — one syscall, reused for every timing operation
    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

    // NOTE(felix): Resolve EXE directory so we can build sibling paths
    Win32GetEXEFileName(&Win32State);

    char SourceGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, "handmade.dll",
                              sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);

    char TempGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, "handmade_temp.dll",
                              sizeof(TempGameCodeDLLFullPath), TempGameCodeDLLFullPath);

    // NOTE(felix): Request 1ms scheduler resolution so Sleep(n) sleeps ≈ n ms.
    // The bool records whether the call succeeded so we know if Sleep is safe.
    UINT   DesiredSchedulerMS = 1;
    bool32 SleepIsGranular    = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);

    Win32LoadHotReloadModule();
    Win32LoadGameLobbyModule();
    Win32LoadLauncherModule();
    Win32LoadLoginModule();
    Win32LoadLogoutModule();
    Win32LoadFileIOModule();
    Win32LoadRenderModule();
    Win32LoadSoundModule();
    Win32LoadRecordingModule();

    Win32LoadInputModule();

    // NOTE(felix): Allocate the back-buffer before creating the window so the
    // WM_PAINT handler has a valid buffer on the very first paint.
    Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

    WNDCLASSA WindowClass        = {};
    WindowClass.style            = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc      = Win32MainWindowCallback;
    WindowClass.hInstance        = Instance;
    WindowClass.lpszClassName    = "HandmadeHeroWindowClass";

    if(RegisterClassA(&WindowClass))
    {
        HWND Window = CreateWindowExA(
            0,
            WindowClass.lpszClassName,
            "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT,
            0, 0, Instance, 0);

        if(Window)
        {
            win32_frame_timing FrameTiming = Win32GetFrameTiming(Window);

            win32_sound_state SoundState = {};
            Win32InitializeSound(Window, FrameTiming.GameUpdateHz, &SoundState);

            GlobalRunning = true;

            // NOTE(felix): In INTERNAL builds we put the game memory at a fixed 2 TB
            // address so pointers are reproducible between runs (helps debugging).
#if HANDMADE_INTERNAL
            LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
            LPVOID BaseAddress = 0;
#endif

            // NOTE(felix): Allocate the combined game memory block then split it:
            //   PermanentStorage = [base, base + 64 MB)
            //   TransientStorage = [base + 64 MB, base + 64 MB + 1 GB)
            game_memory GameMemory               = {};
            GameMemory.PermanentStorageSize      = Megabytes(64);
            GameMemory.TransientStorageSize      = Gigabytes(1);
            GameMemory.DEBUGPlatformFreeFileMemory  = DEBUGPlatformFreeFileMemory;
            GameMemory.DEBUGPlatformReadEntireFile  = DEBUGPlatformReadEntireFile;
            GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

            Win32State.TotalSize      = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
            Win32State.GameMemoryBlock = VirtualAlloc(BaseAddress, (size_t)Win32State.TotalSize,
                                                      MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
            GameMemory.TransientStorage = (uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize;

            Win32InitializeReplayBuffers(&Win32State);

            if(Win32SoundIsReady(&SoundState) && GameMemory.PermanentStorage && GameMemory.TransientStorage)
            {
                thread_context Thread = {};

                win32_input_state InputState = {};
                Win32InitializeInputState(&InputState);

                LARGE_INTEGER LastCounter    = Win32GetWallClock();
                LARGE_INTEGER FlipWallClock  = Win32GetWallClock();

                win32_game_code Game       = Win32LoadGameCode(SourceGameCodeDLLFullPath,
                                                               TempGameCodeDLLFullPath);

                while(GlobalRunning)
                {
                    /* ---- Hot-reload check ---- */
                    // NOTE(felix): Compare last-write time of the source DLL;
                    // if changed, unload and reload. LoadCounter resets to 0 each reload.
                    FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
                    if(CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0)
                    {
                        Win32UnloadGameCode(&Game);
                        Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);
                    }

                    /* ---- Input ---- */
                    Win32UpdateInput(&Win32State, Window, &InputState);
                    game_input *NewInput = Win32GetCurrentInput(&InputState);
                    game_offscreen_buffer Buffer = Win32GetGameOffscreenBuffer(&GlobalBackbuffer);

                    if(Win32State.InputRecordingIndex) { Win32RecordInput(&Win32State, NewInput); }
                    if(Win32State.InputPlayingIndex)   { Win32PlayBackInput(&Win32State, NewInput); }

                    if(Game.UpdateAndRender)
                    {
                        Game.UpdateAndRender(&Thread, &GameMemory, NewInput, &Buffer);
                    }

                    LARGE_INTEGER AudioWallClock = Win32GetWallClock();
                    real32 FromBeginToAudioSeconds = Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);
                    Win32UpdateSound(&Thread, &GameMemory, &Game, &SoundState,
                                     FrameTiming.GameUpdateHz, FrameTiming.TargetSecondsPerFrame,
                                     FromBeginToAudioSeconds);

                    Win32PaceFrame(LastCounter, FrameTiming.TargetSecondsPerFrame, SleepIsGranular);
                    Win32DisplayFrame(Window, &LastCounter, &FlipWallClock);

                    Win32RecordSoundDebugFlip(&SoundState);
                    Win32SwapInputBuffers(&InputState);
                    Win32AdvanceSoundDebugMarker(&SoundState);
                } // end while(GlobalRunning)
            }
            // TODO(felix): else Logging (VirtualAlloc failed)
        }
        // TODO(felix): else Logging (CreateWindow failed)
    }
    // TODO(felix): else Logging (RegisterClass failed)

    Win32UnloadRecordingModule();
    Win32UnloadInputModule();
    Win32UnloadSoundModule();
    Win32UnloadRenderModule();
    Win32UnloadFileIOModule();
    Win32UnloadLogoutModule();
    Win32UnloadLoginModule();
    Win32UnloadLauncherModule();
    Win32UnloadGameLobbyModule();
    Win32UnloadHotReloadModule();

    return(0);
}
