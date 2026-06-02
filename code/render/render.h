#if !defined(RENDER_H)
#define RENDER_H

#if HANDMADE_GAME_LAYER
internal void
RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset);

internal void
RenderPlayer(game_offscreen_buffer *Buffer, int PlayerX, int PlayerY);
#endif

#if HANDMADE_PLATFORM_LAYER
struct win32_frame_timing
{
    int MonitorRefreshHz;
    real32 GameUpdateHz;
    real32 TargetSecondsPerFrame;
};

internal bool32 Win32LoadRenderModule(void);
internal void Win32UnloadRenderModule(void);
inline LARGE_INTEGER Win32GetWallClock(void);
inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End);
internal win32_frame_timing Win32GetFrameTiming(HWND Window);
internal win32_window_dimension Win32GetWindowDimension(HWND Window);
internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height);
internal game_offscreen_buffer Win32GetGameOffscreenBuffer(win32_offscreen_buffer *Buffer);
internal void Win32PaceFrame(LARGE_INTEGER LastCounter, real32 TargetSecondsPerFrame, bool32 SleepIsGranular);
internal void Win32DisplayFrame(HWND Window, LARGE_INTEGER *LastCounter, LARGE_INTEGER *FlipWallClock);
internal void Win32PaintWindow(HWND Window);
internal void Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer,
                                         HDC DeviceContext, int WindowWidth, int WindowHeight);
#endif

#endif
