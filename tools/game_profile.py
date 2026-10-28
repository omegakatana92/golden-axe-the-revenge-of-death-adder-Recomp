"""
Name:        game_profile.py
Description: what the shared decompiler core (tools/core) needs to know of Golden Axe: The Revenge of
             Death Adder

The core is game-neutral. This module gives it the repository root, the ROM-set module (the board's
region layout), the names the generated files carry, and the decompiler settings for the V60 program.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "core"))

import ga2_rom as rom  # noqa: E402
from decompiler import constants
from decompiler import dataflow
from decompiler import memory
from decompiler import pointers
from decompiler import signatures

ROOT = Path(__file__).resolve().parents[1]
GAME = "Golden Axe: The Revenge of Death Adder (arcade, Sega System 32)"
ROM_SET_TITLE = "Golden Axe: The Revenge of Death Adder (ga2, World, Rev B)"
CPU = "V60"
GUARD = "GA2"

# MAME's V60 core charges every instruction 8 board cycles; the generated code calls v60_tick() before
# each one, so timers, VBLANK and the sprite list happen at their real times.
CYCLE_TIMING = True

# System 32 wires five interrupt sources; the vector table at 0x100000 has entries 0x40-0x44.
VECTORS = (0x40, 0x41, 0x42, 0x43, 0x44)
SKIP_DEFAULT_HANDLER = True

# Work RAM and the registers the whole program keeps. These are refined from the disassembly; the
# scaffold generator does not depend on them yet.
pointers.STRUCTS = {}
pointers.VARIABLES = 0x200000
pointers.WORK_RAM = (0x200000, 0x210000)
memory.WORK_RAM = (0x200000, 0x210000)
memory.VARIABLES = 0x200000
memory.CANONICAL = rom.canonical_address

signatures.CONTEXT = ()
constants.GLOBALS_BASE = 0
constants.GLOBALS_VALUE = 0
dataflow.HANDLER_SLOTS = (0x1C, 0x9C)

# The sound board: the same System 32 Z80/YM3438/RF5C68 board.
SOUND_BLOCK_FETCHES = False
SOUND_HARDWARE = [(0xA000, 0xC000, "SAMPLE_BANK"), (0xC000, 0xD000, "PCM"), (0xD000, 0xE000, "PCM_RAM")]
SOUND_NOTES = {
    "variables": ["/* the sound program's variables, in the RAM it shares with the V60 (E000-FFFF) */"],
    "scoped": ["/* shared RAM as one family of functions uses it */"],
    "tables": ["/* data in the program ROM the code reads */"],
    "hardware": ["/* the sample bank's window and the RF5C68 (read and written through the bus) */"],
}
