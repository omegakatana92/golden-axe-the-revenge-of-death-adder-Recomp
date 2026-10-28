/**
 *    Name:        main.c
 *    Description: Golden Axe: The Revenge of Death Adder in a window: SDL2 picture, sound and controls
 *
 *    license:PolyForm-Noncommercial-1.0.0 AND BSD-3-Clause (see LICENSE and THIRD_PARTY_NOTICES.md)
 *
 *    The game itself is the C translated from the player's own ga2 ROM (tools/recompile.py) plus the
 *    decompiled Z80 sound driver (src/sound). The ROM set is read at startup for the data, graphics,
 *    samples and sound program the original reads from its boards; no ROM is included with this port.
 *
 *    Controls (keyboard)
 *      P1: W A S D move, J K L attack/jump/magic, 1 start, 5 coin
 *      P2: arrows move, Numpad 1 2 3 buttons, 2 start, 6 coin
 *      P3: T F G H move, U I O buttons, 3 start, 7 coin
 *      P4: Numpad 8 4 5 6 move, Numpad 7 9 3 buttons, 4 start, 8 coin
 *      F2 test menu, 9 service, F11 / Alt+Enter full screen, F3 smooth/sharp pixels, Escape quit
 *
 *    Gamepads (Xbox layout, Steam Deck built-in controls, arcade sticks)
 *      left stick or D-pad move, A/B/X buttons, Start start, Back coin, RT/LT also attack/jump.
 *      Up to four controllers, assigned in the order they are connected (player 1 first).
 */
#include "game/data.h"
#include "s32/machine.h"
#include "s32/program.h"
#include "s32/video.h"
#include "sound/chips.h"

#include <SDL.h>

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLAYERS            4
#define OUTPUT_RATE        48000
#define QUEUE_FRAMES_MAX   6
#define ROM_FILE           "ga2.zip"
#define EEPROM_FILE        "ga2.eeprom"

static const double FM_RATE = S32_SOUND_CLOCK / 144.0;
static const double PCM_RATE = S32_PCM_CLOCK / 384.0;

static jmp_buf finished;
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_AudioDeviceID audio;
static SDL_GameController *controllers[PLAYERS];
static bool full_screen;
static bool smooth = true;
static bool square_pixels;             /* F4: 416x224 native pixels instead of the arcade 4:3 monitor */
static uint64_t next_frame;
static char eeprom_path[1024];
static uint64_t smoke_frames;          /* --smoke-test: run this many frames and exit */

typedef struct resampler
{
	double step;
	double position;
	int32_t *samples;
	size_t count;
	size_t capacity;
} resampler;

static resampler chips[3];

/* ---- files ---- */

static uint8_t *load_file(const char *path, size_t *size)
{
	FILE *input = fopen(path, "rb");
	uint8_t *data;
	long length;

	if (!input)
		return NULL;
	fseek(input, 0, SEEK_END);
	length = ftell(input);
	fseek(input, 0, SEEK_SET);
	data = length > 0 ? (uint8_t *)malloc((size_t)length) : NULL;
	if (data && fread(data, 1, (size_t)length, input) != (size_t)length)
	{
		free(data);
		data = NULL;
	}
	fclose(input);
	if (size)
		*size = data ? (size_t)length : 0;
	return data;
}

static void save_eeprom(void)
{
	FILE *output;

	if (!s32_board.eeprom.dirty)
		return;
	output = fopen(eeprom_path, "wb");
	if (!output)
		return;
	fwrite(s32_board.eeprom.data, 1, sizeof(s32_board.eeprom.data), output);
	fclose(output);
	s32_board.eeprom.dirty = false;
}

static void load_eeprom(void)
{
	size_t size;
	uint8_t *data = load_file(eeprom_path, &size);

	if (data && size == sizeof(s32_board.eeprom.data))
		memcpy(s32_board.eeprom.data, data, size);
	free(data);
}

/* ---- picture ---- */

