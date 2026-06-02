#if !defined(GAME_H)
#define GAME_H

internal void
GameUpdateAndRenderFrame(thread_context *Thread, game_memory *Memory,
                         game_input *Input, game_offscreen_buffer *Buffer);

internal void
GameWriteSoundSamples(thread_context *Thread, game_memory *Memory,
                      game_sound_output_buffer *SoundBuffer);

#endif
