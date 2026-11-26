"""Redistribute the Android charge grip and upper-body poses over the three charge stages."""
import argparse
import re
from pathlib import Path

import numpy as np

from portkit import ANIM_ENTRY, Lmt, arc_replace, decode_q14, nlerp, qinv, qmul, read_arc, unpack


STAND, WALK = 210, 217
FRAMES = 229
# Keep the authored opening and full-charge pose. The middle anchors were
# already present in the wind-up but used to pass before the first stage.
ANCHORS = ((44, 44), (50, 51), (100, 54), (146, 56), (180, 180))


def timing(frame):
    for (start, source_start), (end, source_end) in zip(ANCHORS, ANCHORS[1:]):
        if start <= frame < end:
            t = (frame - start) / (end - start)
            return source_start, source_end, t * t * (3 - 2 * t)
    return frame, frame, 0.0


def sample(values, frame, rotate=False):
    a, b, t = timing(frame)
    if a == b:
        return values[a]
    return nlerp(values[a], values[b], t) if rotate else values[a] * (1 - t) + values[b] * t


def tracks(lmt, slot):
    frames, entries = lmt.tracks(slot)
    assert frames == FRAMES
    rotations, positions = {}, {}
    for track in entries:
        if track.usage in (0, 3) and track.codec == 6 and len(track.data) == frames * 8:
            rotations[track.bone] = [decode_q14(track.data[i:i + 8])[0] for i in range(0, len(track.data), 8)]
        if track.usage == 1 and track.codec == 3 and len(track.data) == frames * 16:
            positions[track.bone] = [np.array(unpack('3f', track.data, i)) for i in range(0, len(track.data), 16)]
    return rotations, positions


def animation(archive):
    lmt = Lmt(archive)
    stand, stand_pos = tracks(lmt, STAND)
    walk, walk_pos = tracks(lmt, WALK)
    for slot, source, positions in ((STAND, stand, stand_pos), (WALK, walk, walk_pos)):
        upper = {bone: [sample(values, i, rotate=True) for i in range(FRAMES)]
                 for bone, values in source.items() if 1 <= bone <= 13}
        position = {bone: [sample(values, i) for i in range(FRAMES)]
                    for bone, values in positions.items() if 1 <= bone <= 13}
        if slot == WALK:
            # Keep the original gait; compensate the spine for its moving hip.
            for frame in range(ANCHORS[0][0], ANCHORS[-1][0]):
                a, b, t = timing(frame)
                spine = nlerp(qmul(stand[0][a], stand[1][a]),
                              qmul(stand[0][b], stand[1][b]), t)
                upper[1][frame] = qmul(qinv(walk[0][frame]), spine)
        lmt = Lmt(lmt.replace(slot, upper, position))
    return lmt.data


def grip_header(source, target):
    text = source.read_text(encoding='utf-8')
    prefix, rows = text.split('static const float charge_pose_grips[][7]={', 1)
    values = [np.array([float(v) for v in row.replace('f', '').split(',')])
              for row in re.findall(r'\{([^{}]+)\}', rows)]
    assert len(values) >= 2 * FRAMES
    for start in (0, FRAMES):
        grip = values[start:start + FRAMES]
        position = [x[:3] for x in grip]
        rotation = [x[3:] for x in grip]
        for frame in range(ANCHORS[0][0], ANCHORS[-1][0]):
            values[start + frame] = np.r_[sample(position, frame), sample(rotation, frame, rotate=True)]
    with target.open('w', encoding='utf-8', newline='\n') as out:
        out.write(prefix.replace('build-charge-poses237.py', Path(__file__).name))
        out.write('static const float charge_pose_grips[][7]={\n')
        for row in values:
            out.write(' {' + ','.join(f'{n:.8f}f' for n in row) + '},\n')
        out.write('};\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('base', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('grips', type=Path)
    parser.add_argument('output_grips', type=Path)
    args = parser.parse_args()
    lmt = next(entry.data() for entry in read_arc(args.base)[1] if entry.name == ANIM_ENTRY)
    arc_replace(args.base, args.output, {ANIM_ENTRY: animation(lmt)})
    grip_header(args.grips, args.output_grips)
