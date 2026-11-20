"""Refresh the Wilds sound bank while retaining the installed GU weapon banks."""
import argparse
from pathlib import Path

from build_gu_round_audio import append_gu_follow_audio, pack
from portkit import read_arc


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('wilds', type=Path)
    parser.add_argument('combined', type=Path)
    parser.add_argument('out', type=Path)
    args = parser.parse_args()
    header, wilds = read_arc(args.wilds)
    _, installed = read_arc(args.combined)
    stock = [entry for entry in installed if entry.name.startswith('sound\\se\\pl\\wp\\swr\\')]
    entries = wilds + stock
    requests = {entry.name: i for i, entry in enumerate(entries)
                if entry.kind == 0x1bcc4966 and entry in stock}
    assert len(requests) >= 2
    common = next(i for name, i in requests.items() if name.endswith('\\snd_wp_swo_com'))
    follow_index, follow_sound = append_gu_follow_audio(entries, common)
    mapping = [0] * 256
    for name, i in requests.items():
        suffix = name.rsplit('snd_wp_swo', 1)[-1]
        slot = 255 if suffix == '_com' else int(suffix)
        mapping[slot] = i + (i >= follow_index)
    args.out.mkdir(parents=True, exist_ok=True)
    pack(header, entries, args.out / 'world_audio.arc')
    (args.out / 'gu_round_banks.h').write_text(
        '#pragma once\n'
        f'#define GU_ROUND_RESOURCE_COUNT {len(entries)}u\n'
        f'#define GU_FOLLOW_SOUND {follow_sound}u\n'
        f'#define GU_FOLLOW_FINISH_SOUND {follow_sound + 2}u\n'
        'static const unsigned short gu_round_request_index[256]={\n'
        + ''.join(','.join(map(str, mapping[i:i+16])) + ',\n' for i in range(0, 256, 16))
        + '};\n'
    )
    print(len(entries), follow_sound, len(requests))
