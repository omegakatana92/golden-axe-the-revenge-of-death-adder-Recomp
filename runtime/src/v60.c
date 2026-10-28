/**
 *    Name:        v60.c
 *    Description: V60 runtime for the generated code: processor status, the frame loop and its
 *                 interrupts, calls through computed addresses and the multi-step instructions
 *    Version:     0.1.0
 *    Created:     11-10-2026
 *    Modified:    11-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    Behavior follows MAME's V60 core (src/devices/cpu/v60/v60.cpp, op12.hxx, op7a.hxx;
 *    Farfetch'd, R. Belmont) and the frame timing of tools/reference/s32ref.cpp.
 */
#include "v60/cpu.h"
#include "v60/ops.h"
#include "v60/code.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

v60_cpu cpu;
void (*program_frame_done)(void);

/* ---- processor status word (v60ReadPSW / v60WritePSW / v60_update_psw_for_exception) ---- */

static void save_stack(void)
{
	if (cpu.psw & V60_PSW_IS)
		cpu.reg[V60_ISP] = cpu.reg[V60_SP];
	else
		cpu.reg[V60_L0SP + ((cpu.psw >> 24) & 3)] = cpu.reg[V60_SP];
}

static void reload_stack(void)
{
	if (cpu.psw & V60_PSW_IS)
		cpu.reg[V60_SP] = cpu.reg[V60_ISP];
	else
		cpu.reg[V60_SP] = cpu.reg[V60_L0SP + ((cpu.psw >> 24) & 3)];
}

uint32_t v60_read_psw(void)
{
	cpu.psw = (cpu.psw & 0xfffffff0u) | cpu.z | (cpu.s << 1) | (cpu.ov << 2) | (cpu.cy << 3);
	return cpu.psw;
}

void v60_write_psw(uint32_t value)
{
	/* switching between the interrupt stack and a level stack, or between levels, swaps SP */
	const bool switch_stack = ((value ^ cpu.psw) & V60_PSW_IS)
		|| (!(cpu.psw & V60_PSW_IS) && ((value ^ cpu.psw) & 0x03000000u));

	if (switch_stack)
		save_stack();
	cpu.psw = value;
	cpu.z = value & 1;
	cpu.s = (value >> 1) & 1;
	cpu.ov = (value >> 2) & 1;
	cpu.cy = (value >> 3) & 1;
	if (switch_stack)
		reload_stack();
}

void v60_load_privileged(uint32_t number, uint32_t value)
{
	/* LDPR: privileged registers 0-28 live at 36-64; MAME stops on anything else */
	if (number > 28)
	{
		fprintf(stderr, "LDPR to privileged register %u (frame %llu)\n", number, (unsigned long long)cpu.frame);
		exit(3);
	}
	cpu.reg[36 + number] = value;
}

static uint32_t enter_exception(bool interrupt, uint32_t level)
{
	const uint32_t old_psw = v60_read_psw();
	uint32_t psw = old_psw;

	psw &= ~(3u << 24);
	psw |= level << 24;
	psw &= ~((1u << 18) | (1u << 16) | (1u << 27) | (1u << 17) | (1u << 29));
	if (interrupt)
		psw |= V60_PSW_IS;
	psw |= 1u << 31;
	v60_write_psw(psw);
	return old_psw;
}

/* ---- interrupts ---- */

static void take_interrupt(uint32_t resume)
{
	/* the interrupt cycle asks the controller for its vector, as MAME's v60_try_irq does */
	const uint32_t vector = s32_irq_acknowledge() + 0x40u;
	const uint32_t old_psw = enter_exception(true, 0);
	uint32_t handler;

	cpu.interrupts++;
	push32(old_psw);
	push32(resume);
	handler = mem_read32((cpu.reg[V60_SBR] & ~0xfffu) + vector * 4);
	v60_call(handler);
	/* An interrupt handler may replace the return address it was given (Alien 3's VBLANK end does
	   this at 0x86FF9 when a game mode changes): the CPU then continues at the address the handler
	   chose, not at the interrupted instruction. Follow it in the translated code. */
	while (cpu.pc != resume)
	{
		const uint32_t next = cpu.pc & 0xffffff;

		v60_call(next);
	}
}

/* ---- the frame loop ---- */

void v60_frame_wait(uint32_t frames_to_wait)
{
	if ((int8_t)mem_read8(frames_to_wait) >= 0)
		s32_wait();
}

void v60_take_interrupts(uint32_t resume)
{
	/* MAME's core checks the line after every instruction; a handler that leaves another source
	   pending returns into the next one */
	while (cpu.irq_line && (cpu.psw & V60_PSW_IE))
		take_interrupt(resume);
}

