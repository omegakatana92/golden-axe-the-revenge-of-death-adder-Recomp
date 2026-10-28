"""
Name:        verify_rom.py
Description: checks that the user-provided ROM is the complete MAME 'ga2' set (World, Rev B)

Usage: python tools/verify_rom.py [folder or .zip]   (default: rom/)
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ga2_rom  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "rom"
    if not source.exists():
        raise SystemExit(f"{source} does not exist")
    found, report = ga2_rom.identify_set(source)
    print("\n".join(report))
    total = len(ga2_rom.ROM_FILES)
    print(f"\n{len(found)}/{total} files of the 'ga2' set")
    return 0 if len(found) == total else 1


if __name__ == "__main__":
    sys.exit(main())
