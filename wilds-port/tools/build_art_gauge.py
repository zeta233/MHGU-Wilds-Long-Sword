"""Fill missing Hunter Art gains on the active Wilds damage rows of w07.arc."""
import argparse
import re
from pathlib import Path
from struct import pack_into

from portkit import arc_replace, read_arc, unpack


HIT_ENTRY = r'player\hit\pl_we07_hitdata'
HIT_TABLE_HEADER = 16
HIT_RECORD_SIZE = 56
MOTION_VALUE = 4
HUNTER_ART_GAIN = 48


def wilds_damage_rows(header):
    strike_table = header.split('WildsStrike wilds_strikes[]={', 1)[1].split('};', 1)[0]
    rows = set()
    for row, tiered in re.findall(r'\{\d+u,\d+u,\d+u,(\d+)u,([01])u\}', strike_table):
        first = int(row)
        rows.update(range(first, first + (4 if tiered == '1' else 1)))
    assert rows, 'No active Wilds strikes'
    release = int(re.search(r'#define WILDS_RELEASE_ROW (\d+)u', header)[1])
    rows.update(range(release, release + 4))
    rows.update(int(row) for row in re.findall(r'#define WILDS_ROUND_\w+_ROW (\d+)u', header))
    return rows


def repair_hunter_art_gain(hdp, rows):
    count = unpack('I', hdp, 8)[0]
    if hdp[:4] != b'HDP\0' or len(hdp) != HIT_TABLE_HEADER + count * HIT_RECORD_SIZE:
        raise ValueError('Invalid weapon hit table')
    result = bytearray(hdp)
    for row in sorted(rows):
        if not 0 <= row < count:
            raise ValueError(f'Attack row {row} is outside the hit table')
        base = HIT_TABLE_HEADER + row * HIT_RECORD_SIZE
        mv = unpack('h', hdp, base + MOTION_VALUE)[0]
        art = unpack('h', hdp, base + HUNTER_ART_GAIN)[0]
        # Most stock damaging Long Sword hits award their motion value.
        # Keep native-derived rates and zero-damage contact samplers intact.
        if mv > 0 and art == 0:
            pack_into('<h', result, base + HUNTER_ART_GAIN, mv)
    return result


def patch(source, output, header):
    hits = next(entry.data() for entry in read_arc(source)[1] if entry.name == HIT_ENTRY)
    expected = int(re.search(r'#define WILDS_HIT_COUNT (\d+)u', header)[1])
    if unpack('I', hits, 8)[0] != expected:
        raise ValueError('The weapon ARC and the combat table are from different versions')
    fixed = repair_hunter_art_gain(hits, wilds_damage_rows(header))
    arc_replace(source, output, {HIT_ENTRY: fixed})


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--header', type=Path, default=Path(__file__).resolve().parents[2] / 'integration/c/wilds_combat_data.h')
    args = parser.parse_args()
    patch(args.source, args.output, args.header.read_text(encoding='utf-8'))
