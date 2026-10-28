"""
Name:        ga2_rom.py
Description: MAME 'ga2' ROM set (Golden Axe: The Revenge of Death Adder, World Rev B) and System 32
             memory region builders

Files are identified by CRC32 + SHA1, not by name, so renamed dumps are accepted. The set may be a
folder or the original .zip. Region layouts follow ROM_START( ga2 ) in MAME
src/mame/sega/segas32.cpp (BSD-3-Clause).

Hardware notes (MAME sega/segas32.cpp):
  - Sega System 32 4-player PCB (833-8933-02), NEC V60 main CPU at 16.10795 MHz.
  - NEC V25 protection MCU (10 MHz) on the 834-8529-02 sub board, with a 4 KB dual-port RAM the V60
    sees at 0xA00000. Its firmware only initialises the DPRAM and halts (see docs/PROTECTION.md);
    the port models that initialisation directly instead of running the V25.
  - Two 93C46-family EEPROMs are on the I/O chips; MAME's set ships no factory image, so settings
    begin blank and are saved beside the executable.
  - Z80 sound CPU, two YM3438, one RF5C68, 4 MB sound ROM.
  - 4-player controls: P1/P2 through the 315-5296, P3/P4 through an i8255 at 0xC00060.
"""
import hashlib
import zipfile
import zlib
from pathlib import Path

# MAME name -> (crc32, sha1, size)
ROM_FILES = {
    "epr-14961b.ic17": ("d9cd8885", "dc9d1f01770bd23ba5959e300badbc5093a149bc", 0x020000),
    "epr-14958b.ic8": ("0be324a3", "5e5f457548906453eaa8d326c353b47353eab73d", 0x020000),
    "epr-15148b.ic18": ("c477a9fd", "a9d60f801c12fd067e5ad1801a92c84edd13bd08", 0x040000),
    "epr-15147b.ic9": ("1bb676ea", "125ffd13204f48be23e20b281c42c2307888c40b", 0x040000),
    "epr-14945.ic36": ("4781d4cb", "bd1b774b3cd0c3e0290c55e426f66d6820d21d0f", 0x010000),
    "mpr-14944.ic35": ("fd4d4b86", "e14b9cd6004bf9ecd902e37b433b828241361b46", 0x100000),
    "mpr-14943.ic34": ("24d40333", "38faf8f3eac317a163e93bd2247fe98189b13d2d", 0x100000),
    "mpr-14942.ic24": ("a89b0e90", "e14c62418eb7f9a2deb2a6dcf635bedc1c73c253", 0x100000),
    "epr-14468-02.u3": ("77634daa", "339169d164b9ed7dc3787b084d33effdc8e9efc1", 0x010000),
    "mpr-14948.ic14": ("75050d4a", "51d6bc9935abcf30af438e69c2cf4e09f57a803f", 0x200000),
    "mpr-14947.ic5": ("b53e62f4", "5aa0f198e6eb070b77b0d180d30c0228a9bc691e", 0x200000),
    "mpr-14949.ic32": ("152c716c", "448d16ea036b66e886119c00af543dfa5e53fd84", 0x200000),
    "mpr-14951.ic30": ("fdb1a534", "3126b595bf69bf9952fedf8f9c6743eb10489dc6", 0x200000),
    "mpr-14953.ic28": ("33bd1c15", "4e16562e3357d4db54b20543073e8f1fd6f74b1f", 0x200000),
    "mpr-14955.ic26": ("e42684aa", "12e0f18a11edb46f09e2e8c5c4ba14170d0cf00d", 0x200000),
    "mpr-14950.ic31": ("15fd0026", "e918984bd60ad63172fe273b31cc9019100228c8", 0x200000),
    "mpr-14952.ic29": ("96f96613", "4c9808866032dab0401de322c28242e8a8775457", 0x200000),
    "mpr-14954.ic27": ("39b2ac9e", "74f4c81d85ab9b4c5e8ae4b4d2c6dff766c482ca", 0x200000),
    "mpr-14956.ic25": ("298fca50", "16e09b19cc7be3dfc8e82b45348e6d1cf2ed5621", 0x200000),
}

# Optional serial EEPROM dump. MAME's ga2 set does not require it.
EEPROM_NAME = "93c45_eeprom.ic76"
EEPROM_CRC = "6e1d9df3"
EEPROM_SHA1 = "2fd818bc393fb96e945fa37a63c8a3c4aff2f79f"
EEPROM_SIZE = 0x80