static void on_frame_shown(void)
{
	int width;
	const uint32_t *pixels;
	SDL_Rect source, dest;
	int ww, wh;
	double scale;

	s32_video_draw_frame();
	pixels = s32_video_frame(&width);
	SDL_UpdateTexture(texture, NULL, pixels, S32_VIDEO_MAX_WIDTH * (int)sizeof(uint32_t));
	SDL_GetWindowSize(window, &ww, &wh);
	if (square_pixels)
	{
		scale = (double)ww / width < (double)wh / S32_VIDEO_HEIGHT
			? (double)ww / width : (double)wh / S32_VIDEO_HEIGHT;
		dest.w = (int)(width * scale + 0.5);
		dest.h = (int)(S32_VIDEO_HEIGHT * scale + 0.5);
	}
	else
	{
		/* the cabinet's 4:3 monitor: 416x224 pixels are wider than square */
		dest.w = ww;
		dest.h = ww * 3 / 4;
		if (dest.h > wh)
		{
			dest.h = wh;
			dest.w = wh * 4 / 3;
		}
	}
	dest.x = (ww - dest.w) / 2;
	dest.y = (wh - dest.h) / 2;
	source.x = 0;
	source.y = 0;
	source.w = width;
	source.h = S32_VIDEO_HEIGHT;
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, &source, &dest);
	SDL_RenderPresent(renderer);
}

static void toggle_full_screen(void)
{
	full_screen = !full_screen;
	SDL_SetWindowFullscreen(window, full_screen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

/* ---- controls ---- */

static void read_events(void)
{
	SDL_Event event;

	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_QUIT:
				longjmp(finished, 1);
			case SDL_KEYDOWN:
				if (event.key.repeat)
					break;
				if (event.key.keysym.sym == SDLK_ESCAPE)
					longjmp(finished, 1);
				if (event.key.keysym.sym == SDLK_F11
					|| (event.key.keysym.sym == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT)))
					toggle_full_screen();
				if (event.key.keysym.sym == SDLK_F3)
					smooth = !smooth;
				if (event.key.keysym.sym == SDLK_F4)
					square_pixels = !square_pixels;
				break;
			case SDL_CONTROLLERDEVICEADDED:
				for (int player = 0; player < PLAYERS; player++)
					if (!controllers[player])
					{
						controllers[player] = SDL_GameControllerOpen(event.cdevice.which);
						break;
					}
				break;
			case SDL_CONTROLLERDEVICEREMOVED:
				for (int player = 0; player < PLAYERS; player++)
					if (controllers[player]
						&& SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controllers[player])) == event.cdevice.which)
					{
						SDL_GameControllerClose(controllers[player]);
						controllers[player] = NULL;
					}
				break;
			default:
				break;
		}
	}
	SDL_SetTextureScaleMode(texture, smooth ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
}

/* A pad's 8-way stick: the D-pad first, then the left stick past a dead zone. */
static void pad_direction(SDL_GameController *pad, bool *up, bool *down, bool *left, bool *right)
{
	const int x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
	const int y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
	const int dead = 12000;

	*up = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) || y < -dead;
	*down = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || y > dead;
	*left = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || x < -dead;
	*right = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || x > dead;
}

static void on_controls_update(void)
{
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	s32_inputs *input = &s32_board.input;
	uint8_t ports[PLAYERS] = { 0xff, 0xff, 0xff, 0xff };
	bool start[PLAYERS] = { false, false, false, false };
	bool coin1 = false, coin2 = false;
	bool service, test;

	read_events();

	for (int player = 0; player < PLAYERS; player++)
	{
		SDL_GameController *pad = controllers[player];
		bool up, down, left, right;

		if (!pad)
			continue;
		pad_direction(pad, &up, &down, &left, &right);
		if (up) ports[player] &= (uint8_t)~0x20;
		if (down) ports[player] &= (uint8_t)~0x10;
		if (left) ports[player] &= (uint8_t)~0x80;
		if (right) ports[player] &= (uint8_t)~0x40;
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A)
			|| SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 8000)
			ports[player] &= (uint8_t)~0x01;
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B)
			|| SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 8000)
			ports[player] &= (uint8_t)~0x02;
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X))
			ports[player] &= (uint8_t)~0x04;
		start[player] |= SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START) != 0;
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK))
		{
			if (player < 2) coin1 = true;
			else coin2 = true;
		}
	}

	/* P1: WASD, JKL */
