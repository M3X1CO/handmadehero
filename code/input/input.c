#include "input.h"

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) { return(ERROR_DEVICE_NOT_CONNECTED); }
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) { return(ERROR_DEVICE_NOT_CONNECTED); }
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

internal bool32
Win32LoadInputModule(void)
{
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
    if(!XInputLibrary) { XInputLibrary = LoadLibraryA("xinput9_1_0.dll"); }
    if(!XInputLibrary) { XInputLibrary = LoadLibraryA("xinput1_3.dll"); }

    if(XInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        if(!XInputGetState) { XInputGetState = XInputGetStateStub; }

        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
        if(!XInputSetState) { XInputSetState = XInputSetStateStub; }
    }

    OutputDebugStringA("input loaded\n");
    return(true);
}

internal void
Win32UnloadInputModule(void)
{
    OutputDebugStringA("input unloaded\n");
}

internal void
Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown)
{
    if(NewState->EndedDown != IsDown)
    {
        NewState->EndedDown = IsDown;
        ++NewState->HalfTransitionCount;
    }
}

internal void
Win32ProcessXInputDigitalButton(DWORD XInputButtonState,
                                game_button_state *OldState, DWORD ButtonBit,
                                game_button_state *NewState)
{
    NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal real32
Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold)
{
    real32 Result = 0;
    if(Value < -DeadZoneThreshold)
    {
        Result = (real32)((Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold));
    }
    else if(Value > DeadZoneThreshold)
    {
        Result = (real32)((Value - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold));
    }
    return(Result);
}

internal void
Win32ProcessPendingMessages(win32_state *State, game_controller_input *KeyboardController)
{
    MSG Message;
    while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
    {
        switch(Message.message)
        {
            case WM_QUIT: { GlobalRunning = false; } break;

            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                uint32 VKCode = (uint32)Message.wParam;
                bool32 WasDown = ((Message.lParam & (1 << 30)) != 0);
                bool32 IsDown = ((Message.lParam & (1 << 31)) == 0);

                if(WasDown != IsDown)
                {
                    if     (VKCode == 'W')        Win32ProcessKeyboardMessage(&KeyboardController->MoveUp,        IsDown);
                    else if(VKCode == 'A')        Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft,      IsDown);
                    else if(VKCode == 'S')        Win32ProcessKeyboardMessage(&KeyboardController->MoveDown,      IsDown);
                    else if(VKCode == 'D')        Win32ProcessKeyboardMessage(&KeyboardController->MoveRight,     IsDown);
                    else if(VKCode == 'Q')        Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder,  IsDown);
                    else if(VKCode == 'E')        Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
                    else if(VKCode == VK_UP)      Win32ProcessKeyboardMessage(&KeyboardController->ActionUp,      IsDown);
                    else if(VKCode == VK_LEFT)    Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft,    IsDown);
                    else if(VKCode == VK_DOWN)    Win32ProcessKeyboardMessage(&KeyboardController->ActionDown,    IsDown);
                    else if(VKCode == VK_RIGHT)   Win32ProcessKeyboardMessage(&KeyboardController->ActionRight,   IsDown);
                    else if(VKCode == VK_ESCAPE)  Win32ProcessKeyboardMessage(&KeyboardController->Start,         IsDown);
                    else if(VKCode == VK_SPACE)   Win32ProcessKeyboardMessage(&KeyboardController->Back,          IsDown);

#if HANDMADE_INTERNAL
                    else if(VKCode == 'P')
                    {
                        if(IsDown) { GlobalPause = !GlobalPause; }
                    }
                    else if(VKCode == 'L')
                    {
                        if(IsDown)
                        {
                            if(State->InputPlayingIndex == 0)
                            {
                                if(State->InputRecordingIndex == 0) { Win32BeginRecordingInput(State, 1); }
                                else { Win32EndRecordingInput(State); Win32BeginInputPlayBack(State, 1); }
                            }
                            else { Win32EndInputPlayBack(State); }
                        }
                    }
#endif
                }

                bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
                if((VKCode == VK_F4) && AltKeyWasDown) { GlobalRunning = false; }
            } break;

            default:
            {
                TranslateMessage(&Message);
                DispatchMessageA(&Message);
            } break;
        }
    }
}

internal void
Win32InitializeInputState(win32_input_state *InputState)
{
    *InputState = {};
    InputState->NewInput = &InputState->Input[0];
    InputState->OldInput = &InputState->Input[1];
}

internal game_input *
Win32GetCurrentInput(win32_input_state *InputState)
{
    return(InputState->NewInput);
}

