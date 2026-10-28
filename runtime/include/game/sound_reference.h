/**
 *    Name:        sound_reference.h
 *    Description: the sound board the window port uses until the native Z80 decompilation is finished:
 *                 the original Z80 program on MAME's Z80 core with MAME's RF5C68 and ymfm's YM3438
 *    Version:     0.1.0
 *    Created:     28-10-2026
 *    Author:      Spider-Man recompilation project
 *
 *    license:BSD-3-Clause
 *
 *    This is a documented fallback, not the native sound path. The main V60 program is native; the
 *    sound CPU here is emulated (MAME's Z80 core, BSD-3-Clause). The native Z80 translation is the
 *    next step; see BUILD_STATUS.md. The API is C so the C port can call it.
 */
#ifndef GAME_SOUND_REFERENCE_H
#define GAME_SOUND_REFERENCE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Attach to s32_board (s32_board.sound_rom must be set first) */
void game_sound_attach(void);

/* Close the frame's samples at board time 'time' */
void game_sound_end_frame(uint64_t time);

/* The frame's mixed stereo samples (left, right interleaved), and their rate */
const int16_t *game_sound_frame_mix(size_t *frames);
uint32_t game_sound_rate(void);

#ifdef __cplusplus
}
#endif

#endif
