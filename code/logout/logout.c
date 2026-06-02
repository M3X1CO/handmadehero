#include "logout.h"

internal bool32
Win32LoadLogoutModule(void)
{
    OutputDebugStringA("logout loaded\n");
    return(true);
}

internal void
Win32UnloadLogoutModule(void)
{
    OutputDebugStringA("logout unloaded\n");
}
