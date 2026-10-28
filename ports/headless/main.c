/**
 *    Name:        main.c
 *    Description: runs the translated Alien 3 program with no window, for reference comparisons
 *
 *    Usage: alien3_headless [--rom FILE_OR_FOLDER] [--frames N] [--script FILE] [--hashes FILE]
 *                            [--video-hashes FILE] [--dump-frame N --dump-dir DIR]
 *
 *    The program is the C tools/recompile.py generated from the player's own ROM. The ROM set is still
 *    read at startup because the translated program reads its data, tiles, sprites and sound program
 *    from the board's memories.
 */
#include "game/data.h"
#include "s32/machine.h"
#include "s32/program.h"
#include "s32/script.h"
#include "s32/video.h"
#include "sound/chips.h"
#include "v60/cpu.h"

#include <inttypes.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VIDEO_FILE "video_ram.bin"

static jmp_buf finished;
static uint64_t frames_wanted = 600;
static FILE *hashes;
static FILE *video_hashes;
static FILE *sound_hashes;
static s32_script *script;
static uint64_t dump_frame = UINT64_MAX;
static const char *dump_dir;
static uint64_t video_dump_frame = UINT64_MAX;
static const char *video_dump_path;
static int aim1x = -1, aim1y = -1;
static int aim2x = -1, aim2y = -1;

static void write_file(const char *name, const void *data, size_t size)
{
	char path[1024];
	FILE *output;

	snprintf(path, sizeof(path), "%s/%s", dump_dir, name);
	output = fopen(path, "wb");
	if (!output)
	{
		fprintf(stderr, "%s: cannot write\n", path);
		return;
	}
	fwrite(data, 1, size, output);
	fclose(output);
}

static void dump_state(void)
{
	write_file("work_ram.bin", s32_board.work_ram, sizeof(s32_board.work_ram));
	write_file(VIDEO_FILE, s32_board.video_ram, sizeof(s32_board.video_ram));
	write_file("sprite_ram.bin", s32_board.sprite_ram, sizeof(s32_board.sprite_ram));
	write_file("palette.bin", s32_board.palette, sizeof(s32_board.palette));
	write_file("mixer.bin", s32_board.mixer, sizeof(s32_board.mixer));
	write_file("shared_ram.bin", s32_board.shared_ram, sizeof(s32_board.shared_ram));
	write_file("eeprom.bin", s32_board.eeprom.data, sizeof(s32_board.eeprom.data));
}

static void on_irq_changed(bool asserted)
{
	program_irq_line(asserted);
}

static void on_log(const char *message)
{
	fprintf(stderr, "[s32] %s\n", message);
}

static void on_frame_shown(void)
{
	s32_video_draw_frame();
}

/* The board sends its sprite list 50 us after VBLANK ends: that is one complete frame, the point the
   reference accounts for. Chain the video library's callback and account our frame here. */
static void (*video_sprites_due)(bool erase, bool draw);

static void on_frame_done(void);

static void on_sprites_due(bool erase, bool draw)
{
	video_sprites_due(erase, draw);
	on_frame_done();
}

static void on_frame_done(void)
{
	const uint64_t frame = s32_board.frame;

	if (hashes)
		fprintf(hashes, "%" PRIu64 " %016" PRIx64 "\n", frame, s32_state_hash());
	if (video_hashes)
		fprintf(video_hashes, "%" PRIu64 " %016" PRIx64 "\n", frame, s32_video_frame_hash());
	if (sound_hashes)
	{
#ifdef GA2_SOUND
		s32_board.time_passes(s32_board.cycles);
		sound_chips_end_frame(s32_board.cycles);
		fprintf(sound_hashes, "%" PRIu64 " %016" PRIx64 " %016" PRIx64 "\n", frame,
			sound_chips_frame_hash(), sound_board_state_hash());
#endif
	}
	if (dump_dir && frame == dump_frame)
		dump_state();
	if (video_dump_path && frame == video_dump_frame)
	{
		int width;
		const uint32_t *pixels = s32_video_frame(&width);
		const uint32_t row = ((uint32_t)width * 3 + 3) & ~3u;
		const uint32_t size = 54 + row * S32_VIDEO_HEIGHT;
		uint8_t *file = (uint8_t *)calloc(size, 1);

		if (file)
		{
			for (int byte = 0; byte < 4; byte++)
			{
				file[2 + byte] = (uint8_t)(size >> (8 * byte));
				file[18 + byte] = (uint8_t)((uint32_t)width >> (8 * byte));
				file[22 + byte] = (uint8_t)((uint32_t)S32_VIDEO_HEIGHT >> (8 * byte));
			}
			file[0] = 'B';
			file[1] = 'M';
			file[10] = 54;
			file[14] = 40;
			file[26] = 1;
			file[28] = 24;
			for (int y = 0; y < S32_VIDEO_HEIGHT; y++)
				for (int x = 0; x < width; x++)
				{
					const uint32_t pixel = pixels[y * S32_VIDEO_MAX_WIDTH + x];
					uint8_t *out = &file[54 + (uint32_t)(S32_VIDEO_HEIGHT - 1 - y) * row + (uint32_t)x * 3];
					out[0] = (uint8_t)pixel;
					out[1] = (uint8_t)(pixel >> 8);
					out[2] = (uint8_t)(pixel >> 16);
				}
			FILE *output = fopen(video_dump_path, "wb");
			if (output)
			{
				fwrite(file, 1, size, output);
				fclose(output);
			}
			free(file);
		}
	}
	if (frames_wanted && frame + 1 >= frames_wanted)
		longjmp(finished, 1);
}

