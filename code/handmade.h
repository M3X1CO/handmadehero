#if !defined(HANDMADE_H)
/* ========================================================================
   handmade.h — Game-side types, structs, and platform API contracts.
   This header is the boundary between the game DLL and the platform layer.
   The platform includes this; the game includes this. Neither side includes
   the other's header. Everything the game needs to talk to the platform,
   and everything the platform needs to call into the game, lives here.
   ======================================================================== */

/*
  NOTE(felix): Build configuration flags (set by the build script via -D):

  HANDMADE_INTERNAL:
    0 — Public/shipping build. Debug tools stripped out.
    1 — Developer build. Debug file I/O and asserts enabled.

  HANDMADE_SLOW:
    0 — No slow code. Assert() compiles to nothing.
    1 — Slow code OK. Assert() triggers a null-pointer fault on failure
        so the debugger catches it immediately.
*/

// TODO(felix): Replace with our own math — eliminate CRT dependency
#include <math.h>
#include <stdint.h>

// NOTE(felix): Readability macros. "internal" = translation-unit-local (static),
// "local_persist" = persists across calls (static local), "global_variable" = file-scope global.
#define internal        static
#define local_persist   static
#define global_variable static

#define Pi32 3.14159265359f

// NOTE(felix): Explicit-width integer aliases. Prefer these over bare int/long
// everywhere so sizes are unambiguous across compilers and platforms.
typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;
typedef int64_t  int64;
typedef int32    bool32;   // NOTE(felix): Win32-style bool — 0 = false, non-zero = true

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float  real32;
typedef double real64;

// NOTE(felix): Assert — in SLOW builds, a failing expression writes to address 0,
// which is an illegal write and immediately traps the debugger. In non-SLOW builds
// the macro expands to nothing (zero overhead).
#if HANDMADE_SLOW
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif

// NOTE(felix): Storage-size helpers. The LL suffix forces 64-bit arithmetic
// so e.g. Gigabytes(2) doesn't overflow a 32-bit intermediate.
#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

// NOTE(felix): Returns the element count of a stack-allocated array.
// sizeof(Array)/sizeof(element) — only valid for actual arrays, not pointers.
#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

// TODO(felix): Add Swap, Min, Max macros

// NOTE(felix): Safely narrows a uint64 to uint32. Asserts that the value
// fits — used when Win32 gives us a LARGE_INTEGER we need to pass as a DWORD.
inline uint32
SafeTruncateUInt64(uint64 Value)
{
    Assert(Value <= 0xFFFFFFFF);
    uint32 Result = (uint32)Value;
    return(Result);
}

// NOTE(felix): Opaque per-thread context passed through every platform call.
// Currently unused but reserved so we can add thread IDs, arenas, etc. later
// without changing every function signature.
struct thread_context
{
    int Placeholder;
};

/* ========================================================================
   Platform → Game services  (INTERNAL / debug builds only)
   These are blocking, single-threaded helpers. Never call from the shipping
   game — they are stripped when HANDMADE_INTERNAL == 0.
   ======================================================================== */
#if HANDMADE_INTERNAL

// NOTE(felix): Returned by DEBUGPlatformReadEntireFile.
// ContentsSize = byte count of the allocation; Contents = pointer to data.
// Caller must free Contents via DEBUGPlatformFreeFileMemory when done.
struct debug_read_file_result
{
    uint32 ContentsSize;
    void  *Contents;
};