#define KEY(port, bit, key) do { if (keys[(key)]) (port) &= (uint8_t)~(bit); } while (0)
	KEY(ports[0], 0x20, SDL_SCANCODE_W);
	KEY(ports[0], 0x10, SDL_SCANCODE_S);
	KEY(ports[0], 0x80, SDL_SCANCODE_A);
	KEY(ports[0], 0x40, SDL_SCANCODE_D);
	KEY(ports[0], 0x01, SDL_SCANCODE_J);
	KEY(ports[0], 0x02, SDL_SCANCODE_K);
	KEY(ports[0], 0x04, SDL_SCANCODE_L);
	start[0] |= keys[SDL_SCANCODE_1] != 0;
	coin1 |= keys[SDL_SCANCODE_5] != 0;

	/* P2: arrows and the numeric keypad */
	KEY(ports[1], 0x20, SDL_SCANCODE_UP);
	KEY(ports[1], 0x10, SDL_SCANCODE_DOWN);
	KEY(ports[1], 0x80, SDL_SCANCODE_LEFT);
	KEY(ports[1], 0x40, SDL_SCANCODE_RIGHT);
	KEY(ports[1], 0x01, SDL_SCANCODE_KP_1);
	KEY(ports[1], 0x02, SDL_SCANCODE_KP_2);
	KEY(ports[1], 0x04, SDL_SCANCODE_KP_3);
	start[1] |= keys[SDL_SCANCODE_2] != 0;
	coin2 |= keys[SDL_SCANCODE_6] != 0;

	/* P3: TFGH, UIO */
	KEY(ports[2], 0x20, SDL_SCANCODE_T);
	KEY(ports[2], 0x10, SDL_SCANCODE_G);
	KEY(ports[2], 0x80, SDL_SCANCODE_F);
	KEY(ports[2], 0x40, SDL_SCANCODE_H);
	KEY(ports[2], 0x01, SDL_SCANCODE_U);
	KEY(ports[2], 0x02, SDL_SCANCODE_I);
	KEY(ports[2], 0x04, SDL_SCANCODE_O);
	start[2] |= keys[SDL_SCANCODE_3] != 0;
	coin1 |= keys[SDL_SCANCODE_7] != 0;

	/* P4: keypad 8456, 793 */
	KEY(ports[3], 0x20, SDL_SCANCODE_KP_8);
	KEY(ports[3], 0x10, SDL_SCANCODE_KP_5);
	KEY(ports[3], 0x80, SDL_SCANCODE_KP_4);
	KEY(ports[3], 0x40, SDL_SCANCODE_KP_6);
	KEY(ports[3], 0x01, SDL_SCANCODE_KP_7);
	KEY(ports[3], 0x02, SDL_SCANCODE_KP_9);
	KEY(ports[3], 0x04, SDL_SCANCODE_KP_3);
	start[3] |= keys[SDL_SCANCODE_4] != 0;
	coin2 |= keys[SDL_SCANCODE_8] != 0;
#undef KEY

	service = keys[SDL_SCANCODE_9] != 0;
	test = keys[SDL_SCANCODE_F2] != 0;

	input->port_a = ports[0];
	input->port_b = ports[1];
	input->extra1 = ports[2];
	input->extra2 = ports[3];
	input->port_e = (uint8_t)(0xff
		& ~(service ? 0x01 : 0) & ~(test ? 0x02 : 0)
		& ~(coin1 ? 0x04 : 0) & ~(coin2 ? 0x08 : 0)
		& ~(start[0] ? 0x10 : 0) & ~(start[1] ? 0x20 : 0));
	input->extra3 = (uint8_t)(0xff & ~(start[2] ? 0x01 : 0) & ~(start[3] ? 0x02 : 0));

	/* GA2_AUTOPLAY: deterministic smoke test (AppImage launch check, CI), no keyboard focus needed */
	if (getenv("GA2_AUTOPLAY"))
	{
		const uint64_t frame = s32_board.frame;
		if (frame == 60)
			input->port_e &= (uint8_t)~0x04;          /* coin 1 */
		if (frame == 90)
			input->port_e &= (uint8_t)~0x10;          /* start 1 */
		if (frame >= 150 && frame < 158)
			input->port_a &= (uint8_t)~0x01;          /* choose / attack */
		if (frame >= 300 && frame < 900 && (frame / 120) % 2 == 0)
			input->port_a &= (uint8_t)~0x40;          /* walk right */
		if (frame >= 400 && frame % 90 == 0 && frame < 800)
			input->port_a &= (uint8_t)~0x01;          /* attack */
	}
}

/* ---- sound ---- */

static void keep(resampler *chip, const int32_t *samples, size_t count)
{
	if (chip->count + count > chip->capacity)
	{
		chip->capacity = (chip->count + count) * 2;
		chip->samples = (int32_t *)realloc(chip->samples, chip->capacity * 2 * sizeof(int32_t));
	}
	memcpy(chip->samples + chip->count * 2, samples, count * 2 * sizeof(int32_t));
	chip->count += count;
}