internal void
Win32UpdateInput(win32_state *State, HWND Window, win32_input_state *InputState)
{
    game_input *OldInput = InputState->OldInput;
    game_input *NewInput = InputState->NewInput;

    game_controller_input *OldKeyboardController = GetController(OldInput, 0);
    game_controller_input *NewKeyboardController = GetController(NewInput, 0);
    *NewKeyboardController = {};
    NewKeyboardController->IsConnected = true;

    for(int ButtonIndex = 0;
        ButtonIndex < ArrayCount(NewKeyboardController->Buttons);
        ++ButtonIndex)
    {
        NewKeyboardController->Buttons[ButtonIndex].EndedDown =
            OldKeyboardController->Buttons[ButtonIndex].EndedDown;
    }

    Win32ProcessPendingMessages(State, NewKeyboardController);

    POINT MouseP;
    GetCursorPos(&MouseP);
    ScreenToClient(Window, &MouseP);
    NewInput->MouseX = MouseP.x;
    NewInput->MouseY = MouseP.y;
    NewInput->MouseZ = 0;

    Win32ProcessKeyboardMessage(&NewInput->MouseButtons[0], GetKeyState(VK_LBUTTON) & (1 << 15));
    Win32ProcessKeyboardMessage(&NewInput->MouseButtons[1], GetKeyState(VK_MBUTTON) & (1 << 15));
    Win32ProcessKeyboardMessage(&NewInput->MouseButtons[2], GetKeyState(VK_RBUTTON) & (1 << 15));
    Win32ProcessKeyboardMessage(&NewInput->MouseButtons[3], GetKeyState(VK_XBUTTON1) & (1 << 15));
    Win32ProcessKeyboardMessage(&NewInput->MouseButtons[4], GetKeyState(VK_XBUTTON2) & (1 << 15));

    DWORD MaxControllerCount = XUSER_MAX_COUNT;
    if(MaxControllerCount > ArrayCount(NewInput->Controllers) - 1)
    {
        MaxControllerCount = ArrayCount(NewInput->Controllers) - 1;
    }

    for(DWORD ControllerIndex = 0;
        ControllerIndex < MaxControllerCount;
        ++ControllerIndex)
    {
        DWORD OurControllerIndex = ControllerIndex + 1;
        game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
        game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

        XINPUT_STATE ControllerState;
        if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
        {
            NewController->IsConnected = true;
            NewController->IsAnalog = true;

            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

            NewController->StickAverageX =
                Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
            NewController->StickAverageY =
                Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

            if((NewController->StickAverageX != 0.0f) ||
               (NewController->StickAverageY != 0.0f))
            {
                NewController->IsAnalog = true;
            }

            real32 Threshold = 0.5f;
            Win32ProcessXInputDigitalButton((NewController->StickAverageX < -Threshold) ? 1 : 0, &OldController->MoveLeft,  1, &NewController->MoveLeft);
            Win32ProcessXInputDigitalButton((NewController->StickAverageX >  Threshold) ? 1 : 0, &OldController->MoveRight, 1, &NewController->MoveRight);
            Win32ProcessXInputDigitalButton((NewController->StickAverageY < -Threshold) ? 1 : 0, &OldController->MoveDown,  1, &NewController->MoveDown);
            Win32ProcessXInputDigitalButton((NewController->StickAverageY >  Threshold) ? 1 : 0, &OldController->MoveUp,    1, &NewController->MoveUp);

            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionDown,    XINPUT_GAMEPAD_A,              &NewController->ActionDown);
            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionRight,   XINPUT_GAMEPAD_B,              &NewController->ActionRight);
            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionLeft,    XINPUT_GAMEPAD_X,              &NewController->ActionLeft);
            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->ActionUp,      XINPUT_GAMEPAD_Y,              &NewController->ActionUp);
            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->LeftShoulder,  XINPUT_GAMEPAD_LEFT_SHOULDER,  &NewController->LeftShoulder);
            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);
            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Start,         XINPUT_GAMEPAD_START,          &NewController->Start);
            Win32ProcessXInputDigitalButton(Pad->wButtons, &OldController->Back,          XINPUT_GAMEPAD_BACK,           &NewController->Back);
        }
        else
        {
            NewController->IsConnected = false;
        }
    }
}

internal void
Win32SwapInputBuffers(win32_input_state *InputState)
{
    game_input *Temp = InputState->NewInput;
    InputState->NewInput = InputState->OldInput;
    InputState->OldInput = Temp;
}
