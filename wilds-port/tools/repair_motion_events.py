"""Supply empty native event tracks for w07 motions with no event table."""

import argparse
import struct
from pathlib import Path

from portkit import ANIM_ENTRY, Lmt, arc_replace, read_arc


def repair_motion_events(data):
    motion = Lmt(data)
    repaired = bytearray(data)
    slots = set()
    for slot in range(motion.count):
        offset, = struct.unpack_from("<I", data, 8 + 4 * slot)
        if not offset or struct.unpack_from("<I", repaired, offset + 0x34)[0]:
            continue

        frames, = struct.unpack_from("<I", data, offset + 8)
        if not frames or data[offset + 0x33] & 2:
            raise ValueError(f"Cannot add a relative event table to motion {slot}")

        repaired.extend(bytes((-len(repaired)) % 4))
        table = len(repaired)
        repaired.extend(bytes(4 * 0x48))
        # LMT 67 stores four channels. Each channel can carry a single
        # zero-mask event spanning the clip, as native silent channels do.
        for channel in range(4):
            events = len(repaired)
            repaired.extend(struct.pack("<II", 0, frames))
            struct.pack_into("<II", repaired, table + channel * 0x48 + 0x40, 1, events)
        struct.pack_into("<I", repaired, offset + 0x34, table)
        slots.add(slot)
    return repaired, slots


def repair_arc(source, destination):
    _, entries = read_arc(source)
    motion = next(entry for entry in entries if entry.name == ANIM_ENTRY)
    data, slots = repair_motion_events(motion.data())
    arc_replace(source, destination, {ANIM_ENTRY: data})
    return slots


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    slots = repair_arc(args.source, args.destination)
    print(f"Added native event tables for {len(slots)} motions.")