void v60_frame_end(void)
{
	cpu.frame++;
	if (program_frame_done)
		program_frame_done();
}

/* Alien 3 with cycle timing no longer needs a special frame hook: A52D5 is an ordinary polling loop,
   and its 8 cycles per test make the VBLANK handler arrive at the right time. Kept as a name for old
   generated trees. */
void alien3_frame_wait(void)
{
}

void v60_tick(uint32_t resume)
{
	s32_advance_to(s32_board.cycles + 8);
	while (cpu.irq_line && (cpu.psw & V60_PSW_IE))
		take_interrupt(resume);
}

/* ---- string instructions (MAME v60 op7a.hxx) ---- */

void v60_search_bytes(uint32_t start, uint32_t length, uint8_t value, bool until_equal)
{
	uint32_t i;

	for (i = 0; i < length; i++)
	{
		const bool equal = mem_read8(start + i) == value;
		if (until_equal ? equal : !equal)
			break;
	}
	cpu.reg[28] = start + i;
	cpu.reg[27] = i;
	/* MAME's comment: this is the opposite of what the V60 manual states */
	cpu.z = (i == length);
}

void v60_search_words(uint32_t start, uint32_t length, uint16_t value, bool until_equal)
{
	uint32_t i;

	for (i = 0; i < length; i++)
	{
		const bool equal = mem_read16(start + i * 2) == value;
		if (until_equal ? equal : !equal)
			break;
	}
	cpu.reg[28] = start + i * 2;
	cpu.reg[27] = i;
	cpu.z = (i == length);
}

static void compare_strings(uint32_t first, uint32_t first_length, uint32_t second, uint32_t second_length,
	bool fill, bool stop, bool words)
{
	const uint32_t unit = words ? 2 : 1;
	const uint32_t destination = first_length < second_length ? first_length : second_length;
	uint32_t i;

	if (fill)
	{
		if (first_length < second_length)
			for (i = first_length; i < second_length; i++)
				words ? mem_write16(first + i * unit, (uint16_t)cpu.reg[26])
				      : mem_write8(first + i, (uint8_t)cpu.reg[26]);
		else if (second_length < first_length)
			for (i = second_length; i < first_length; i++)
				words ? mem_write16(second + i * unit, (uint16_t)cpu.reg[26])
				      : mem_write8(second + i, (uint8_t)cpu.reg[26]);
	}

	cpu.z = 0;
	cpu.s = 0;
	if (stop)
		cpu.cy = 1;
	for (i = 0; i < destination; i++)
	{
		const uint32_t c1 = words ? mem_read16(first + i * unit) : mem_read8(first + i);
		const uint32_t c2 = words ? mem_read16(second + i * unit) : mem_read8(second + i);
		const uint32_t stop_value = words ? (uint16_t)cpu.reg[26] : (uint8_t)cpu.reg[26];

		if (c1 > c2)
		{
			cpu.s = 1;
			break;
		}
		if (c2 > c1)
		{
			cpu.s = 0;
			break;
		}
		if (stop && (c1 == stop_value || c2 == stop_value))
		{
			cpu.cy = 0;
			break;
		}
	}
	cpu.reg[28] = first + i * unit;
	cpu.reg[27] = second + i * unit;

	if (i == destination)
	{
		if (first_length > second_length)
			cpu.s = 1;
		else if (second_length > first_length)
			cpu.s = 0;
		else
			cpu.z = 1;
	}
}

void v60_compare_bytes(uint32_t first, uint32_t first_length, uint32_t second, uint32_t second_length,
	bool fill, bool stop)
{
	compare_strings(first, first_length, second, second_length, fill, stop, false);
}

void v60_compare_words(uint32_t first, uint32_t first_length, uint32_t second, uint32_t second_length,
	bool fill, bool stop)
{
	compare_strings(first, first_length, second, second_length, fill, stop, true);
}

/* ---- the program as a port runs it (s32/program.h) ---- */

void program_reset(void)
{
	v60_reset();
}

void program_run(void)
{
	v60_call(cpu.pc);
}

void program_irq_line(bool asserted)
{
	cpu.irq_line = asserted;
}

uint64_t program_frames(void)
{
	return cpu.frame;
}

uint32_t program_interrupts(void)
{
	return cpu.interrupts;
}

/* ---- calls and jumps through computed addresses ---- */

