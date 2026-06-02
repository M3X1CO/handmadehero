#include "render.h"

#if HANDMADE_GAME_LAYER
internal void
RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
    uint8 *Row = (uint8 *)Buffer->Memory;
    for(int Y = 0; Y < Buffer->Height; ++Y)
    {
        uint32 *Pixel = (uint32 *)Row;
        for(int X = 0; X < Buffer->Width; ++X)
        {
            uint8 Blue = (uint8)(X + BlueOffset);
            uint8 Green = (uint8)(Y + GreenOffset);
            *Pixel++ = ((Green << 16) | Blue);
        }
        Row += Buffer->Pitch;
    }
}

internal void
RenderPlayer(game_offscreen_buffer *Buffer, int PlayerX, int PlayerY)
{
    uint8 *EndOfBuffer = (uint8 *)Buffer->Memory + Buffer->Pitch * Buffer->Height;
    uint32 Color = 0xFFFFFFFF;
    int Top = PlayerY;
    int Bottom = PlayerY + 10;

    for(int X = PlayerX; X < PlayerX + 10; ++X)
    {
        uint8 *Pixel = (uint8 *)Buffer->Memory
                     + X * Buffer->BytesPerPixel
                     + Top * Buffer->Pitch;

        for(int Y = Top; Y < Bottom; ++Y)
        {
            if((Pixel >= Buffer->Memory) && ((Pixel + 4) <= EndOfBuffer))
            {
                *(uint32 *)Pixel = Color;
            }
            Pixel += Buffer->Pitch;
        }
    }
}
#endif

#if HANDMADE_PLATFORM_LAYER
internal bool32
Win32LoadRenderModule(void)
{
    OutputDebugStringA("render loaded\n");
    return(true);
}

internal void
Win32UnloadRenderModule(void)
{
    OutputDebugStringA("render unloaded\n");
}

inline LARGE_INTEGER
Win32GetWallClock(void)
{
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return(Result);
}

inline real32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    return (real32)(End.QuadPart - Start.QuadPart) / (real32)GlobalPerfCountFrequency;
}

internal win32_frame_timing
Win32GetFrameTiming(HWND Window)
{
    win32_frame_timing Result = {};
    Result.MonitorRefreshHz = 60;

    HDC RefreshDC = GetDC(Window);
    int Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
    ReleaseDC(Window, RefreshDC);
    if(Win32RefreshRate > 1) { Result.MonitorRefreshHz = Win32RefreshRate; }

    Result.GameUpdateHz = Result.MonitorRefreshHz / 2.0f;
    Result.TargetSecondsPerFrame = 1.0f / Result.GameUpdateHz;
    return(Result);
}

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    win32_window_dimension Result;
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;
    return(Result);
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    if(Buffer->Memory) { VirtualFree(Buffer->Memory, 0, MEM_RELEASE); }

    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->BytesPerPixel = 4;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize = Buffer->Width * Buffer->Height * Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch = Width * Buffer->BytesPerPixel;
}

internal game_offscreen_buffer
Win32GetGameOffscreenBuffer(win32_offscreen_buffer *Buffer)
{
    game_offscreen_buffer Result = {};
    Result.Memory = Buffer->Memory;
    Result.Width = Buffer->Width;
    Result.Height = Buffer->Height;
    Result.Pitch = Buffer->Pitch;
    Result.BytesPerPixel = Buffer->BytesPerPixel;
    return(Result);
}

internal void
Win32PaceFrame(LARGE_INTEGER LastCounter, real32 TargetSecondsPerFrame, bool32 SleepIsGranular)
{
    real32 SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());

    if(SecondsElapsedForFrame < TargetSecondsPerFrame)
    {
        if(SleepIsGranular)
        {
            DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));
            if(SleepMS > 0) { Sleep(SleepMS); }
        }

        while(SecondsElapsedForFrame < TargetSecondsPerFrame)
        {
            SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
        }
    }
}

internal void
Win32DisplayFrame(HWND Window, LARGE_INTEGER *LastCounter, LARGE_INTEGER *FlipWallClock)
{
    LARGE_INTEGER EndCounter = Win32GetWallClock();
    real32 MSPerFrame = 1000.0f * Win32GetSecondsElapsed(*LastCounter, EndCounter);
    *LastCounter = EndCounter;

    win32_window_dimension Dimension = Win32GetWindowDimension(Window);
    HDC DeviceContext = GetDC(Window);
    Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext,
                               Dimension.Width, Dimension.Height);
    ReleaseDC(Window, DeviceContext);
    *FlipWallClock = Win32GetWallClock();
}

internal void
Win32PaintWindow(HWND Window)
{
    PAINTSTRUCT Paint;
    HDC DeviceContext = BeginPaint(Window, &Paint);
    win32_window_dimension Dimension = Win32GetWindowDimension(Window);
    Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext,
                               Dimension.Width, Dimension.Height);
    EndPaint(Window, &Paint);
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer,
                           HDC DeviceContext, int WindowWidth, int WindowHeight)
{
    StretchDIBits(DeviceContext,
                  0, 0, Buffer->Width, Buffer->Height,
                  0, 0, Buffer->Width, Buffer->Height,
                  Buffer->Memory,
                  &Buffer->Info,
                  DIB_RGB_COLORS, SRCCOPY);
}
#endif
