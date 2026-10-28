/**
 *    Name:        rom_set.c
 *    Description: the player's 'ga2' ROM set into the board's memory (game/data.h), from a .zip or folder
 *    Version:     0.1.0
 *
 *    license:BSD-3-Clause
 *
 *    Region layouts follow ROM_START( ga2 ) in MAME src/mame/sega/segas32.cpp (BSD-3-Clause).
 *    Files are recognised by CRC32 (and the ZIP's CRC is enough to reject a wrong dump before it is
 *    inflated). The set is only read, never changed.
 */
#include "game/data.h"
#include "inflate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZIP_END             0x06054B50u
#define ZIP_ENTRY           0x02014B50u
#define ZIP_LOCAL           0x04034B50u
#define ZIP_STORED          0
#define ZIP_DEFLATED        8
#define ZIP_COMMENT_MOST    0xFFFFu

typedef struct
{
	const char *name;
	const char *older;
	uint32_t crc, size;
} rom_file;

enum
{
	IC17, IC8, IC18, IC9, IC36, IC35, IC34, IC24, MCU, IC14, IC5,
	IC32, IC30, IC28, IC26, IC31, IC29, IC27, IC25, EEPROM, FILES
};

static const rom_file files[FILES] = {
	{ "epr-14961b.ic17", NULL, 0xd9cd8885u, 0x020000u },
	{ "epr-14958b.ic8",  NULL, 0x0be324a3u, 0x020000u },
	{ "epr-15148b.ic18", NULL, 0xc477a9fdu, 0x040000u },
	{ "epr-15147b.ic9",  NULL, 0x1bb676eau, 0x040000u },
	{ "epr-14945.ic36",  NULL, 0x4781d4cbu, 0x010000u },
	{ "mpr-14944.ic35",  NULL, 0xfd4d4b86u, 0x100000u },
	{ "mpr-14943.ic34",  NULL, 0x24d40333u, 0x100000u },
	{ "mpr-14942.ic24",  NULL, 0xa89b0e90u, 0x100000u },
	{ "epr-14468-02.u3", NULL, 0x77634daau, 0x010000u },
	{ "mpr-14948.ic14",  NULL, 0x75050d4au, 0x200000u },
	{ "mpr-14947.ic5",   NULL, 0xb53e62f4u, 0x200000u },
	{ "mpr-14949.ic32",  NULL, 0x152c716cu, 0x200000u },
	{ "mpr-14951.ic30",  NULL, 0xfdb1a534u, 0x200000u },
	{ "mpr-14953.ic28",  NULL, 0x33bd1c15u, 0x200000u },
	{ "mpr-14955.ic26",  NULL, 0xe42684aau, 0x200000u },
	{ "mpr-14950.ic31",  NULL, 0x15fd0026u, 0x200000u },
	{ "mpr-14952.ic29",  NULL, 0x96f96613u, 0x200000u },
	{ "mpr-14954.ic27",  NULL, 0x39b2ac9eu, 0x200000u },
	{ "mpr-14956.ic25",  NULL, 0x298fca50u, 0x200000u },
	{ "93c45_eeprom.ic76", NULL, 0x6e1d9df3u, 0x0080u },
};

uint16_t game_eeprom[64];
int game_eeprom_valid;

uint8_t game_program[GAME_PROGRAM_SIZE];
uint8_t game_tiles[GAME_TILES_SIZE];
uint8_t game_sprites[GAME_SPRITES_SIZE];
uint8_t game_sound[GAME_SOUND_SIZE];
uint8_t game_mcu[GAME_MCU_SIZE];

static uint32_t crc32_of(const uint8_t *data, uint32_t size)
{
	static uint32_t table[256];
	uint32_t crc = 0xFFFFFFFFu, index;
	int bit;

	if (table[1] == 0u)
		for (index = 0; index < 256u; index++)
		{
			table[index] = index;
			for (bit = 0; bit < 8; bit++)
				table[index] = (table[index] >> 1) ^ (0xEDB88320u & (0u - (table[index] & 1u)));
		}
	for (index = 0; index < size; index++)
		crc = (crc >> 8) ^ table[(crc ^ data[index]) & 0xFFu];
	return ~crc;
}

static uint32_t little16(const uint8_t *bytes)
{
	return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8;
}

static uint32_t little32(const uint8_t *bytes)
{
	return little16(bytes) | little16(bytes + 2) << 16;
}

