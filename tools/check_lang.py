#!/usr/bin/env python3
"""Keep lang/*.json honest.

The built-in English table in gui/i18n.c is the single source of truth.

    python3 tools/check_lang.py            # verify every lang/*.json  (CI uses this)
    python3 tools/check_lang.py --write-en # regenerate lang/en.json from gui/i18n.c

Checks performed on each lang/<code>.json:
  * it is valid JSON and a flat object of string -> string
  * every key exists in the built-in English table (no stale keys)
  * every value uses exactly the same printf placeholders, in the same order,
    as the English text

The last check is the important one. The GUI is C: a translation that turns
"%s" into "%d", or drops a placeholder, is undefined behaviour at runtime. The
program ignores such entries and falls back to English, but a translator should
find out here rather than from a crash report.

Missing keys are reported as a warning, not an error: a partial translation is
useful, and the GUI falls back to English for whatever is absent.
"""

import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
I18N_C = ROOT / "gui" / "i18n.c"
LANG_DIR = ROOT / "lang"

# One { "key", "value" } row of the EN[] table. Values may be split across
# adjacent C string literals, so capture the whole value expression first.
ROW = re.compile(r'\{\s*("(?:[^"\\]|\\.)*")\s*,\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\}', re.S)
LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')

# A printf conversion specification, e.g. %s %d %3d %-10s %.2f
SPEC = re.compile(r"%(?:%|[-+ #0]*[0-9*]*(?:\.[0-9*]+)?(?:hh|h|ll|l|z|j|t|L)?[diouxXeEfgGaAcspn])")

C_ESCAPES = {"n": "\n", "t": "\t", "r": "\r", "b": "\b", "f": "\f", '"': '"', "\\": "\\", "/": "/"}


def unescape_c(s: str) -> str:
    """Decode a C string literal body, including \\xNN byte escapes (UTF-8)."""
    out = bytearray()
    i = 0
    while i < len(s):
        c = s[i]
        if c != "\\":
            out += c.encode("utf-8")
            i += 1
            continue
        i += 1
        nxt = s[i]
        if nxt == "x":
            j = i + 1
            while j < len(s) and s[j] in "0123456789abcdefABCDEF" and j - i <= 2:
                j += 1
            out.append(int(s[i + 1:j], 16))
            i = j
        else:
            out += C_ESCAPES.get(nxt, nxt).encode("utf-8")
            i += 1
    return out.decode("utf-8", "replace")


def builtin_english() -> dict:
    """Read the EN[] table out of gui/i18n.c."""
    src = I18N_C.read_text(encoding="utf-8")
    start = src.index("static const Pair EN[] = {")
    end = src.index("\n};", start)
    table = {}
    for key_lit, val_expr in ROW.findall(src[start:end]):
        key = unescape_c(key_lit[1:-1])
        value = "".join(unescape_c(part) for part in LITERAL.findall(val_expr))
        table[key] = value
    return table


def placeholders(text: str) -> list:
    return [s for s in SPEC.findall(text) if s != "%%"]


def main() -> int:
    english = builtin_english()
    if not english:
        print("error: could not read the EN[] table from gui/i18n.c", file=sys.stderr)
        return 1

    if "--write-en" in sys.argv:
        LANG_DIR.mkdir(exist_ok=True)
        path = LANG_DIR / "en.json"
        path.write_text(json.dumps(english, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"wrote {path.relative_to(ROOT)}  ({len(english)} keys)")
        return 0

    errors = warnings = 0
    files = sorted(LANG_DIR.glob("*.json")) if LANG_DIR.is_dir() else []
    if not files:
        print(f"error: no language files in {LANG_DIR}", file=sys.stderr)
        return 1

    for path in files:
        code = path.stem
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            print(f"{path.name}: invalid JSON: {exc}")
            errors += 1
            continue
        if not isinstance(data, dict) or not all(isinstance(v, str) for v in data.values()):
            print(f"{path.name}: must be a flat object of string -> string")
            errors += 1
            continue

        file_errors = []
        for key, value in data.items():
            if key not in english:
                file_errors.append(f"  unknown key {key!r} (not in gui/i18n.c)")
                continue
            want, got = placeholders(english[key]), placeholders(value)
            if want != got:
                file_errors.append(
                    f"  {key!r}: placeholders {got or '(none)'} but English has {want or '(none)'}"
                )

        missing = sorted(set(english) - set(data))
        if code == "en" and english != data:
            file_errors.append("  out of sync with gui/i18n.c - run: python3 tools/check_lang.py --write-en")

        if file_errors:
            print(f"{path.name}: FAIL")
            print("\n".join(file_errors))
            errors += len(file_errors)
        elif missing:
            print(f"{path.name}: ok, {len(data)} keys ({len(missing)} missing, English is used for those)")
            warnings += 1
        else:
            print(f"{path.name}: ok, {len(data)} keys, complete")

    print()
    if errors:
        print(f"{errors} problem(s) found.")
        return 1
    print("All language files are valid." + (f" ({warnings} incomplete)" if warnings else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
