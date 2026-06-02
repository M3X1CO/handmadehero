#include "game_lobby.h"

internal bool32
Win32LoadGameLobbyModule(void)
{
    OutputDebugStringA("lobby loaded\n");
    return(true);
}

internal void
Win32UnloadGameLobbyModule(void)
{
    OutputDebugStringA("lobby unloaded\n");
}
