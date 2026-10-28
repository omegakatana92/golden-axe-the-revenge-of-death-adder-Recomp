"""
Name:        fix_sound_dispatch.py
Description: complete the two Z80 command dispatchers in the generated sound C

The sound program's `sound_0473` and `sound_1094` routines push a continuation address and jump through
a table whose handlers return to it. The decompiler's control-flow graph cannot see a return address that
only the Z80 stack knows, so it emits each handler as a normal `return`. The generated C then handles one
command and returns without advancing to the continuation, which leaves the sound driver silent or stuck
on the first command.

This pass rewrites top-level case returns in those two generated functions only:

  sound_0473: continue at 0x049F (increment DE, store it at object+2/+3, loop to 0x0478)
  sound_1094: continue at 0x10AD (increment DE, loop to 0x1099)

Returns inside nested `if` blocks are left alone: the generated case bodies use them for real early exits,
and changing them would break the case's control flow.

The original ROM, `sound_symbols.txt` and the toolkit are unchanged; only the generated C is adjusted.

Usage: python tools/fix_sound_dispatch.py [src/sound/main.c]
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PATH = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "src" / "sound" / "main.c"


def split_function(text, signature):
    start = text.find(signature)
    if start < 0:
        raise SystemExit(f"{PATH}: no {signature}")
    end = text.find("\n}\n", start + len(signature))
    if end < 0:
        raise SystemExit(f"{PATH}: unterminated {signature}")
    return start, end + 3


def rewrite_tail(tail, continuation, call_prefix="sound_"):
    lines = []
    depth = 0
    changed = False
    for line in tail.splitlines(keepends=True):
        stripped = line.strip()
        if depth == 1 and stripped.startswith("return"):
            expression = stripped[len("return"):].strip().rstrip(";").strip()
            indent = line[:len(line) - len(line.lstrip())]
            if expression and not expression.startswith(call_prefix):
                lines.append(line)          # leave an unexpected value return alone
            else:
                if expression:
                    lines.append(f"{indent}{expression};\n")
                lines.append(f"{indent}{continuation}\n")
                changed = True
            depth += line.count("{") - line.count("}")
            continue
        lines.append(line)
        depth += line.count("{") - line.count("}")
    return "".join(lines), changed


def patch_0473(s):
    if "L_0473_loop" in s:
        return s, False
    start, end = split_function(s, "uint16_t sound_0473(uint16_t hl)")
    body = s[start:end]
    body = body.replace("uint16_t sound_0473(uint16_t hl)\n{",
                        "uint16_t sound_0473(uint16_t hl)\n{\n\tunsigned guard = 0;", 1)
    switch = body.find("switch (hl_2)")
    if switch < 0:
        raise SystemExit(f"{PATH}: sound_0473 has no switch")
    head, tail = body[:switch], body[switch:]
    head = head.replace("a = z80_read8(de);",
                        "L_0473_loop:\n\tif (++guard > 256) { de++; z80_write8((hl & ~0xFFu) | 2u, (uint8_t)de); z80_write8((hl & ~0xFFu) | 3u, de >> 8); return hl; }\n\ta = z80_read8(de);", 1)
    continuation = ("de++;\n"
                    "\tz80_write8((hl & ~0xFFu) | 2u, (uint8_t)de);\n"
                    "\tz80_write8((hl & ~0xFFu) | 3u, de >> 8);\n"
                    "\tgoto L_0473_loop;")
    tail, changed = rewrite_tail(tail, continuation)
    return s[:start] + head + tail + s[end:], changed


def patch_1094(s):
    if "L_1094_loop" in s:
        return s, False
    start, end = split_function(s, "void sound_1094(uint16_t hl, uint16_t ix)")
    body = s[start:end]
    body = body.replace("void sound_1094(uint16_t hl, uint16_t ix)\n{",
                        "void sound_1094(uint16_t hl, uint16_t ix)\n{\n\tunsigned guard = 0;", 1)
    switch = body.find("switch (hl_2)")
    if switch < 0:
        raise SystemExit(f"{PATH}: sound_1094 has no switch")
    head, tail = body[:switch], body[switch:]
    head = head.replace("a = z80_read8(de);",
                        "L_1094_loop:\n\tif (++guard > 256) { de++; return; }\n\ta = z80_read8(de);", 1)
    continuation = "de++;\n\tgoto L_1094_loop;"
    tail, changed = rewrite_tail(tail, continuation)
    return s[:start] + head + tail + s[end:], changed


def main():
    text = PATH.read_text(encoding="utf-8")
    text, first = patch_0473(text)
    text, second = patch_1094(text)
    PATH.write_text(text, encoding="utf-8")
    if first or second:
        print(f"sound dispatch fix applied to {PATH}")
    else:
        print(f"sound dispatch fix already present in {PATH}")


if __name__ == "__main__":
    main()
