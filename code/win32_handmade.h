#if !defined(WIN32_HANDMADE_H)
/* ========================================================================
   win32_handmade.h — Win32 platform-layer private types.
   Only included by main.cpp (and its sub-modules).
   The game DLL never sees this header.
   ======================================================================== */

/* -----------------------------------------------------------------------
   Win32-side pixel buffer.
   Mirrors game_offscreen_buffer but also carries a BITMAPINFO so we can
   pass it straight to StretchDIBits. The Info header is filled once in
   Win32ResizeDIBSection; the Memory pointer is a VirtualAlloc'd block.
   ----------------------------------------------------------------------- */
struct win32_offscreen_buffer
{
    // NOTE(felix): Pixel layout 0x XX RR GG BB, top-down (biHeight is negative)
    BITMAPINFO Info;
    void      *Memory;
    int        Width;
    int        Height;
    int        Pitch;         // NOTE(felix): Bytes per scanline
    int        BytesPerPixel; // NOTE(felix): Always 4
};

/* -----------------------------------------------------------------------
   Snapshot of the client-rect dimensions, returned by Win32GetWindowDimension.
   Refreshed every time we need to blit so we handle window resize correctly.
   ----------------------------------------------------------------------- */
struct win32_window_dimension
{
    int Width;
    int Height;
};

/* -----------------------------------------------------------------------
   DirectSound output parameters. Held for the lifetime of the session.
   RunningSampleIndex tracks how many stereo samples have been written in
   total so we can compute where to write next in the ring buffer.
   SafetyBytes is extra padding added to the write-cursor target to absorb
   scheduler jitter.
   ----------------------------------------------------------------------- */
struct win32_sound_output
{
    int    SamplesPerSecond;
    uint32 RunningSampleIndex;   // NOTE(felix): Total samples written, wraps via modulo
    int    BytesPerSample;       // NOTE(felix): 2 channels × sizeof(int16) = 4
    DWORD  SecondaryBufferSize;  // NOTE(felix): Full ring buffer size in bytes
    DWORD  SafetyBytes;          // NOTE(felix): Latency guard — see WinMain audio comment
    real32 tSine;                // NOTE(felix): Unused here; kept for symmetry with game_state
};

/* -----------------------------------------------------------------------
   Debug audio timing snapshot.
   One entry captured per frame (up to 30 frames ring-buffered).
   The Output* fields are captured before we fill the buffer; the Flip*
   fields are captured after StretchDIBits so we can correlate audio
   latency with the display flip.
   ----------------------------------------------------------------------- */
struct win32_debug_time_marker
{
    DWORD OutputPlayCursor;
    DWORD OutputWriteCursor;
    DWORD OutputLocation;          // NOTE(felix): ByteToLock we chose this frame
    DWORD OutputByteCount;         // NOTE(felix): BytesToWrite we chose this frame
    DWORD ExpectedFlipPlayCursor;  // NOTE(felix): Where play cursor should be on next flip

    DWORD FlipPlayCursor;          // NOTE(felix): Actual play cursor at flip time
    DWORD FlipWriteCursor;
};

/* -----------------------------------------------------------------------
   Hot-reload game DLL handle.
   We copy the DLL to a temp path before loading so the linker can still
   write to the original. IsValid is false if either function pointer is
   null (load failed or DLL was unloaded). Always check IsValid before calling.
   ----------------------------------------------------------------------- */
struct win32_game_code
{
    HMODULE              GameCodeDLL;
    FILETIME             DLLLastWriteTime;   // NOTE(felix): Used to detect recompile

    // NOTE(felix): EITHER pointer can be null — always check IsValid before calling!
    game_update_and_render  *UpdateAndRender;
    game_get_sound_samples  *GetSoundSamples;

    bool32 IsValid;
};

/* -----------------------------------------------------------------------
   Input record/playback buffers.
   One buffer per replay slot (4 slots). Each slot memory-maps a file on
   disk so we can snapshot the full game memory (64 MB + 1 GB) cheaply by
   mapping, then CopyMemory into the mapped region.
   ----------------------------------------------------------------------- */
#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH

struct win32_replay_buffer
{
    HANDLE  FileHandle;
    HANDLE  MemoryMap;                             // NOTE(felix): Win32 file-mapping object
    char    FileName[WIN32_STATE_FILE_NAME_COUNT]; // NOTE(felix): loop_edit_N_state.hmi
    void   *MemoryBlock;                           // NOTE(felix): MapViewOfFile result
};

/* -----------------------------------------------------------------------
   Top-level Win32 platform state. One instance on the WinMain stack.
   GameMemoryBlock is the single VirtualAlloc that backs both Permanent and
   Transient storage. EXEFileName is used to build sibling paths (DLL, temp
   DLL, replay files) relative to the executable location.
   ----------------------------------------------------------------------- */
struct win32_state
{
    uint64              TotalSize;              // NOTE(felix): PermanentSize + TransientSize
    void               *GameMemoryBlock;        // NOTE(felix): Base of the combined allocation
    win32_replay_buffer ReplayBuffers[4];

    HANDLE RecordingHandle;        // NOTE(felix): File handle while recording input
    int    InputRecordingIndex;    // NOTE(felix): 0 = not recording

    HANDLE PlaybackHandle;         // NOTE(felix): File handle while playing back input
    int    InputPlayingIndex;      // NOTE(felix): 0 = not playing back

    char  EXEFileName[WIN32_STATE_FILE_NAME_COUNT];
    char *OnePastLastEXEFileNameSlash; // NOTE(felix): Points into EXEFileName, marks start of leaf name
};

#define WIN32_HANDMADE_H
#endif
