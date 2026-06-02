#if !defined(WIN32_FILE_IO_MODULE_H)
#define WIN32_FILE_IO_MODULE_H

internal bool32 Win32LoadFileIOModule(void);
internal void Win32UnloadFileIOModule(void);
DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory);
DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile);
DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile);

#endif
