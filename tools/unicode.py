#!/usr/bin/env python3
"""Writes src/unicode_data.inc, the tables of src/unicode.c, from the Unicode
Character Database.

Usage: tools/unicode.py UCD_DIR > src/unicode_data.inc

UCD_DIR holds UnicodeData.txt, SpecialCasing.txt, CaseFolding.txt, PropList.txt,
and DerivedCoreProperties.txt, from https://www.unicode.org/Public/<version>/ucd/.
"""

import re
import sys
from pathlib import Path

CODE_POINTS = 0x110000

# Property bits, as src/unicode.h numbers them: (C name, file, property).
PROPERTIES: list[tuple[str, str, str]] = [
    ('MINIM_UNICODE_ALPHABETIC', 'DerivedCoreProperties.txt', 'Alphabetic'),
    ('MINIM_UNICODE_UPPER', 'DerivedCoreProperties.txt', 'Uppercase'),
    ('MINIM_UNICODE_LOWER', 'DerivedCoreProperties.txt', 'Lowercase'),
    ('MINIM_UNICODE_SPACE', 'PropList.txt', 'White_Space'),
    ('MINIM_UNICODE_DIGIT', 'UnicodeData.txt', 'Nd'),
    ('MINIM_UNICODE_CASED', 'DerivedCoreProperties.txt', 'Cased'),
    ('MINIM_UNICODE_CASE_IGNORABLE', 'DerivedCoreProperties.txt', 'Case_Ignorable'),
]

# Most code points in a full case mapping, as src/unicode.h has it.
MAX_MAPPING = 3

WIDTH = 100


def records(path: Path) -> list[list[str]]:
    """The `;`-separated fields of each line of `path`, without comments."""
    result: list[list[str]] = []
    for line in path.read_text(encoding='utf-8').splitlines():
        line = line.split('#', 1)[0].strip()
        if line:
            result.append([field.strip() for field in line.split(';')])
    return result


def code_points(field: str) -> range:
    """The code points of a field: `0041` or `0041..005A`."""
    first, _, last = field.partition('..')
    return range(int(first, 16), int(last or first, 16) + 1)


def version(ucd: Path) -> str:
    """The UCD's version, from DerivedCoreProperties.txt's first line."""
    first = (ucd / 'DerivedCoreProperties.txt').read_text(encoding='utf-8').splitlines()[0]
    match = re.search(r'-(\d+\.\d+\.\d+)\.txt', first)
    if match is None:
        sys.exit('no version in DerivedCoreProperties.txt')
    return match.group(1)


def property_bits(ucd: Path, unicode_data: list[list[str]]) -> list[int]:
    """Each code point's property bits."""
    bits = [0] * CODE_POINTS
    for i, (_, file, name) in enumerate(PROPERTIES):
        if file == 'UnicodeData.txt':
            for fields in unicode_data:
                if fields[2] == name:
                    bits[int(fields[0], 16)] |= 1 << i
        else:
            for fields in records(ucd / file):
                if fields[1] == name:
                    for c in code_points(fields[0]):
                        bits[c] |= 1 << i
    return bits


def ranges(bits: list[int]) -> list[tuple[int, int, int]]:
    """Maximal runs of code points with the same nonzero bits: (first, last, bits)."""
    result: list[tuple[int, int, int]] = []
    for c, b in enumerate(bits):
        if b and result and result[-1][1] == c - 1 and result[-1][2] == b:
            result[-1] = (result[-1][0], c, b)
        elif b:
            result.append((c, c, b))
    return result


def check_digits(unicode_data: list[list[str]], props: list[tuple[int, int, int]]) -> None:
    """Checks that each digit's value is its offset from its range's start, mod 10."""
    digit = 1 << [p[0] for p in PROPERTIES].index('MINIM_UNICODE_DIGIT')
    value = {int(f[0], 16): int(f[6]) for f in unicode_data if f[2] == 'Nd'}
    for first, last, b in props:
        if b & digit:
            for c in range(first, last + 1):
                if value[c] != (c - first) % 10:
                    sys.exit(f'digit U+{c:04X} is {value[c]}, not its offset mod 10')


