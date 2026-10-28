/**
 *    Name:        io.c
 *    Description: 315-5296 I/O chip, 93C46 serial EEPROM and uPD4701 trackball counters of the System 32 board
 *    Version:     0.1.0
 *    Created:     11-10-2026
 *    Modified:    11-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    Behavior follows MAME sega/315_5296.cpp (hap), machine/eepromser.cpp and machine/eeprom.cpp
 *    (Aaron Giles), machine/upd4701.cpp, and the wiring in sega/segas32.cpp.
 */
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- 93C46, 16-bit organization: 6 address bits, 16 data bits ---- */

enum
{
	EEPROM_IN_RESET,
	EEPROM_WAIT_FOR_START_BIT,
	EEPROM_WAIT_FOR_COMMAND,
	EEPROM_READING_DATA,
	EEPROM_WAIT_FOR_DATA,
	EEPROM_WAIT_FOR_COMPLETION
};

enum
{
	EEPROM_READ,
	EEPROM_WRITE,
	EEPROM_ERASE,
	EEPROM_LOCK,
	EEPROM_UNLOCK,
	EEPROM_WRITE_ALL,
	EEPROM_ERASE_ALL
};

#define EEPROM_ADDRESS_BITS 6
#define EEPROM_DATA_BITS    16

static uint64_t microseconds(uint32_t value)
{
	return (uint64_t)value * S32_CPU_CLOCK / 1000000u;
}

static bool eeprom_ready(void)
{
	return s32_board.cycles >= s32_board.eeprom.busy_until;
}

static void eeprom_busy(uint32_t duration_us)
{
	/* with frame timing the program takes no time, so it could never see a write end */
	s32_board.eeprom.busy_until = s32_board.cycles + (s32_board.frame_timing ? 0 : microseconds(duration_us));
	s32_board.eeprom.dirty = true;
}

/* With S32_LOG_EEPROM set in the environment, every command the chip takes goes to stderr (to compare
   two runs of the program) */
static void eeprom_log(const char *what, int address, uint32_t value)
{
	static int enabled = -1;

	if (enabled < 0)
		enabled = getenv("S32_LOG_EEPROM") != NULL;
	if (enabled)
		fprintf(stderr, "eeprom %s %02X %04X frame %llu\n", what, address, value,
		        (unsigned long long)s32_board.frame);
}

static void eeprom_execute_command(void)
{
	s32_eeprom *chip = &s32_board.eeprom;
	const uint32_t opcode = chip->command_bits >> EEPROM_ADDRESS_BITS;

	chip->address = (int)(chip->command_bits & ((1u << EEPROM_ADDRESS_BITS) - 1));
	chip->bits = 0;
	switch (opcode)
	{
		case 0:
		{
			static const int extended[4] = { EEPROM_LOCK, EEPROM_WRITE_ALL, EEPROM_ERASE_ALL, EEPROM_UNLOCK };
			chip->command = extended[chip->address >> (EEPROM_ADDRESS_BITS - 2)];
			chip->address = 0;
			break;
		}
		case 1: chip->command = EEPROM_WRITE; break;
		case 2: chip->command = EEPROM_READ; break;
		default: chip->command = EEPROM_ERASE; break;
	}
	eeprom_log("command", chip->address, (uint32_t)chip->command);

	switch (chip->command)
	{
		case EEPROM_READ:
			chip->shift = 0;    /* the dummy 0 bit before the data */
			chip->state = EEPROM_READING_DATA;
			break;
		case EEPROM_WRITE:
		case EEPROM_WRITE_ALL:
			chip->shift = 0;
			chip->state = EEPROM_WAIT_FOR_DATA;
			break;
		case EEPROM_ERASE:
			if (chip->locked)
			{
				chip->state = EEPROM_IN_RESET;
				break;
			}
			chip->data[chip->address] = 0xffff;
			eeprom_busy(1000);
			chip->state = EEPROM_WAIT_FOR_COMPLETION;
			break;
		case EEPROM_ERASE_ALL:
			if (chip->locked)
			{
				chip->state = EEPROM_IN_RESET;
				break;
			}
			memset(chip->data, 0xff, sizeof(chip->data));
			eeprom_busy(8000);
			chip->state = EEPROM_WAIT_FOR_COMPLETION;
			break;
		case EEPROM_LOCK:
			chip->locked = true;
			chip->state = EEPROM_IN_RESET;
			break;
		default:
			chip->locked = false;
			chip->state = EEPROM_IN_RESET;
			break;
	}
}

