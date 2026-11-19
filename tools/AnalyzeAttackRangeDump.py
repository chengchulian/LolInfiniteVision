"""Resolve attack-range event signatures in a mapped dump, without process access.

Reads the DLL's signature definitions. No version, timestamp, SHA or fixed-RVA
checks. File offsets must correspond to RVAs (mapped layout).
"""
import argparse
import json
from pathlib import Path
import re
import struct


def load_patterns():
    source = (Path(__file__).resolve().parents[1] / 'src/AttackRangeSignatures.h').read_text()
    return {
        name: ' '.join(re.findall(r'"([^"]+)"', body)).split()
        for name, body in re.findall(r'inline constexpr char (\w+)\[\]\s*=\s*(.*?);', source, re.S)
    }


def unique(data, tokens, begin=0, end=None):
    pattern = b''.join(b'.' if token == '??' else re.escape(bytes.fromhex(token)) for token in tokens)
    matches = [m.start() for m in re.finditer(b'(?=' + pattern + b')', data, re.S)
               if m.start() >= begin and m.start() + len(tokens) <= (len(data) if end is None else end)]
    if len(matches) != 1:
        raise ValueError(f'Expected one signature match, found {len(matches)}: {[hex(x) for x in matches]}')
    return matches[0]


def analyze(path):
    data = path.read_bytes()
    patterns = load_patterns()
    matches = {}
    for name in ('Dispatch', 'Handler', 'EnableBranch', 'PlayerSelection', 'PlayerTable'):
        try:
            scoped = name not in ('Dispatch', 'Handler')
            start = matches['Handler'] if scoped else 0
            end = min(start + 0x200, len(data)) if scoped else len(data)
            matches[name] = unique(data, patterns[name], start, end)
        except ValueError as error:
            raise ValueError(f'{name}: {error}') from error

    def rip(instruction):
        return instruction + 7 + struct.unpack_from('<i', data, instruction + 3)[0]

    selection = matches['PlayerSelection']
    references = {
        'dispatcher_slot': rip(matches['Dispatch'] + 6),
        'hud_slot': rip(matches['Handler'] + 20),
        'selector_slot': rip(selection),
        'invalid_player': rip(selection + 7),
        'player_table': rip(matches['PlayerTable'] + 11),
    }
    for name, address in references.items():
        size = 256 * 0x120 if name == 'player_table' else 1 if name == 'invalid_player' else 8
        if address < 0 or address + size > len(data):
            raise ValueError(f'{name} outside mapped dump: {address:#x}')
    index_offset = struct.unpack_from('<I', data, selection + 34)[0]
    if index_offset > 0x10000:
        raise ValueError(f'Unexpected selector index offset: {index_offset:#x}')
    return {
        'dump': str(path.resolve()),
        'scope': 'static mapped-file analysis; no version gate, game calls or process access',
        'signature_rvas': {name: hex(value) for name, value in matches.items()},
        'reference_rvas': {name: hex(value) for name, value in references.items()},
        'selector_index_offset': hex(index_offset),
        'event_string_rvas': [hex(m.start()) for m in re.finditer(b'evtToggleShowOverlayAttackRange\x00', data)],
        'enable_patch': '74 05 -> EB 05 (native handler always selects enabled)',
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dump', type=Path)
    parser.add_argument('--output', type=Path, help='Write JSON evidence')
    args = parser.parse_args()
    try:
        result = analyze(args.dump)
        output = json.dumps(result, ensure_ascii=False, indent=2) + '\n'
        if args.output:
            args.output.write_text(output, encoding='utf-8')
        else:
            print(output, end='')
    except (OSError, ValueError, KeyError, struct.error) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