// NOTE(felix): Function-pointer typedef macros. The macro defines both the
// function signature AND a typedef for a pointer to that signature, so the
// game_memory struct can store these as plain function pointers.

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) \
    void name(thread_context *Thread, void *Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) \
    debug_read_file_result name(thread_context *Thread, char *Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) \
    bool32 name(thread_context *Thread, char *Filename, uint32 MemorySize, void *Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

#endif // HANDMADE_INTERNAL

/* ========================================================================
   Shared buffer types  (platform allocates, game writes into)
   ======================================================================== */

// NOTE(felix): Pixel layout is always 32-bit: 0x XX RR GG BB (little-endian).
// Memory = pointer to the top-left pixel; Pitch = bytes per row (may include padding).
// TODO(felix): Rendering will eventually become a three-tier abstraction.
struct game_offscreen_buffer
{
    void *Memory;
    int   Width;
    int   Height;
    int   Pitch;
    int   BytesPerPixel;
};

// NOTE(felix): Interleaved stereo PCM — left sample then right sample, 16-bit signed.
// Game fills Samples[0..SampleCount*2-1] each frame.
struct game_sound_output_buffer
{
    int    SamplesPerSecond;
    int    SampleCount;
    int16 *Samples;
};

/* ========================================================================
   Input types
   ======================================================================== */

// NOTE(felix): Tracks one digital button. EndedDown = final state this frame;
// HalfTransitionCount = how many times the button crossed the up/down boundary
// during the frame (2 = pressed and released within a single frame).
struct game_button_state
{
    int    HalfTransitionCount;
    bool32 EndedDown;
};

// NOTE(felix): One controller's worth of input for a single frame.
// The union lets us address buttons by name (MoveUp, ActionDown…) or by
// index (Buttons[i]) — both views alias the same memory. Terminator is a
// sentinel placed after the last named button; the Assert in GameUpdateAndRender
// checks that the two views stay in sync if we ever add/remove buttons.
struct game_controller_input
{
    bool32  IsConnected;
    bool32  IsAnalog;
    real32  StickAverageX;  // NOTE(felix): Normalised [-1, 1], dead-zone already removed
    real32  StickAverageY;

    union
    {
        game_button_state Buttons[12];
        struct
        {
            game_button_state MoveUp;
            game_button_state MoveDown;
            game_button_state MoveLeft;
            game_button_state MoveRight;

            game_button_state ActionUp;
            game_button_state ActionDown;
            game_button_state ActionLeft;
            game_button_state ActionRight;

            game_button_state LeftShoulder;
            game_button_state RightShoulder;

            game_button_state Back;
            game_button_state Start;

            // NOTE(felix): ALL new buttons must be added ABOVE this line.
            game_button_state Terminator;
        };
    };
};

// NOTE(felix): Full input snapshot for one frame.
// Controllers[0] is always the keyboard. Controllers[1..4] are XInput pads.
// MouseX/Y are in client-rect pixels; MouseZ is reserved for scroll wheel.
struct game_input
{
    game_button_state   MouseButtons[5];
    int32               MouseX, MouseY, MouseZ;

    // TODO(felix): Add dt (seconds since last frame) here
    game_controller_input Controllers[5];
};

// NOTE(felix): Bounds-checked controller accessor — asserts ControllerIndex
// is in range, then returns a pointer into the Controllers array.
inline game_controller_input *
GetController(game_input *Input, int unsigned ControllerIndex)
{
    Assert(ControllerIndex < ArrayCount(Input->Controllers));
    return &Input->Controllers[ControllerIndex];
}

/* ========================================================================
   Game memory layout
   The platform allocates one contiguous block and splits it into two regions:
     PermanentStorage — lives for the entire session (game state, assets)
     TransientStorage — scratch space; can be thrown away between levels etc.
   Both regions are guaranteed zero-initialised at startup.
   ======================================================================== */
struct game_memory
{
    bool32  IsInitialized;              // NOTE(felix): Set to true by the game on first frame

    uint64  PermanentStorageSize;
    void   *PermanentStorage;

    uint64  TransientStorageSize;
    void   *TransientStorage;

    // NOTE(felix): Debug callbacks — only populated in INTERNAL builds.
    debug_platform_free_file_memory  *DEBUGPlatformFreeFileMemory;
    debug_platform_read_entire_file  *DEBUGPlatformReadEntireFile;
    debug_platform_write_entire_file *DEBUGPlatformWriteEntireFile;
};

/* ========================================================================
   Game DLL entry-point macros
   Using macros for the signatures means a single source-of-truth: the macro
   defines the typedef AND the actual function. Changing the signature only
   requires editing one place.
   ======================================================================== */

// NOTE(felix): Called once per frame. Updates game logic, writes into Buffer.
#define GAME_UPDATE_AND_RENDER(name) \
    void name(thread_context *Thread, game_memory *Memory, \
               game_input *Input, game_offscreen_buffer *Buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

// NOTE(felix): Called once per frame (possibly on a separate thread in future).
// Must be very fast — target < 1ms. Writes PCM samples into SoundBuffer.
// TODO(felix): Measure and enforce the timing budget.
#define GAME_GET_SOUND_SAMPLES(name) \
    void name(thread_context *Thread, game_memory *Memory, \
               game_sound_output_buffer *SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

/* ========================================================================
   Game state  (lives at the base of PermanentStorage)
   ======================================================================== */

// NOTE(felix): Cast PermanentStorage to this at the top of GameUpdateAndRender.
// The Assert at startup guarantees the struct fits in the allocated region.
struct game_state
{
    int    ToneHz;
    int    GreenOffset;
    int    BlueOffset;

    real32 tSine;       // NOTE(felix): Accumulated sine phase — prevents discontinuity across frames

    int    PlayerX;
    int    PlayerY;
    real32 tJump;       // NOTE(felix): Jump timer, counts down from 4.0 to 0 over the arc
};

#define HANDMADE_H
#endif