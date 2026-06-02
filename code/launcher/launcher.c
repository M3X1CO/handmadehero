#include "launcher.h"

internal bool32
Win32LoadLauncherModule(void)
{
    OutputDebugStringA("launcher loaded\n");
    return(true);
}

internal void
Win32UnloadLauncherModule(void)
{
    OutputDebugStringA("launcher unloaded\n");
}