static void sample_at(const resampler *chip, double position, double out[2])
{
	const size_t index = (size_t)position;
	const double fraction = position - (double)index;

	for (int side = 0; side < 2; side++)
	{
		const double here = chip->samples[index * 2 + side];
		const double next = chip->samples[(index + 1) * 2 + side];
		out[side] = here + (next - here) * fraction;
	}
}

static void drop_used(resampler *chip)
{
	const size_t used = (size_t)chip->position;

	if (!used)
		return;
	memmove(chip->samples, chip->samples + used * 2, (chip->count - used) * 2 * sizeof(int32_t));
	chip->count -= used;
	chip->position -= (double)used;
}

static void play_frame(void)
{
#ifdef GA2_SOUND
	static int16_t mix[OUTPUT_RATE / 20 * 2];
	size_t made = 0;

	sound_chips_end_frame(s32_board.cycles);
	for (int chip = 0; chip < 3; chip++)
	{
		size_t count;
		const int32_t *samples = sound_chips_frame_samples(chip, &count);
		keep(&chips[chip], samples, count);
	}
	while (made < OUTPUT_RATE / 20)
	{
		double fm0[2], fm1[2], pcm[2];

		if (chips[0].position + 1 >= (double)chips[0].count || chips[1].position + 1 >= (double)chips[1].count
			|| chips[2].position + 1 >= (double)chips[2].count)
			break;
		sample_at(&chips[0], chips[0].position, fm0);
		sample_at(&chips[1], chips[1].position, fm1);
		sample_at(&chips[2], chips[2].position, pcm);
		for (int side = 0; side < 2; side++)
		{
			const double value = 0.30 * (fm0[side] + fm1[side]) + 0.40 * pcm[side];
			mix[made * 2 + side] = (int16_t)(value > 32767 ? 32767 : (value < -32768 ? -32768 : value));
		}
		for (int chip = 0; chip < 3; chip++)
			chips[chip].position += chips[chip].step;
		made++;
	}
	for (int chip = 0; chip < 3; chip++)
		drop_used(&chips[chip]);
	if (audio && SDL_GetQueuedAudioSize(audio) < (Uint32)(OUTPUT_RATE / 60 * 4 * QUEUE_FRAMES_MAX))
		SDL_QueueAudio(audio, mix, (Uint32)(made * 2 * sizeof(int16_t)));
#endif
}

/* ---- frame ---- */

static void wait_for_frame(void)
{
	const uint64_t frequency = SDL_GetPerformanceFrequency();
	uint64_t now = SDL_GetPerformanceCounter();

	next_frame += frequency / S32_FRAME_RATE;
	if (now > next_frame + frequency / 4)
		next_frame = now;
	while ((now = SDL_GetPerformanceCounter()) < next_frame)
	{
		const uint64_t left = (next_frame - now) * 1000 / frequency;
		if (left > 2)
			SDL_Delay((Uint32)(left - 1));
	}
}

static void on_frame_done(void)
{
	play_frame();
	if (s32_board.frame % 600 == 0)
		save_eeprom();
	if (smoke_frames && s32_board.frame + 1 >= smoke_frames)
		longjmp(finished, 1);
	wait_for_frame();
}

/* The sprite list goes out 50 us after VBLANK ends: one complete board frame. The video library owns
   the callback; chain it so the port's audio and pacing happen at the same point as the reference. */
static void (*video_sprites_due)(bool erase, bool draw);

static void on_sprites_due(bool erase, bool draw)
{
	video_sprites_due(erase, draw);
	on_frame_done();
}

static void on_irq_changed(bool asserted)
{
	program_irq_line(asserted);
}

static void on_log(const char *message)
{
	(void)message;
}

static int fail(const char *what, const char *detail)
{
	char message[1200];

	snprintf(message, sizeof(message), "%s\n%s", what, detail);
	fprintf(stderr, "%s\n", message);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Golden Axe: The Revenge of Death Adder", message, NULL);
	return 1;
}

static char *saves_folder(void)
{
#ifdef _WIN32
	return SDL_GetBasePath();
#else
	return SDL_GetPrefPath("", "GoldenAxe2");
#endif
}

