"""Give the red Foresight roundslash a drawn recovery after its swing."""
import argparse
from pathlib import Path
from struct import pack_into

from build_single_round import EVENT_OFFSET, EVENT_TRACKS_OFFSET
from portkit import ANIM_ENTRY, Lmt, arc_replace, decode_q14, encode_q14, nlerp, read_arc, unpack


FORESIGHT_SLOT = 148
DRAWN_IDLE_SLOT = 1
RECOVERY_START = 44
RECOVERY_END = 76


def drawn_recovery(data):
    lmt = Lmt(data)
    frames, tracks = lmt.tracks(FORESIGHT_SLOT)
    _, idle = lmt.tracks(DRAWN_IDLE_SLOT)
    ready = {(track.bone, track.usage): track.reference for track in idle}
    assert frames == 159
    output = bytearray(data)
    for track in tracks:
        if track.usage == 3:
            continue
        if track.usage == 0 and track.codec == 6:
            stride = 8
            first = RECOVERY_START * stride
            start = decode_q14(track.data[first:first + stride])[0]
        elif track.usage in (1, 4) and track.codec == 3:
            stride = 16
            start = unpack('3f', track.data, RECOVERY_START * stride)
        else:
            raise ValueError(f'Unexpected roundslash track {track.bone}, {track.usage}, {track.codec}')
        assert len(track.data) == frames * stride
        address = unpack('I', track.header, 12)[0]
        # Root positions are cumulative. Keep the travel from the swing.
        target = start if track.usage == 4 else ready[(track.bone, track.usage)]
        for frame in range(RECOVERY_START + 1, frames):
            t = min(1.0, (frame - RECOVERY_START) / (RECOVERY_END - RECOVERY_START))
            t = t * t * (3 - 2 * t)
            duration = int(frame + 1 < frames)
            offset = address + frame * stride
            if track.usage == 0:
                output[offset:offset + stride] = encode_q14(nlerp(start, target, t), duration)
            else:
                position = [(1 - t) * a + t * b for a, b in zip(start, target)]
                pack_into('<3fI', output, offset, *position, duration)
    # The sweep begins in GU's drawn grip. Keep that grip through the
    # recovery and the return to drawn idle instead of switching hands.
    event_table = unpack('I', lmt.descriptor(FORESIGHT_SLOT), EVENT_OFFSET)[0]
    attachment = event_table + EVENT_TRACKS_OFFSET
    count, runs = unpack('2I', data, attachment)
    assert count == 2 and unpack('4I', data, runs) == (8, 53, 1, 106)
    pack_into('<I', output, attachment, 1)
    pack_into('<2I', output, runs, 8, frames)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='ARC with the single-swing roundslash')
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    motion = next(entry.data() for entry in read_arc(args.source)[1] if entry.name == ANIM_ENTRY)
    arc_replace(args.source, args.output, {ANIM_ENTRY: drawn_recovery(motion)})


if __name__ == '__main__':
    main()
