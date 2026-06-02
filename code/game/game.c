#include "game.h"

internal void
GameUpdateAndRenderFrame(thread_context *Thread, game_memory *Memory,
                         game_input *Input, game_offscreen_buffer *Buffer)
{
    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
           (ArrayCount(Input->Controllers[0].Buttons)));
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    game_state *GameState = (game_state *)Memory->PermanentStorage;

    if(!Memory->IsInitialized)
    {
        char *Filename = __FILE__;
        debug_read_file_result File = Memory->DEBUGPlatformReadEntireFile(Thread, Filename);
        if(File.Contents)
        {
            Memory->DEBUGPlatformWriteEntireFile(Thread, "test.out", File.ContentsSize, File.Contents);
            Memory->DEBUGPlatformFreeFileMemory(Thread, File.Contents);
        }

        GameState->ToneHz  = 512;
        GameState->tSine   = 0.0f;
        GameState->PlayerX = 100;
        GameState->PlayerY = 100;

        Memory->IsInitialized = true;
    }

    for(int ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex)
    {
        game_controller_input *Controller = GetController(Input, ControllerIndex);

        if(Controller->IsAnalog)
        {
            GameState->BlueOffset += (int)(4.0f * Controller->StickAverageX);
            GameState->ToneHz = 512 + (int)(128.0f * Controller->StickAverageY);
        }
        else
        {
            if(Controller->MoveLeft.EndedDown)  { GameState->BlueOffset -= 1; }
            if(Controller->MoveRight.EndedDown) { GameState->BlueOffset += 1; }
        }

        GameState->PlayerX += (int)(4.0f * Controller->StickAverageX);
        GameState->PlayerY -= (int)(4.0f * Controller->StickAverageY);

        if(GameState->tJump > 0)
        {
            GameState->PlayerY += (int)(5.0f * sinf(0.5f * Pi32 * GameState->tJump));
        }
        if(Controller->ActionDown.EndedDown) { GameState->tJump = 4.0f; }
        GameState->tJump -= 0.033f;
    }

    RenderWeirdGradient(Buffer, GameState->BlueOffset, GameState->GreenOffset);
    RenderPlayer(Buffer, GameState->PlayerX, GameState->PlayerY);
    RenderPlayer(Buffer, Input->MouseX, Input->MouseY);

    for(int ButtonIndex = 0;
        ButtonIndex < ArrayCount(Input->MouseButtons);
        ++ButtonIndex)
    {
        if(Input->MouseButtons[ButtonIndex].EndedDown)
        {
            RenderPlayer(Buffer, 10 + 20 * ButtonIndex, 10);
        }
    }
}

internal void
GameWriteSoundSamples(thread_context *Thread, game_memory *Memory,
                      game_sound_output_buffer *SoundBuffer)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer, GameState->ToneHz);
}
