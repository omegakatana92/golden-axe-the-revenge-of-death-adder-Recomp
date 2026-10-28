/**
 *    Name:        sound_reference.cpp
 *    Description: C wrapper around the reference sound board (s32sound.h): the original Z80 program on
 *                 MAME's Z80 core with MAME's RF5C68 and ymfm's YM3438
 *    Version:     0.1.0
 *    Created:     28-10-2026
 *
 *    license:BSD-3-Clause
 *
 *    Documented fallback: the main V60 program is native; this sound CPU is emulated. It uses the same
 *    board model, sound ROM and chip routes as tools/reference/s32ref, so the samples match the
 *    reference. The native Z80 decompilation is incomplete (BUILD_STATUS.md): its two command dispatchers
 *    need a return-address model the decompiler does not have yet. This wrapper keeps the game audible
 *    while that work continues.
 */
#include "game/sound_reference.h"

#include "s32sound.h"

extern "C" void game_sound_attach(void)
{
	s32s_attach();
}

extern "C" void game_sound_end_frame(uint64_t time)
{
	s32s_end_frame(time);
}

extern "C" const int16_t *game_sound_frame_mix(size_t *frames)
{
	return s32s_frame_mix(frames);
}

extern "C" uint32_t game_sound_rate(void)
{
	return s32s_mix_rate();
}
