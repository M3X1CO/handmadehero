#if !defined(INPUT_H)
#define INPUT_H

struct win32_input_state
{
    game_input Input[2];
    game_input *NewInput;
    game_input *OldInput;
};

internal bool32 Win32LoadInputModule(void);
internal void Win32UnloadInputModule(void);
internal void Win32InitializeInputState(win32_input_state *InputState);
internal game_input *Win32GetCurrentInput(win32_input_state *InputState);
internal void Win32UpdateInput(win32_state *State, HWND Window, win32_input_state *InputState);
internal void Win32SwapInputBuffers(win32_input_state *InputState);

#endif