# Windows of the V60 program image with unique content; the rest are mirrors (ROM_LOAD_x4 / _x2).
# ic17 (128 KB) fills 0x000000-0x07FFFF, ic8 (128 KB) 0x080000-0x0FFFFF, and the two data chips
# interleave over 0x100000-0x1FFFFF.
PROGRAM_AREAS = [
    (0x000000, 0x080000),  # epr-14961b.ic17
    (0x080000, 0x100000),  # epr-14958b.ic8
    (0x100000, 0x200000),  # epr-15148b.ic18 + epr-15147b.ic9, interleaved
]

SPRITE_CHIPS = [
    (0x000000, ["mpr-14949.ic32", "mpr-14951.ic30", "mpr-14953.ic28", "mpr-14955.ic26"]),
    (0x800000, ["mpr-14950.ic31", "mpr-14952.ic29", "mpr-14954.ic27", "mpr-14956.ic25"]),
]

# The V25 protection firmware, after MAME's address-line unscramble. Kept for verification and for
# docs/PROTECTION.md; the game runtime does not execute it.
MCU_NAME = "epr-14468-02.u3"


def read_files(source):
    """Return {original_name: bytes} from a folder or a .zip."""
    source = Path(source)
    contents = {}
    if source.is_file() and source.suffix.lower() == ".zip":
        with zipfile.ZipFile(source) as archive:
            for entry in archive.infolist():
                if not entry.is_dir():
                    contents[Path(entry.filename).name] = archive.read(entry)
        return contents
    for path in sorted(source.iterdir()):
        if path.is_file() and path.suffix.lower() == ".zip":
            # rom/ may hold the set's zip; read its entries too
            with zipfile.ZipFile(path) as archive:
                for entry in archive.infolist():
                    if not entry.is_dir():
                        contents[Path(entry.filename).name] = archive.read(entry)
        elif path.is_file():
            contents[path.name] = path.read_bytes()
    return contents


def identify_set(source):
    """Return ({mame_name: bytes}, [report lines]), matching every file by its hashes."""
    by_crc = {crc: name for name, (crc, _, _) in ROM_FILES.items()}
    found = {}
    report = []
    for original_name, data in read_files(source).items():
        crc = format(zlib.crc32(data) & 0xFFFFFFFF, "08x")
        mame_name = by_crc.get(crc)
        if mame_name and ROM_FILES[mame_name][1] == hashlib.sha1(data).hexdigest():
            found[mame_name] = data
            report.append(f"OK      {original_name:20} = {mame_name}")
        elif original_name == EEPROM_NAME or (crc == EEPROM_CRC and len(data) == EEPROM_SIZE):
            report.append(f"OK      {original_name:20} = {EEPROM_NAME} (optional EEPROM)")
        else:
            report.append(f"--      {original_name:20} not part of the set (crc {crc})")
    for mame_name in sorted(set(ROM_FILES) - set(found)):
        report.append(f"MISSING {mame_name}")
    return found, report


def load_set(source):
    """Like identify_set, but stops when a required file is missing."""
    found, _ = identify_set(source)
    missing = sorted(set(ROM_FILES) - set(found))
    if missing:
        raise SystemExit("The 'ga2' ROM set is incomplete. Missing: " + ", ".join(missing))
    return found


def interleave16(even, odd):
    """ROM_LOAD16_BYTE: even bytes from one chip, odd bytes from the other."""
    result = bytearray(len(even) * 2)
    result[0::2] = even
    result[1::2] = odd
    return result


def build_program(rom):
    """mainpcb:maincpu region (2 MB, little endian) as the V60 sees it at 0x000000-0x1FFFFF."""
    image = bytearray(0x200000)
    image[0x000000:0x080000] = rom["epr-14961b.ic17"] * 4
    image[0x080000:0x100000] = rom["epr-14958b.ic8"] * 4
    interleaved = interleave16(rom["epr-15148b.ic18"] * 2, rom["epr-15147b.ic9"] * 2)
    image[0x100000:0x200000] = interleaved
    return bytes(image)


def build_tiles(rom):
    """mainpcb:tiles region (4 MB): 32768 tiles of 16x16 pixels at 4 bpp, 128 bytes each."""
    return bytes(interleave16(rom["mpr-14948.ic14"], rom["mpr-14947.ic5"]))