def cases(ucd: Path, unicode_data: list[list[str]]) -> list[tuple[int, int, int, int]]:
    """(c, upper, lower, fold) for each code point with a simple mapping."""
    upper: dict[int, int] = {}
    lower: dict[int, int] = {}
    for fields in unicode_data:
        c = int(fields[0], 16)
        if fields[12]:
            upper[c] = int(fields[12], 16)
        if fields[13]:
            lower[c] = int(fields[13], 16)
    fold = {int(f[0], 16): int(f[2], 16) for f in records(ucd / 'CaseFolding.txt')
            if f[1] in ('C', 'S')}
    return [(c, upper.get(c, c), lower.get(c, c), fold.get(c, c))
            for c in sorted(upper.keys() | lower.keys() | fold.keys())]


def full_cases(ucd: Path, simple: list[tuple[int, int, int, int]]
               ) -> list[tuple[int, list[int], list[int], list[int]]]:
    """(c, upper, lower, fold) full mappings for each code point with one that isn't
    simple: SpecialCasing.txt's unconditional mappings and CaseFolding.txt's `F`."""
    upper: dict[int, list[int]] = {}
    lower: dict[int, list[int]] = {}
    fold: dict[int, list[int]] = {}
    for f in records(ucd / 'SpecialCasing.txt'):
        if len(f) < 5 or not f[4]:
            c = int(f[0], 16)
            lower[c] = [int(x, 16) for x in f[1].split()]
            upper[c] = [int(x, 16) for x in f[3].split()]
    for f in records(ucd / 'CaseFolding.txt'):
        if f[1] == 'F':
            fold[int(f[0], 16)] = [int(x, 16) for x in f[2].split()]
    by_code = {c: (u, l, f) for c, u, l, f in simple}
    result: list[tuple[int, list[int], list[int], list[int]]] = []
    for c in sorted(upper.keys() | lower.keys() | fold.keys()):
        u, l, f = by_code.get(c, (c, c, c))
        row = (c, upper.get(c, [u]), lower.get(c, [l]), fold.get(c, [f]))
        if any(len(m) > MAX_MAPPING for m in row[1:]):
            sys.exit(f'U+{c:04X} maps to more than {MAX_MAPPING} code points')
        result.append(row)
    return result


def mapping(m: list[int]) -> str:
    """The C initializer of the full mapping `m`."""
    return '{' + ', '.join(f'0x{x:X}' for x in m) + '}'


def rows(entries: list[str]) -> list[str]:
    """`entries`, comma-separated, in lines of at most WIDTH columns."""
    lines: list[str] = []
    line = '   '
    for entry in entries:
        if len(line) + len(entry) + 2 > WIDTH:
            lines.append(line)
            line = '   '
        line += f' {entry},'
    lines.append(line)
    return lines


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    ucd = Path(sys.argv[1])
    unicode_data = records(ucd / 'UnicodeData.txt')
    props = ranges(property_bits(ucd, unicode_data))
    check_digits(unicode_data, props)
    simple = cases(ucd, unicode_data)
    out = [
        f'// Generated by tools/unicode.py from Unicode {version(ucd)}; do not edit.',
        '',
        *[f'static_assert({name} == {1 << i}, "property bit");'
          for i, (name, _, _) in enumerate(PROPERTIES)],
        '',
        '// Runs of code points with the same properties: first, last, properties.',
        'static const prop_range props[] = {',
        *rows([f'{{0x{f:X}, 0x{l:X}, {b}}}' for f, l, b in props]),
        '};',
        '',
        '// Simple case mappings: code point, upper, lower, fold.',
        'static const case_row cases[] = {',
        *rows([f'{{0x{c:X}, 0x{u:X}, 0x{l:X}, 0x{f:X}}}' for c, u, l, f in simple]),
        '};',
        '',
        '// Full case mappings that aren\'t simple: code point, upper, lower, fold, each',
        '// 0-padded.',
        'static const full_row fulls[] = {',
        *rows([f'{{0x{c:X}, {mapping(u)}, {mapping(l)}, {mapping(f)}}}'
               for c, u, l, f in full_cases(ucd, simple)]),
        '};',
    ]
    sys.stdout.write('\n'.join(out) + '\n')


if __name__ == '__main__':
    main()
