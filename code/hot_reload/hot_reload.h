#if !defined(WIN32_HOT_RELOAD_MODULE_H)
#define WIN32_HOT_RELOAD_MODULE_H

internal bool32 Win32LoadHotReloadModule(void);
internal void Win32UnloadHotReloadModule(void);
inline FILETIME Win32GetLastWriteTime(char *Filename);
internal win32_game_code Win32LoadGameCode(char *SourceDLLName, char *TempDLLName);
internal void Win32UnloadGameCode(win32_game_code *GameCode);

#endif
