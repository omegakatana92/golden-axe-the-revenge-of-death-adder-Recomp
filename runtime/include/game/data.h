/**
 *    Name:        data.h
 *    Description: the game's data in the board's memory, loaded from the player's own ROM set
 *    Version:     0.1.0
 *
 *    license:BSD-3-Clause
 *
 *    No executable or source release carries the ROM or anything extracted from it. The original V60
 *    program is replaced by C generated from the player's ROM at build time; the ROM set is still read
 *    when the game starts because the translated program reads its data tables, graphics, samples and
 *    sound program from the board's memories, exactly where the original does.
 *
 *    Region sizes follow ROM_START( ga2 ) in MAME src/mame/sega/segas32.cpp (BSD-3-Clause).
 */
#ifndef GAME_DATA_H
#define GAME_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GAME_PROGRAM_SIZE   0x200000     /* the V60's program space as it sees it at 000000 */
#define GAME_TILES_SIZE     0x400000     /* 32768 16x16 tiles at 4 bits a pixel */
#define GAME_SPRITES_SIZE   0x1000000    /* sprite pixels, every 32-bit word big endian */
#define GAME_SOUND_SIZE     0x400000     /* the Z80's program space, then three 1 MB PCM banks */
#define GAME_MCU_SIZE       0x10000      /* the V25 protection firmware (address-decoded) */

extern uint8_t game_program[GAME_PROGRAM_SIZE];
extern uint8_t game_tiles[GAME_TILES_SIZE];
extern uint8_t game_sprites[GAME_SPRITES_SIZE];
extern uint8_t game_sound[GAME_SOUND_SIZE];
extern uint8_t game_mcu[GAME_MCU_SIZE];

/* The set's 93C45 serial EEPROM image (64 16-bit words), when the zip includes it. MAME's ga2 set
   lists none; starting from it preserves factory settings when a dump is supplied. */
extern uint16_t game_eeprom[64];
extern int game_eeprom_valid;

/* Fill the V25 protection MCU's dual-port RAM the way its reset routine does. The values live in the
   player's own MCU ROM (address-decoded), so no game data is embedded in the source. */
void game_protection_init(uint8_t *dpram, unsigned size);

/* Read the 'ga2' set from a .zip or a folder into the regions above. Returns NULL on success, or a
   short message naming the missing or damaged file. The source is never modified. */
const char *game_rom_set_load(const char *source);

#ifdef __cplusplus
}
#endif

#endif