static void eeprom_execute_write(void)
{
	s32_eeprom *chip = &s32_board.eeprom;

	if (chip->locked)
	{
		chip->state = EEPROM_IN_RESET;
		return;
	}
	eeprom_log(chip->locked ? "write-locked" : "write", chip->address, chip->shift & 0xffff);
	if (chip->command == EEPROM_WRITE)
	{
		chip->data[chip->address] = (uint16_t)chip->shift;
		eeprom_busy(1750);
	}
	else
	{
		/* write all assumes a previous erase all, so it can only clear bits */
		for (int index = 0; index < S32_EEPROM_WORDS; index++)
			chip->data[index] &= (uint16_t)chip->shift;
		eeprom_busy(8000);
	}
	chip->state = EEPROM_WAIT_FOR_COMPLETION;
}

static void eeprom_clock_rising(bool cs_rose_now)
{
	s32_eeprom *chip = &s32_board.eeprom;

	switch (chip->state)
	{
		case EEPROM_WAIT_FOR_START_BIT:
			/* a clock edge written together with the CS edge is ignored */
			if (chip->di && eeprom_ready() && !cs_rose_now)
			{
				chip->command_bits = 0;
				chip->bits = 0;
				chip->state = EEPROM_WAIT_FOR_COMMAND;
			}
			break;
		case EEPROM_WAIT_FOR_COMMAND:
			chip->command_bits = (chip->command_bits << 1) | chip->di;
			if (++chip->bits == 2 + EEPROM_ADDRESS_BITS)
				eeprom_execute_command();
			break;
		case EEPROM_READING_DATA:
			if (chip->bits++ == 0)
				chip->shift = (uint32_t)chip->data[chip->address] << (32 - EEPROM_DATA_BITS);
			else
				chip->shift = (chip->shift << 1) | 1;
			break;
		case EEPROM_WAIT_FOR_DATA:
			chip->shift = (chip->shift << 1) | chip->di;
			if (++chip->bits == EEPROM_DATA_BITS)
				eeprom_execute_write();
			break;
		default:
			break;
	}
}

static void eeprom_write_lines(bool di, bool cs, bool clk)
{
	s32_eeprom *chip = &s32_board.eeprom;
	bool cs_rose_now = false;

	/* same order as MAME's port D callbacks: DI, then CS, then CLK */
	chip->di = di;
	if (cs != chip->cs)
	{
		chip->cs = cs;
		if (cs)
		{
			cs_rose_now = true;
			if (chip->state == EEPROM_IN_RESET)
				chip->state = EEPROM_WAIT_FOR_START_BIT;
		}
		else
			chip->state = EEPROM_IN_RESET;
	}
	if (clk != chip->clk)
	{
		chip->clk = clk;
		if (clk && chip->cs)
			eeprom_clock_rising(cs_rose_now);
	}
}

static int eeprom_data_out(void)
{
	const s32_eeprom *chip = &s32_board.eeprom;

	if (chip->state == EEPROM_WAIT_FOR_START_BIT)
		return eeprom_ready() ? 1 : 0;     /* DO doubles as READY/BUSY */
	if (chip->state == EEPROM_READING_DATA)
		return (chip->shift & 0x80000000u) ? 1 : 0;
	return 1;                              /* tristated, pulled up */
}

/* ---- uPD4701 trackball counters ---- */

