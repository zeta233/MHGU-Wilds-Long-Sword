"""Retain one strike in the installed mod's GU-derived roundslash animation."""
import argparse
from pathlib import Path
from struct import pack, pack_into

import numpy as np

from portkit import ANIM_ENTRY, Lmt, arc_replace, decode_q14, nlerp, read_arc, unpack


RECOVERY_FROM = 44
RECOVERY_TO = 110
JOIN_END = 62
CUT_FRAMES = RECOVERY_TO - JOIN_END
SHEATHING_SLOTS = (127, 219)
SOUND_ENTRY = r"sound\call\pl\pl_wp07"
EVENT_OFFSET = 52
EVENT_BLOCK_SIZE = 144
EVENT_TRACK_COUNT = 10
EVENT_TRACKS_OFFSET = 64


def round_tracks(lmt, shorten):
    # The port's slot 148 already has one key per frame. Its first swing
    # matches GU's stock slot 127 and can keep its original hit/cancel clock.
    frames, tracks = lmt.tracks(148)
    if frames != 159:
        raise ValueError(f'Expected a 159-frame roundslash, got {frames}')
    rotations = {}
    positions = {}
    for track in tracks:
        if track.usage == 0 and track.codec == 6:
            source = [decode_q14(track.data[i * 8:(i + 1) * 8])[0] for i in range(frames)]
            interpolate = nlerp
            output = rotations
        elif track.usage in (1, 4) and track.codec == 3:
            source = [np.array(unpack('3f', track.data, i * 16)) for i in range(frames)]
            if track.usage == 4:
                # Remove the forward travel from the discarded turn.
                shift = source[RECOVERY_TO] - source[55]
                source[RECOVERY_TO:] = [point - shift for point in source[RECOVERY_TO:]]
            interpolate = mix_position
            output = positions
        else:
            continue

        samples = source[:RECOVERY_FROM + 1]
        for frame in range(RECOVERY_FROM + 1, JOIN_END):
            fraction = (frame - RECOVERY_FROM) / (JOIN_END - RECOVERY_FROM)
            eased = fraction * fraction * (3 - 2 * fraction)
            samples.append(interpolate(source[RECOVERY_FROM], source[RECOVERY_TO], eased))
        if shorten:
            samples.extend(source[RECOVERY_TO:])
        else:
            for frame in range(JOIN_END, frames):
                raw = RECOVERY_TO + (frames - 1 - RECOVERY_TO) * (frame - JOIN_END) / (frames - 1 - JOIN_END)
                left = int(raw)
                right = min(left + 1, frames - 1)
                samples.append(interpolate(source[left], source[right], raw - left))
        output[track.bone] = samples

    return rotations, positions


def edited_round(data):
    original = Lmt(data)
    drawn = round_tracks(original, shorten=False)
    # Remove the gap left by the second turn. Play the GU sheathing tail at 1x.
    sheathing = round_tracks(original, shorten=True)
    result = data
    for slot in (127, 219, 148):
        tracks = drawn if slot == 148 else sheathing
        # Descriptors need distinct storage for the loader's relocation.
        frames = 159 if slot == 148 else 159 - CUT_FRAMES
        result = Lmt(result).replace(slot, *tracks, frames=frames, template=148)
    return add_round_attachments(result, original)


def add_round_attachments(data, original):
    event_source = unpack('I', original.descriptor(127), EVENT_OFFSET)[0]
    assert event_source
    first_count, first_source = unpack('2I', original.data, event_source + EVENT_TRACKS_OFFSET)
    runs = [unpack('2I', original.data, first_source + 8 * i) for i in range(first_count)]
    if runs != [(8, 58), (1, 70), (2, 1), (6, 29), (0, 1)]:
        raise ValueError('Unexpected GU roundslash attachment track')

    output = bytearray(data)
    for slot in (127, 148, 219):
        handoff = (RECOVERY_FROM + JOIN_END) // 2
        if slot == 148:
            handoff_runs = [(8, handoff), (1, 159 - handoff)]
        else:
            handoff_runs = [(8, handoff), (1, 128 - CUT_FRAMES - handoff), *runs[2:]]

        output += bytes(-len(output) % 16)
        event_table = len(output)
        tables_size = 2 * EVENT_BLOCK_SIZE
        output += original.data[event_source:event_source + tables_size]
        for bank in range(2):
            for channel in range(EVENT_TRACK_COUNT):
                bank_offset = bank * EVENT_BLOCK_SIZE + EVENT_TRACKS_OFFSET + channel * 8
                count, pointer = unpack('2I', original.data, event_source + bank_offset)
                if not count:
                    continue
                dest = len(output)
                if bank == 0 and channel == 0:
                    pack_into('<2I', output, event_table + bank_offset, len(handoff_runs), dest)
                    for value, duration in handoff_runs:
                        output += value.to_bytes(4, 'little') + duration.to_bytes(4, 'little')
                else:
                    pack_into('<I', output, event_table + bank_offset + 4, dest)
                    payload = original.data[pointer:pointer + count * 8]
                    if slot in SHEATHING_SLOTS:
                        if payload != pack('<2I', 0, 159):
                            raise ValueError('Unexpected auxiliary roundslash track')
                        payload = pack('<2I', 0, 159 - CUT_FRAMES)
                    output += payload
        descriptor = unpack('I', output, 8 + slot * 4)[0]
        pack_into('<I', output, descriptor + EVENT_OFFSET, event_table)
    return output


def shorten_sound_motion(data):
    motion, count, last = unpack('IHH', data)
    cues = [unpack('2I', data, 8 + i * 8) for i in range(count)]
    retained = []
    for i, (frame, offset) in enumerate(cues):
        end = cues[i + 1][1] if i + 1 < count else len(data)
        if RECOVERY_FROM < frame < RECOVERY_TO:
            continue
        payload = bytearray(data[offset:end])
        if frame != 0xffffffff:
            if frame != unpack('H', payload)[0]:
                raise ValueError(f'Unexpected sound cue in motion {motion}')
            if frame >= RECOVERY_TO:
                frame -= CUT_FRAMES
                pack_into('<H', payload, 0, frame)
        retained.append((frame, payload))

    header = bytearray(pack('<IHH', motion, len(retained), last - CUT_FRAMES))
    payloads = bytearray()
    first = len(header) + 8 * len(retained)
    for frame, payload in retained:
        header += pack('<2I', frame, first + len(payloads))
        payloads += payload
    return header + payloads


def retime_round_sound(data):
    count, capacity = unpack('2H', data, 8)
    table = 16 + 2 * capacity
    motions = [unpack('2I', data, table + 8 * i) for i in range(count)]
    header = bytearray(data[:table])
    payloads = bytearray()
    first = table + 8 * count
    for i, (motion, offset) in enumerate(motions):
        end = motions[i + 1][1] if i + 1 < count else len(data)
        payload = data[offset:end]
        if motion - 5000 in SHEATHING_SLOTS:
            payload = shorten_sound_motion(payload)
        header += pack('<2I', motion, first + len(payloads))
        payloads += payload
    return header + payloads


def mix_position(a, b, fraction):
    return a + (b - a) * fraction


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='Original v0.3.84 mod w07.arc')
    parser.add_argument('output', type=Path, help='Patched w07.arc')
    args = parser.parse_args()
    entries = {entry.name: entry for entry in read_arc(args.source)[1]}
    arc_replace(args.source, args.output, {
        ANIM_ENTRY: edited_round(entries[ANIM_ENTRY].data()),
        SOUND_ENTRY: retime_round_sound(entries[SOUND_ENTRY].data()),
    })


if __name__ == '__main__':
    main()