static uint8_t *read_whole(const char *path, size_t *size)
{
	FILE *input = fopen(path, "rb");
	uint8_t *data = NULL;
	long length;

	if (!input)
		return NULL;
	if (fseek(input, 0, SEEK_END) == 0 && (length = ftell(input)) > 0 && fseek(input, 0, SEEK_SET) == 0
		&& (data = malloc((size_t)length)) != NULL && fread(data, 1, (size_t)length, input) != (size_t)length)
	{
		free(data);
		data = NULL;
	}
	fclose(input);
	if (data)
		*size = (size_t)length;
	return data;
}

static int read_zip(const uint8_t *zip, size_t size, uint8_t **chips)
{
	size_t end, entry, local;
	uint32_t entries, index;
	int file;

	if (size < 22)
		return 0;
	for (end = size - 22;; end--)
	{
		if (little32(zip + end) == ZIP_END)
			break;
		if (end == 0 || size - end > 22 + ZIP_COMMENT_MOST)
			return 0;
	}
	entries = little16(zip + end + 10);
	entry = little32(zip + end + 16);
	for (index = 0; index < entries; index++)
	{
		uint32_t method, crc, packed, unpacked;

		if (entry > size || size - entry < 46 || little32(zip + entry) != ZIP_ENTRY)
			return 0;
		method = little16(zip + entry + 10);
		crc = little32(zip + entry + 16);
		packed = little32(zip + entry + 20);
		unpacked = little32(zip + entry + 24);
		local = little32(zip + entry + 42);
		entry += 46 + little16(zip + entry + 28) + little16(zip + entry + 30) + little16(zip + entry + 32);
		for (file = 0; file < FILES; file++)
			if (!chips[file] && files[file].crc == crc && files[file].size == unpacked)
				break;
		if (file == FILES || local > size || size - local < 30 || little32(zip + local) != ZIP_LOCAL)
			continue;
		local += 30 + little16(zip + local + 26) + little16(zip + local + 28);
		if (local > size || packed > size - local || (method == ZIP_STORED && packed != unpacked)
			|| !(chips[file] = malloc(unpacked)))
			continue;
		if ((method == ZIP_STORED ? (memcpy(chips[file], zip + local, unpacked), 1)
				: method == ZIP_DEFLATED && inflate_raw(zip + local, packed, chips[file], unpacked))
			&& crc32_of(chips[file], unpacked) == crc)
			continue;
		free(chips[file]);
		chips[file] = NULL;
	}
	return 1;
}

static void read_folder(const char *folder, uint8_t **chips)
{
	char path[1024];
	int file, name;

	for (file = 0; file < FILES; file++)
		for (name = 0; name < 2 && !chips[file]; name++)
		{
			const char *names[2] = { files[file].name, files[file].older };
			size_t size = 0;

			if (!names[name])
				continue;
			snprintf(path, sizeof(path), "%s/%s", folder, names[name]);
			chips[file] = read_whole(path, &size);
			if (chips[file] && (size != files[file].size || crc32_of(chips[file], files[file].size) != files[file].crc))
			{
				free(chips[file]);
				chips[file] = NULL;
			}
		}
}

static void interleave16(uint8_t *out, const uint8_t *even, const uint8_t *odd, uint32_t size)
{
	uint32_t index;

	for (index = 0; index < size; index++)
	{
		out[index * 2u] = even[index];
		out[index * 2u + 1u] = odd[index];
	}
}

/* The V60's upper 1 MB region: both 256 KB chips are decoded twice (ROM_LOAD16_BYTE_x2). */
static void interleave16_mirrored(uint8_t *out, const uint8_t *even, const uint8_t *odd, uint32_t chip_size)
{
	uint32_t index;

	for (index = 0; index < chip_size * 2u; index++)
	{
		const uint32_t source = index % chip_size;
		out[index * 2u] = even[source];
		out[index * 2u + 1u] = odd[source];
	}
}

static void sprite_lanes(uint32_t base, uint8_t *const *lanes, uint32_t size)
{
	uint32_t lane, word;

	for (lane = 0; lane < 4u; lane++)
		for (word = 0; word < size / 2u; word++)
		{
			game_sprites[base + word * 8u + lane * 2u] = lanes[lane][word * 2u];
			game_sprites[base + word * 8u + lane * 2u + 1u] = lanes[lane][word * 2u + 1u];
		}
}