def build_sprites(rom):
    """mainpcb:sprites region (16 MB) in file byte order: every 32-bit word is big endian."""
    image = bytearray(b"\xff" * 0x1000000)
    for base, chips in SPRITE_CHIPS:
        for lane, name in enumerate(chips):
            data = rom[name]
            end = base + (len(data) // 2) * 8
            image[base + lane * 2:end:8] = data[0::2]
            image[base + lane * 2 + 1:end:8] = data[1::2]
    return bytes(image)


def build_sound(rom):
    """mainpcb:soundcpu region (4 MB): Z80 program (x16) and three 1 MB banks of PCM samples."""
    image = bytearray(0x400000)
    image[0x000000:0x100000] = rom["epr-14945.ic36"] * 16
    image[0x100000:0x200000] = rom["mpr-14944.ic35"]
    image[0x200000:0x300000] = rom["mpr-14943.ic34"]
    image[0x300000:0x400000] = rom["mpr-14942.ic24"]
    return bytes(image)


def decrypt_mcu(rom):
    """The V25 firmware with MAME's address lines unscrambled (segas32_m.cpp: decrypt_protrom).

    The V25 stays in secure mode and its opcode bytes are still translated by ga2_opcode_table at
    fetch time; the bytes here are the address-decoded image MAME's device sees.
    """
    source = rom[MCU_NAME]
    image = bytearray(0x10000)
    # bitswap<16>(i, 14, 11, 15, 12, 13, 4, 3, 7, 5, 10, 2, 8, 9, 6, 1, 0)
    order = (14, 11, 15, 12, 13, 4, 3, 7, 5, 10, 2, 8, 9, 6, 1, 0)
    for i in range(0x10000):
        source_index = 0
        for bit in order:
            source_index = (source_index << 1) | ((i >> bit) & 1)
        image[i] = source[source_index]
    return bytes(image)


def chips_from_regions(regions):
    """The set's chips back from the regions build_program, build_tiles, build_sprites and build_sound
    lay out ({region name: bytes}, every one of the four): {mame_name: bytes}. Each chip is read from
    the copy its region's builder writes first; the mirrors are not read."""
    program, tiles, sprites, sound = (regions[name] for name in ("program", "tiles", "sprites", "sound"))
    chips = {
        "epr-14961b.ic17": program[0x000000:0x020000],
        "epr-14958b.ic8": program[0x080000:0x0A0000],
        "epr-15148b.ic18": program[0x100000:0x200000:2][0:0x40000],
        "epr-15147b.ic9": program[0x100001:0x200000:2][0:0x40000],
        "mpr-14948.ic14": tiles[0::2],
        "mpr-14947.ic5": tiles[1::2],
        "epr-14945.ic36": sound[0x000000:0x010000],
        "mpr-14944.ic35": sound[0x100000:0x200000],
        "mpr-14943.ic34": sound[0x200000:0x300000],
        "mpr-14942.ic24": sound[0x300000:0x400000],
    }
    for base, names in SPRITE_CHIPS:
        for lane, name in enumerate(names):
            size = ROM_FILES[name][2]
            end = base + (size // 2) * 8
            chip = bytearray(size)
            chip[0::2] = sprites[base + lane * 2:end:8]
            chip[1::2] = sprites[base + lane * 2 + 1:end:8]
            chips[name] = chip
    return {name: bytes(chip) for name, chip in chips.items()}


def canonical_address(address):
    """Map a V60 address to its copy inside PROGRAM_AREAS, or None if it is not ROM.

    The bus is 24 bits wide; 0xF00000-0xFFFFFF mirrors the first MB of the region (MAME's map).
    """
    address &= 0xFFFFFF
    if address >= 0xF00000:
        address -= 0xF00000
    elif address >= 0x200000:
        return None
    return address


class ProgramMemory:
    """V60 view of the program ROM for the disassembler: mirrors folded, anything else reads 0xFF."""

    def __init__(self, image):
        self.image = image

    def r8(self, address):
        offset = canonical_address(address)
        return self.image[offset] if offset is not None else 0xFF

    def r16(self, address):
        return self.r8(address) | (self.r8(address + 1) << 8)

    def r32(self, address):
        return self.r16(address) | (self.r16(address + 2) << 16)
