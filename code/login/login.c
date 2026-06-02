#include "login.h"

internal bool32
Win32LoadLoginModule(void)
{
    OutputDebugStringA("login loaded\n");
    return(true);
}

internal void
Win32UnloadLoginModule(void)
{
    OutputDebugStringA("login unloaded\n");
}