static void on_controls_update(void)
{
	s32_script_apply(script, s32_board.frame);
	if (aim1x >= 0)
	{
		s32_board.input.analog[0] = (uint8_t)aim1x;
		s32_board.input.analog[1] = (uint8_t)aim1y;
	}
	if (aim2x >= 0)
	{
		s32_board.input.analog[2] = (uint8_t)aim2x;
		s32_board.input.analog[3] = (uint8_t)aim2y;
	}
}

int main(int argc, char **argv)
{
	const char *rom = "rom";
	const char *hashes_path = NULL;
	const char *video_hashes_path = NULL;
	const char *sound_hashes_path = NULL;
	const char *script_path = NULL;
	bool frames_given = false;

	for (int index = 1; index < argc; index++)
	{
		if (strcmp(argv[index], "--rom") == 0 && index + 1 < argc)
			rom = argv[++index];
		else if (strcmp(argv[index], "--frames") == 0 && index + 1 < argc)
		{
			frames_wanted = strtoull(argv[++index], NULL, 10);
			frames_given = true;
		}
		else if (strcmp(argv[index], "--hashes") == 0 && index + 1 < argc)
			hashes_path = argv[++index];
		else if (strcmp(argv[index], "--video-hashes") == 0 && index + 1 < argc)
			video_hashes_path = argv[++index];
		else if (strcmp(argv[index], "--sound-hashes") == 0 && index + 1 < argc)
			sound_hashes_path = argv[++index];
		else if (strcmp(argv[index], "--script") == 0 && index + 1 < argc)
			script_path = argv[++index];
		else if (strcmp(argv[index], "--dump-frame") == 0 && index + 1 < argc)
			dump_frame = strtoull(argv[++index], NULL, 10);
		else if (strcmp(argv[index], "--dump-dir") == 0 && index + 1 < argc)
			dump_dir = argv[++index];
		else if (strcmp(argv[index], "--video-dump") == 0 && index + 1 < argc)
			video_dump_frame = strtoull(argv[++index], NULL, 10);
		else if (strcmp(argv[index], "--video-file") == 0 && index + 1 < argc)
			video_dump_path = argv[++index];
		else if (strcmp(argv[index], "--aim1") == 0 && index + 2 < argc)
		{
			aim1x = atoi(argv[++index]);
			aim1y = atoi(argv[++index]);
		}
		else if (strcmp(argv[index], "--aim2") == 0 && index + 2 < argc)
		{
			aim2x = atoi(argv[++index]);
			aim2y = atoi(argv[++index]);
		}
		else
		{
			fprintf(stderr, "Usage: alien3_headless [--rom FILE_OR_FOLDER] [--frames N] [--script FILE]"
				" [--hashes FILE] [--video-hashes FILE] [--dump-frame N --dump-dir DIR]\n");
			return 1;
		}
	}

	const char *rom_problem = game_rom_set_load(rom);
	if (rom_problem)
	{
		fprintf(stderr, "%s: %s\n", rom, rom_problem);
		return 1;
	}	if (script_path)
	{
		char error[256];

		script = s32_script_load(script_path, error, sizeof(error));
		if (!script)
		{
			fprintf(stderr, "%s\n", error);
			return 1;
		}
		if (!frames_given && s32_script_frames(script))
			frames_wanted = s32_script_frames(script);
	}
	if (hashes_path)
		hashes = fopen(hashes_path, "w");

	s32_video_init(game_tiles, GAME_TILES_SIZE, game_sprites, GAME_SPRITES_SIZE);
	s32_board.frame_shown = on_frame_shown;
	video_sprites_due = s32_board.sprites_due;
	s32_board.sprites_due = on_sprites_due;
	if (video_hashes_path)
		video_hashes = fopen(video_hashes_path, "w");
	if (sound_hashes_path)
		sound_hashes = fopen(sound_hashes_path, "w");

	s32_board.irq_changed = on_irq_changed;
	s32_board.controls_update = on_controls_update;
	s32_board.log = on_log;
	s32_board.frame_timing = false;    /* the translated code advances the board 8 cycles an instruction */
	s32_board.sound_rom = game_sound;
#ifdef GA2_SOUND
	sound_chips_attach();
	sound_cpu_attach();
#endif
	s32_init(game_program);
	if (game_eeprom_valid)
		memcpy(s32_board.eeprom.data, game_eeprom, sizeof(game_eeprom));
	program_reset();

	if (!setjmp(finished))
		program_run();

	if (hashes)
		fclose(hashes);
	if (video_hashes)
		fclose(video_hashes);
	if (sound_hashes)
		fclose(sound_hashes);
	s32_script_free(script);
	printf("frames:            %llu\n", (unsigned long long)s32_board.frame);
	printf("interrupts taken:  %u\n", program_interrupts());
	printf("sprite lists:      %u\n", s32_board.sprite_lists);
	printf("unmapped reads:    %u\n", s32_board.unmapped_reads);
	printf("unmapped writes:   %u\n", s32_board.unmapped_writes);
	printf("final PC:          %08X\n", cpu.pc);
	printf("final SP:          %08X\n", cpu.reg[V60_SP]);
	return 0;
}