static bool load_rom(const char *base, char *path, size_t size)
{
	const char *places[3];
	char candidate[1024];
	int count = 0;

	/* GA2_ROM lets the AppImage launcher point at the folder that holds the user's zip; SDL's base
	   path is inside the mounted AppImage, and the current folder covers a terminal launch. */
	if (getenv("GA2_ROM"))
		places[count++] = getenv("GA2_ROM");
	if (base && *base)
		places[count++] = base;
	places[count++] = ".";
	for (int place = 0; place < count; place++)
	{
		const size_t length = strlen(places[place]);
		const char *slash = length && places[place][length - 1] == '/' ? "" : "/";

		snprintf(path, size, "%s%s%s", places[place], slash, ROM_FILE);
		if (game_rom_set_load(path) == NULL)
			return true;
		snprintf(candidate, sizeof(candidate), "%s%srom", places[place], slash);
		if (game_rom_set_load(candidate) == NULL)
		{
			snprintf(path, size, "%s", candidate);
			return true;
		}
	}
	return false;
}

int main(int argc, char **argv)
{
	SDL_AudioSpec wanted;
	char rom_path[1024];
	char *base, *saves;

	(void)argc;
	(void)argv;
	for (int index = 1; index < argc; index++)
	{
		if (strcmp(argv[index], "--smoke-test") == 0)
		{
			smoke_frames = 120;
			SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
			SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
		}
		else if (strcmp(argv[index], "--frames") == 0 && index + 1 < argc)
			smoke_frames = strtoull(argv[++index], NULL, 10);
		else if (strcmp(argv[index], "--rom") == 0 && index + 1 < argc)
			SDL_setenv("GA2_ROM", argv[++index], 1);
	}
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
		return fail("SDL could not start.", SDL_GetError());
	base = SDL_GetBasePath();
	saves = saves_folder();
	snprintf(eeprom_path, sizeof(eeprom_path), "%s" EEPROM_FILE, saves ? saves : "");
	if (!load_rom(base ? base : "", rom_path, sizeof(rom_path)))
	{
		SDL_free(base);
		SDL_free(saves);
		return fail("The Golden Axe: The Revenge of Death Adder ROM set was not found.",
			"Put your MAME ga2.zip (World, Rev B, 19 files) beside the executable, or in a rom/ folder.");
	}
	SDL_free(base);
	SDL_free(saves);

	window = SDL_CreateWindow("Golden Axe: The Revenge of Death Adder", SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, 960, 672, SDL_WINDOW_RESIZABLE);
	renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : NULL;
	if (window && !renderer)
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
		S32_VIDEO_MAX_WIDTH, S32_VIDEO_HEIGHT) : NULL;
	if (!texture)
		return fail("The window could not be opened.", SDL_GetError());
	SDL_RaiseWindow(window);
	SDL_SetWindowInputFocus(window);
	SDL_SetTextureScaleMode(texture, smooth ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
	SDL_ShowCursor(SDL_DISABLE);

	SDL_zero(wanted);
	wanted.freq = OUTPUT_RATE;
	wanted.format = AUDIO_S16SYS;
	wanted.channels = 2;
	wanted.samples = 1024;
	audio = SDL_OpenAudioDevice(NULL, 0, &wanted, NULL, 0);
#ifdef GA2_SOUND
	if (audio)
		SDL_PauseAudioDevice(audio, 0);
#endif
	chips[0].step = chips[1].step = FM_RATE / OUTPUT_RATE;
	chips[2].step = PCM_RATE / OUTPUT_RATE;

	s32_board.irq_changed = on_irq_changed;
	s32_board.controls_update = on_controls_update;
	s32_board.log = on_log;
	s32_board.frame_timing = false;    /* the translated code advances the board 8 cycles an instruction */
	s32_board.sound_rom = game_sound;
	s32_video_init(game_tiles, GAME_TILES_SIZE, game_sprites, GAME_SPRITES_SIZE);
	s32_board.frame_shown = on_frame_shown;
	video_sprites_due = s32_board.sprites_due;
	s32_board.sprites_due = on_sprites_due;
#ifdef GA2_SOUND
	sound_chips_attach();
	sound_cpu_attach();
#endif
	s32_init(game_program);
	if (game_eeprom_valid)
		memcpy(s32_board.eeprom.data, game_eeprom, sizeof(game_eeprom));
	load_eeprom();
	program_reset();
	next_frame = SDL_GetPerformanceCounter();

	if (!setjmp(finished))
		program_run();

	save_eeprom();
	if (audio)
		SDL_CloseAudioDevice(audio);
	SDL_Quit();
	return 0;
}