static uint8_t trackball_read(int which, uint32_t offset)
{
	/* offset bit 1 selects Y, bit 0 the upper byte; no switches are wired */
	const uint16_t position = (offset & 2) ? s32_board.input.trackball_y[which] : s32_board.input.trackball_x[which];
	const uint16_t start = (offset & 2) ? s32_board.trackball_start_y[which] : s32_board.trackball_start_x[which];
	const uint16_t counter = (uint16_t)((position - start) & 0xfff);

	return (offset & 1) ? (uint8_t)(counter >> 8) : (uint8_t)counter;
}

static void trackball_reset(int which)
{
	s32_board.trackball_start_x[which] = s32_board.input.trackball_x[which];
	s32_board.trackball_start_y[which] = s32_board.input.trackball_y[which];
}

/* ---- OKI MSM6253 4-channel ADC (Alien 3's analog gun board) ----
   The V60 latches a channel by writing the low byte at 0xC00050 + 2*channel, then shifts its 8 bits out
   MSB first by reading the same address; the data bit comes back on D7. MAME maps this with
   msm6253_device::address_w / d7_r (umask16(0x00ff)). */
static uint8_t adc_read(void)
{
	const uint8_t bit = (uint8_t)((s32_board.adc_shift & 0x80) >> 7);

	s32_board.adc_shift = (uint8_t)(s32_board.adc_shift << 1);
	return (uint8_t)(0x7f | (bit << 7));
}

static void adc_latch(uint32_t offset)
{
	s32_board.adc_shift = s32_board.input.analog[((offset - 0x50) >> 1) & 3];
}

/* ---- i8255 PPI at 0xC00060 (the 4-player PCB's extra controls) ---- */

static uint8_t ppi_port_input(int port)
{
	switch (port)
	{
		case 0: return s32_board.input.extra1;
		case 1: return s32_board.input.extra2;
		default: return s32_board.input.extra3;
	}
}

static uint8_t ppi_read(int port)
{
	const uint8_t control = s32_board.ppi_control;

	if (port == 0)
		return (control & 0x10) ? ppi_port_input(0) : s32_board.ppi_latch[0];
	if (port == 1)
		return (control & 0x04) ? ppi_port_input(1) : s32_board.ppi_latch[1];

	/* port C: upper nibble belongs to group A (bit 3), lower to group B (bit 1) */
	{
		uint8_t output_mask = 0;
		const uint8_t input = ppi_port_input(2);

		if (!(control & 0x08))
			output_mask |= 0xf0;
		if (!(control & 0x02))
			output_mask |= 0x0f;
		return (uint8_t)((input & ~output_mask) | (s32_board.ppi_latch[2] & output_mask));
	}
}

static void ppi_write(int port, uint8_t data)
{
	const uint8_t control = s32_board.ppi_control;

	if (port == 3)
	{
		if (data & 0x80)
		{
			s32_board.ppi_control = data;             /* mode set */
		}
		else
		{
			/* bit set/reset on port C: bit 3-1 selects the bit, bit 0 sets or clears */
			const int bit = (data >> 1) & 7;
			if (data & 1)
				s32_board.ppi_latch[2] |= (uint8_t)(1u << bit);
			else
				s32_board.ppi_latch[2] &= (uint8_t)~(1u << bit);
		}
		return;
	}
	if (port == 0 && !(control & 0x10))
		s32_board.ppi_latch[0] = data;
	else if (port == 1 && !(control & 0x04))
		s32_board.ppi_latch[1] = data;
	else if (port == 2)
	{
		uint8_t output_mask = 0;
		if (!(control & 0x08))
			output_mask |= 0xf0;
		if (!(control & 0x02))
			output_mask |= 0x0f;
		s32_board.ppi_latch[2] = (uint8_t)((s32_board.ppi_latch[2] & ~output_mask) | (data & output_mask));
	}
}

/* ---- 315-5296 ---- */

