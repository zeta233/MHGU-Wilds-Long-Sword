"""Keep the charge wind-up above the walking legs and root from MOT275."""
import argparse
from pathlib import Path
from struct import pack_into

import numpy as np

from portkit import ANIM_ENTRY, Lmt, arc_replace, decode_q14, qinv, qmul, read_arc, unpack


STAND_SLOT = 210
WALK_SLOT = 217
WALK_ENTRY = 30
LEGS = (14, 15, 16, 17, 18, 19)


def baked(lmt, slot):
    frames, tracks = lmt.tracks(slot)
    rotations = {}
    positions = {}
    for track in tracks:
        if track.usage in (0, 3) and track.codec == 6 and len(track.data) == frames * 8:
            rotations[track.bone] = [decode_q14(track.data[i:i + 8])[0] for i in range(0, len(track.data), 8)]
        elif track.usage in (1, 4) and track.codec == 3 and len(track.data) == frames * 16:
            positions[track.bone] = [np.array(unpack('3f', track.data, i)) for i in range(0, len(track.data), 16)]
    return frames, rotations, positions


def edit(original):
    lmt = Lmt(original)
    frames, stand, positions = baked(lmt, STAND_SLOT)
    stride, gait, gait_pos = baked(lmt, WALK_SLOT)
    if frames != 229 or stride != 107:
        raise ValueError(f'Expected original charge clips, got {frames} and {stride} frames')
    steps = stride - 1
    root = gait_pos[255]
    drift = root[-1] - root[0]
    rotations = {bone: stand[bone][:] for bone in stand}
    for frame in range(frames):
        lap, phase = divmod(max(0, frame - WALK_ENTRY), steps)
        for bone in (0, *LEGS):
            rotations[bone][frame] = gait[bone][phase]
        # Preserve the charging spine's orientation as the walking hip turns.
        rotations[1][frame] = qmul(qinv(gait[0][phase]), qmul(stand[0][frame], stand[1][frame]))
        positions[0][frame] = gait_pos[0][phase]
        positions[255][frame] = root[phase] - root[0] + lap * drift
    result = bytearray(lmt.replace(WALK_SLOT, rotations, positions, frames=frames, template=STAND_SLOT))
    desc = unpack('I', result, 8 + WALK_SLOT * 4)[0]
    pack_into('<3f', result, desc + 16, *positions[255][-1])
    return bytes(result)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('base', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    _, entries = read_arc(args.base)
    original = next(entry.data() for entry in entries if entry.name == ANIM_ENTRY)
    arc_replace(args.base, args.output, {ANIM_ENTRY: edit(original)})
