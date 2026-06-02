#include "recording.h"

internal bool32
Win32LoadRecordingModule(void)
{
    OutputDebugStringA("recording loaded\n");
    return(true);
}

internal void
Win32UnloadRecordingModule(void)
{
    OutputDebugStringA("recording unloaded\n");
}

internal void
Win32GetInputFileLocation(win32_state *State, bool32 InputStream,
                          int SlotIndex, int DestCount, char *Dest)
{
    char Temp[64];
    wsprintf(Temp, "loop_edit_%d_%s.hmi", SlotIndex, InputStream ? "input" : "state");
    Win32BuildEXEPathFileName(State, Temp, DestCount, Dest);
}

internal win32_replay_buffer *
Win32GetReplayBuffer(win32_state *State, int unsigned Index)
{
    Assert(Index < ArrayCount(State->ReplayBuffers));
    return &State->ReplayBuffers[Index];
}

internal void
Win32InitializeReplayBuffers(win32_state *State)
{
    for(int ReplayIndex = 0;
        ReplayIndex < ArrayCount(State->ReplayBuffers);
        ++ReplayIndex)
    {
        win32_replay_buffer *ReplayBuffer = &State->ReplayBuffers[ReplayIndex];

        Win32GetInputFileLocation(State, false, ReplayIndex,
                                  sizeof(ReplayBuffer->FileName), ReplayBuffer->FileName);

        ReplayBuffer->FileHandle = CreateFileA(ReplayBuffer->FileName,
                                               GENERIC_WRITE|GENERIC_READ, 0, 0,
                                               CREATE_ALWAYS, 0, 0);

        LARGE_INTEGER MaxSize;
        MaxSize.QuadPart = State->TotalSize;
        ReplayBuffer->MemoryMap = CreateFileMapping(
            ReplayBuffer->FileHandle, 0, PAGE_READWRITE,
            MaxSize.HighPart, MaxSize.LowPart, 0);

        ReplayBuffer->MemoryBlock = MapViewOfFile(
            ReplayBuffer->MemoryMap, FILE_MAP_ALL_ACCESS, 0, 0, State->TotalSize);
    }
}

internal void
Win32BeginRecordingInput(win32_state *State, int InputRecordingIndex)
{
    win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(State, InputRecordingIndex);
    if(ReplayBuffer->MemoryBlock)
    {
        State->InputRecordingIndex = InputRecordingIndex;

        char FileName[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation(State, true, InputRecordingIndex, sizeof(FileName), FileName);
        State->RecordingHandle = CreateFileA(FileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

        CopyMemory(ReplayBuffer->MemoryBlock, State->GameMemoryBlock, State->TotalSize);
    }
}

internal void
Win32EndRecordingInput(win32_state *State)
{
    CloseHandle(State->RecordingHandle);
    State->InputRecordingIndex = 0;
}

internal void
Win32BeginInputPlayBack(win32_state *State, int InputPlayingIndex)
{
    win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(State, InputPlayingIndex);
    if(ReplayBuffer->MemoryBlock)
    {
        State->InputPlayingIndex = InputPlayingIndex;

        char FileName[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation(State, true, InputPlayingIndex, sizeof(FileName), FileName);
        State->PlaybackHandle = CreateFileA(FileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

        CopyMemory(State->GameMemoryBlock, ReplayBuffer->MemoryBlock, State->TotalSize);
    }
}

internal void
Win32EndInputPlayBack(win32_state *State)
{
    CloseHandle(State->PlaybackHandle);
    State->InputPlayingIndex = 0;
}

internal void
Win32RecordInput(win32_state *State, game_input *NewInput)
{
    DWORD BytesWritten;
    WriteFile(State->RecordingHandle, NewInput, sizeof(*NewInput), &BytesWritten, 0);
}

internal void
Win32PlayBackInput(win32_state *State, game_input *NewInput)
{
    DWORD BytesRead = 0;
    if(ReadFile(State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0))
    {
        if(BytesRead == 0)
        {
            int PlayingIndex = State->InputPlayingIndex;
            Win32EndInputPlayBack(State);
            Win32BeginInputPlayBack(State, PlayingIndex);
            ReadFile(State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0);
        }
    }
}
