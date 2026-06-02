#if !defined(WIN32_RECORDING_MODULE_H)
#define WIN32_RECORDING_MODULE_H

internal bool32 Win32LoadRecordingModule(void);
internal void Win32UnloadRecordingModule(void);
internal void Win32BuildEXEPathFileName(win32_state *State, char *FileName, int DestCount, char *Dest);
internal void Win32GetInputFileLocation(win32_state *State, bool32 InputStream,
                                        int SlotIndex, int DestCount, char *Dest);
internal win32_replay_buffer *Win32GetReplayBuffer(win32_state *State, int unsigned Index);
internal void Win32InitializeReplayBuffers(win32_state *State);
internal void Win32BeginRecordingInput(win32_state *State, int InputRecordingIndex);
internal void Win32EndRecordingInput(win32_state *State);
internal void Win32BeginInputPlayBack(win32_state *State, int InputPlayingIndex);
internal void Win32EndInputPlayBack(win32_state *State);
internal void Win32RecordInput(win32_state *State, game_input *NewInput);
internal void Win32PlayBackInput(win32_state *State, game_input *NewInput);

#endif
