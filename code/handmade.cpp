/* ========================================================================
   handmade.cpp - Game DLL entry points.
   ======================================================================== */

#include "handmade.h"
#define HANDMADE_GAME_LAYER 1
#define HANDMADE_PLATFORM_LAYER 0
#include "sound/sound.h"
#include "render/render.h"
#include "game/game.h"

#include "sound/sound.c"
#include "render/render.c"
#include "game/game.c"
#undef HANDMADE_PLATFORM_LAYER
#undef HANDMADE_GAME_LAYER

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    GameUpdateAndRenderFrame(Thread, Memory, Input, Buffer);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    GameWriteSoundSamples(Thread, Memory, SoundBuffer);
}