static const v60_code_entry *find_entry(uint32_t address)
{
	const uint32_t key = address & 0xffffff;
	size_t low = 0;
	size_t high = v60_code_entry_count;

	while (low < high)
	{
		const size_t middle = (low + high) / 2;
		if (v60_code_entries[middle].address < key)
			low = middle + 1;
		else
			high = middle;
	}
	if (low < v60_code_entry_count && v60_code_entries[low].address == key)
		return &v60_code_entries[low];
	return NULL;
}

void v60_call(uint32_t address)
{
	const v60_code_entry *entry = find_entry(address);

	if (entry)
	{
		entry->function();
		return;
	}
	fprintf(stderr, "No translated code starts at %06X (frame %llu)\n", address & 0xffffff,
		(unsigned long long)cpu.frame);
	exit(3);
}

void v60_check_return(uint32_t expected, uint32_t from)
{
	if (cpu.pc == expected)
		return;
	fprintf(stderr, "Code called from %06X returned to %06X instead of %06X (frame %llu)\n",
		from & 0xffffff, cpu.pc & 0xffffff, expected & 0xffffff, (unsigned long long)cpu.frame);
	exit(3);
}

bool v60_returned_to(uint32_t expected, uint32_t from)
{
	(void)from;
	if (cpu.pc == expected)
		return true;

	/* A callee may replace its return address on the V60 stack and return to another function (the
	   "push #handler; ... rsr" idiom Golden Axe uses to chain handlers). Run that function now: when it
	   returns, cpu.pc holds the continuation of whichever C frame is still the right one, and the
	   returning frames above this call site simply unwind until it matches. */
	if (find_entry(cpu.pc))
	{
		const uint32_t target = cpu.pc;
		v60_call(target);
	}
	return false;
}

void v60_jump(uint32_t address)
{
	/* A tail transfer to an address the analysis could not resolve to a function in this one: run the
	   translated function and let the generated code's resume check decide where control goes next. */
	v60_call(address);
}

void v60_unknown_jump(uint32_t address, uint32_t from)
{
	fprintf(stderr, "Jump from %06X to %06X has no translated target (frame %llu)\n",
		from & 0xffffff, address & 0xffffff, (unsigned long long)cpu.frame);
	exit(3);
}

/* ---- setup ---- */

void v60_reset(void)
{
	memset(&cpu, 0, sizeof(cpu));
	cpu.reg[V60_PIR] = 0x00006000;
	cpu.reg[V60_SBR] = 0;
	cpu.reg[V60_SYCW] = 0x00000070;
	cpu.reg[V60_TKCW] = 0x0000e000;
	cpu.reg[V60_PSW2] = 0x0000f002;
	cpu.psw = V60_PSW_IS;
	cpu.pc = 0xfffffff0u;
}

/* ---- DIVX / DIVUX: 64-bit dividend in a register pair or in memory ---- */

static uint64_t load_pair(int reg, uint32_t address)
{
	if (reg >= 0)
		return ((uint64_t)cpu.reg[(reg + 1) & 0x3f] << 32) | cpu.reg[reg];
	return mem_read64(address);
}

static void store_pair(int reg, uint32_t address, uint32_t low, uint32_t high)
{
	if (reg >= 0)
	{
		cpu.reg[reg] = low;
		cpu.reg[(reg + 1) & 0x3f] = high;
	}
	else
	{
		mem_write32(address, low);
		mem_write32(address + 4, high);
	}
}

void v60_divx(uint32_t divisor, int destination_register, uint32_t destination_address)
{
	const int64_t dividend = (int64_t)load_pair(destination_register, destination_address);
	const int32_t by = (int32_t)divisor;
	int32_t quotient;
	int32_t remainder;

	if (by == 0)
	{
		fprintf(stderr, "V60 division by zero (frame %llu)\n", (unsigned long long)cpu.frame);
		exit(3);
	}
	quotient = (int32_t)(dividend / by);
	remainder = (int32_t)(dividend % by);
	cpu.s = ((uint32_t)quotient >> 31) & 1;
	cpu.z = quotient == 0;
	store_pair(destination_register, destination_address, (uint32_t)quotient, (uint32_t)remainder);
}

void v60_divux(uint32_t divisor, int destination_register, uint32_t destination_address)
{
	const uint64_t dividend = load_pair(destination_register, destination_address);
	uint32_t quotient;
	uint32_t remainder;

	if (divisor == 0)
	{
		fprintf(stderr, "V60 division by zero (frame %llu)\n", (unsigned long long)cpu.frame);
		exit(3);
	}
	quotient = (uint32_t)(dividend / divisor);
	remainder = (uint32_t)(dividend % divisor);
	cpu.s = (quotient >> 31) & 1;
	cpu.z = quotient == 0;
	store_pair(destination_register, destination_address, quotient, remainder);
}