static uint8_t io_port_input(int port)
{
	switch (port)
	{
		case 0: return s32_board.input.port_a;
		case 1: return s32_board.input.port_b;
		case 2: return s32_board.input.port_c;
		case 4: return s32_board.input.port_e;
		case 5: return (uint8_t)((s32_board.input.port_f & 0x7f) | (eeprom_data_out() << 7));
		default: return 0xff;
	}
}

static void io_port_output(int port, uint8_t data)
{
	switch (port)
	{
		case 3:    /* port D: coin counters, EEPROM DI (bit 7), CS (bit 5), CLK (bit 6) */
			eeprom_write_lines((data >> 7) & 1, (data >> 5) & 1, (data >> 6) & 1);
			break;
		case 7:    /* port H: external tile bank */
			s32_board.tilebank_external = data;
			break;
		default:
			break;
	}
}

static uint8_t io_chip_read(uint32_t offset)
{
	switch (offset)
	{
		case 0x0: case 0x1: case 0x2: case 0x3: case 0x4: case 0x5: case 0x6: case 0x7:
			if (s32_board.io_direction & (1u << offset))
				return s32_board.io_latch[offset];
			return io_port_input((int)offset);
		case 0x8: return 'S';
		case 0x9: return 'E';
		case 0xa: return 'G';
		case 0xb: return 'A';
		case 0xc: case 0xe: return s32_board.io_cnt;
		default: return s32_board.io_direction;
	}
}

static void io_chip_write(uint32_t offset, uint8_t data)
{
	switch (offset)
	{
		case 0x0: case 0x1: case 0x2: case 0x3: case 0x4: case 0x5: case 0x6: case 0x7:
			if (s32_board.io_direction & (1u << offset))
				io_port_output((int)offset, data);
			s32_board.io_latch[offset] = data;
			break;
		case 0xe:   /* CNT: bit 1 enables the display, bit 2 releases the sound CPU reset */
			if (((s32_board.io_cnt ^ data) & 4) && s32_board.sound_reset_changed)
				s32_board.sound_reset_changed(!(data & 4));
			s32_board.io_cnt = data;
			break;
		case 0xf:
			for (int port = 0; port < 8; port++)
				if ((s32_board.io_direction ^ data) & (1u << port))
					io_port_output(port, (data & (1u << port)) ? s32_board.io_latch[port] : 0);
			s32_board.io_direction = data;
			break;
		default:
			break;
	}
}

uint8_t s32_io_read(uint32_t offset)
{
	if (offset < 0x20)
		return io_chip_read((offset >> 1) & 0xf);
	if (offset >= 0x40 && offset < 0x50)
		return trackball_read((int)((offset - 0x40) >> 3), (offset >> 1) & 3);
	if (offset >= 0x50 && offset < 0x58)
		return adc_read();
	if (offset >= 0x60 && offset < 0x68)
		return ppi_read((int)((offset - 0x60) >> 1) & 3);
	return 0xff;
}

void s32_io_write(uint32_t offset, uint8_t data)
{
	if (offset < 0x20)
		io_chip_write((offset >> 1) & 0xf, data);
	else if (offset >= 0x40 && offset < 0x50)
		trackball_reset((int)((offset - 0x40) >> 3));
	else if (offset >= 0x50 && offset < 0x58)
		adc_latch(offset);
	else if (offset >= 0x60 && offset < 0x68)
		ppi_write((int)((offset - 0x60) >> 1) & 3, data);
}

void s32_io_reset(void)
{
	/* all ports become inputs and the output latches clear */
	s32_board.io_direction = 0;
	s32_board.io_cnt = 0;
	s32_board.ppi_control = 0x9b;    /* 8255 reset: all three ports inputs */
	memset(s32_board.ppi_latch, 0, sizeof(s32_board.ppi_latch));
	if (s32_board.sound_reset_changed)
		s32_board.sound_reset_changed(true);
	memset(s32_board.io_latch, 0, sizeof(s32_board.io_latch));
	for (int port = 0; port < 8; port++)
		io_port_output(port, 0);
}