/* MAME's decrypt_protrom: the V25 board scrambles its ROM's address lines. */
static unsigned mcu_address(unsigned index)
{
	/* bitswap<16>(i, 14, 11, 15, 12, 13, 4, 3, 7, 5, 10, 2, 8, 9, 6, 1, 0) */
	static const unsigned order[16] = { 14, 11, 15, 12, 13, 4, 3, 7, 5, 10, 2, 8, 9, 6, 1, 0 };
	unsigned result = 0;

	for (int bit = 0; bit < 16; bit++)
		result = (result << 1) | ((index >> order[bit]) & 1u);
	return result;
}

void game_protection_init(uint8_t *dpram, unsigned size)
{
	unsigned i;

	/* The V25 clears its 4 KB DPRAM, then copies two blocks from its reset tables: a 16-byte sprite
	   protection table to 0x000 and a 48-byte wake-up string to 0x080. Reproduced from the reset
	   routine at V25 0x4DB/0x504; see docs/PROTECTION.md. */
	memset(dpram, 0, size);
	for (i = 0; i < 16u && i < size; i++)
		dpram[i] = game_mcu[0x4F4u + i];
	for (i = 0; i < 48u && 0x80u + i < size; i++)
		dpram[0x80u + i] = game_mcu[0x51Du + i];
}

const char *game_rom_set_load(const char *source)
{
	static char problem[96];
	uint8_t *chips[FILES] = { NULL };
	size_t size = 0;
	uint8_t *whole = read_whole(source, &size);
	int file, copy;

	if (whole)
	{
		const int zip = read_zip(whole, size, chips);

		free(whole);
		if (!zip)
			return "not a .zip";
	}
	else
	{
		/* a rom/ folder may hold the set's zip; the loader reads both that and loose files */
		char nested[1024];
		FILE *probe;

		snprintf(nested, sizeof(nested), "%s/ga2.zip", source);
		if ((probe = fopen(nested, "rb")) != NULL)
		{
			fclose(probe);
			whole = read_whole(nested, &size);
			if (whole)
			{
				const int zip = read_zip(whole, size, chips);
				free(whole);
				if (!zip)
					return "not a .zip";
			}
		}
		read_folder(source, chips);
	}
	problem[0] = '\0';
	game_eeprom_valid = 0;
	for (file = 0; file < FILES && !problem[0]; file++)
		if (file != EEPROM && !chips[file])
			snprintf(problem, sizeof(problem), "no %s (or not this version of it)", files[file].name);
	if (!problem[0])
	{
		/* ROM_LOAD_x4: each 128 KB program chip fills 512 KB */
		for (copy = 0; copy < 4; copy++)
		{
			memcpy(game_program + copy * 0x20000u, chips[IC17], 0x20000u);
			memcpy(game_program + 0x080000u + copy * 0x20000u, chips[IC8], 0x20000u);
		}
		/* the two 256 KB data chips interleave twice over the upper 1 MB */
		interleave16_mirrored(game_program + 0x100000u, chips[IC18], chips[IC9], 0x40000u);

		interleave16(game_tiles, chips[IC14], chips[IC5], 0x200000u);

		memset(game_sprites, 0xFF, GAME_SPRITES_SIZE);
		sprite_lanes(0x000000u, &chips[IC32], 0x200000u);
		sprite_lanes(0x800000u, &chips[IC31], 0x200000u);

		/* ROM_LOAD_x16: the 64 KB Z80 program is decoded at 0 and repeated over the 1 MB window */
		for (copy = 0; copy < 16; copy++)
			memcpy(game_sound + copy * 0x10000u, chips[IC36], 0x10000u);
		memcpy(game_sound + 0x100000u, chips[IC35], 0x100000u);
		memcpy(game_sound + 0x200000u, chips[IC34], 0x100000u);
		memcpy(game_sound + 0x300000u, chips[IC24], 0x100000u);

		for (copy = 0; copy < GAME_MCU_SIZE; copy++)
			game_mcu[copy] = chips[MCU][mcu_address((unsigned)copy)];

		/* the EEPROM image is big-endian 16-bit words */
		game_eeprom_valid = chips[EEPROM] != NULL;
		if (game_eeprom_valid)
			for (file = 0; file < 64; file++)
				game_eeprom[file] = (uint16_t)((chips[EEPROM][file * 2] << 8) | chips[EEPROM][file * 2 + 1]);
	}
	for (file = 0; file < FILES; file++)
		free(chips[file]);
	return problem[0] ? problem : NULL;
}