/* ---- MOVCU / MOVCFU / MOVCD (op7a.hxx opMOVSTRUB/UH/DB/DH with bStop = 0); R26 is the fill value,
   R27 and R28 end up pointing past the last element moved ---- */

static uint32_t read_element(uint32_t address, int size)
{
	return size == 1 ? mem_read8(address) : mem_read16(address);
}

static void write_element(uint32_t address, uint32_t value, int size)
{
	if (size == 1)
		mem_write8(address, (uint8_t)value);
	else
		mem_write16(address, (uint16_t)value);
}

void v60_move_string(uint32_t source, uint32_t source_length, uint32_t dest, uint32_t dest_length,
                     int element_size, bool fill, bool downward)
{
	const uint32_t count = source_length < dest_length ? source_length : dest_length;
	const uint32_t step = (uint32_t)element_size;
	uint32_t i;

	if (!downward)
	{
		for (i = 0; i < count; i++)
			write_element(dest + i * step, read_element(source + i * step, element_size), element_size);
		cpu.reg[28] = source + i * step;
		cpu.reg[27] = dest + i * step;
		if (fill && source_length < dest_length)
		{
			for (; i < dest_length; i++)
				write_element(dest + i * step, cpu.reg[26], element_size);
			cpu.reg[27] = dest + i * step;
		}
		return;
	}

	for (i = 0; i < count; i++)
		write_element(dest + (count - i - 1) * step, read_element(source + (count - i - 1) * step, element_size), element_size);
	cpu.reg[28] = source + (source_length - i - 1) * step;
	cpu.reg[27] = dest + (dest_length - i - 1) * step;
	if (fill && source_length < dest_length)
	{
		/* MAME's byte and halfword versions differ here; the byte one offsets by the copied count */
		for (; i < dest_length; i++)
		{
			if (element_size == 1)
				write_element(dest + count + (dest_length - i - 1), cpu.reg[26], 1);
			else
				write_element(dest + (dest_length - i - 1) * step, cpu.reg[26], 2);
		}
		cpu.reg[27] = dest + (dest_length - i - 1) * step;
	}
}

/* ---- MOVBSD / MOVBSU (op7a.hxx): bit strings; offsets are already split into byte address + bit ---- */

void v60_move_bits_down(uint32_t source, uint32_t source_offset, uint32_t dest, uint32_t dest_offset, uint32_t length)
{
	uint8_t source_data;
	uint8_t dest_data;

	source_offset += length - 1;
	dest_offset += length - 1;
	source += source_offset / 8;
	dest += dest_offset / 8;
	source_offset &= 7;
	dest_offset &= 7;
	source_data = mem_read8(source);
	dest_data = mem_read8(dest);
	for (uint32_t i = 0; i < length; i++)
	{
		cpu.reg[28] = source;
		cpu.reg[27] = dest;
		dest_data &= (uint8_t)~(1u << dest_offset);
		dest_data |= (uint8_t)(((source_data >> source_offset) & 1) << dest_offset);
		if (source_offset == 0)
		{
			source_offset = 8;
			source--;
			source_data = mem_read8(source);
		}
		if (dest_offset == 0)
		{
			mem_write8(dest, dest_data);
			dest_offset = 8;
			dest--;
			dest_data = mem_read8(dest);
		}
		source_offset--;
		dest_offset--;
	}
	if (dest_offset != 7)
		mem_write8(dest, dest_data);
}

void v60_move_bits_up(uint32_t source, uint32_t source_offset, uint32_t dest, uint32_t dest_offset, uint32_t length)
{
	uint8_t source_data;
	uint8_t dest_data;

	source += source_offset / 8;
	dest += dest_offset / 8;
	source_offset &= 7;
	dest_offset &= 7;
	source_data = mem_read8(source);
	dest_data = mem_read8(dest);
	for (uint32_t i = 0; i < length; i++)
	{
		cpu.reg[28] = source;
		cpu.reg[27] = dest;
		dest_data &= (uint8_t)~(1u << dest_offset);
		dest_data |= (uint8_t)(((source_data >> source_offset) & 1) << dest_offset);
		source_offset++;
		dest_offset++;
		if (source_offset == 8)
		{
			source_offset = 0;
			source++;
			source_data = mem_read8(source);
		}
		if (dest_offset == 8)
		{
			mem_write8(dest, dest_data);
			dest_offset = 0;
			dest++;
			dest_data = mem_read8(dest);
		}
	}
	if (dest_offset != 0)
		mem_write8(dest, dest_data);
}
