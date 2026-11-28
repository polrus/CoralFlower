#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The arpeggiator's written patterns (cf_arp.c, the MODEs after Strum) from MIDI files into a C header.

  tools/gen_arp_patterns.py OUT.h [DIR]

DIR: default assets/arp; every *.mid in it, in name order, is one pattern named after the file (its first 10
characters). All its notes are used, on any channel and track.

The pattern follows the chord being played. The file's own chord is found (the major or minor triad its notes sound
the longest on: C-E-G for a tune in C) and every note is stored as one of that chord's tones (root, 3rd, 5th), an
octave above the chord's root at or below the file's lowest note, and a number of scale steps up from that tone (a
passing note: D in C-E-G is the root and a step). On the unit the tones are the held chord's (root, 3rd, 5th, from its
root up), the steps the current KEY / SCALE's: holding the file's own chord at that octave plays the file as written.

Rhythm: the file's grid (the largest step every start and length is a multiple of) is one RATE step; a note longer
than a step holds over the next ones, a gap is a rest, and notes starting together sound together (up to 3).
"""
import struct
import sys
from math import gcd
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
MAJOR = (0, 2, 4, 5, 7, 9, 11)
MINOR = (0, 2, 3, 5, 7, 8, 10)
MAX_STEPS = 256
MAX_AT_ONCE = 3


def vlq(d, i):
    v = 0
    while True:
        b = d[i]
        i += 1
        v = (v << 7) | (b & 127)
        if b < 128:
            return v, i


def read_midi(path):
    """-> (ppq, [(start, length, note)], end tick)"""
    d = path.read_bytes()
    if d[:4] != b"MThd":
        raise SystemExit(f"{path}: not a MIDI file")
    _fmt, ntr, div = struct.unpack(">HHH", d[8:14])
    if div & 0x8000:
        raise SystemExit(f"{path}: SMPTE time is not supported, only ticks per quarter note")
    i, notes, end = 8 + struct.unpack(">I", d[4:8])[0], [], 0
    for _ in range(ntr):
        if d[i:i + 4] != b"MTrk":
            raise SystemExit(f"{path}: a broken track")
        j = i + 8
        stop = j + struct.unpack(">I", d[i + 4:i + 8])[0]
        tick, run, on = 0, None, {}
        while j < stop:
            dt, j = vlq(d, j)
            tick += dt
            s = d[j]
            if s == 0xFF:
                ln, k = vlq(d, j + 2)
                j = k + ln
                continue
            if s in (0xF0, 0xF7):
                ln, k = vlq(d, j + 1)
                j = k + ln
                continue
            if s & 0x80:
                run = s
                j += 1
            hi = run & 0xF0
            if hi in (0xC0, 0xD0):
                j += 1
                continue
            a, b = d[j], d[j + 1]
            j += 2
            key = (run & 15, a)
            if hi == 0x90 and b:
                on[key] = tick
            elif hi in (0x80, 0x90) and key in on:
                t0 = on.pop(key)
                if tick > t0:
                    notes.append((t0, tick - t0, a))
        end = max(end, tick)
        i = stop
    if not notes:
        raise SystemExit(f"{path}: no notes")
    return div, sorted(notes), end


def file_chord(notes):
    """the triad the notes sound longest on -> (root pc, intervals, scale)"""
    dur = [0] * 12
    for _t, ln, n in notes:
        dur[n % 12] += ln
    best = None
    for r in range(12):
        for third, scale in ((4, MAJOR), (3, MINOR)):
            score = dur[r] + dur[(r + third) % 12] + dur[(r + 7) % 12]
            key = (score, dur[r], third == 4)
            if best is None or key > best[0]:
                best = (key, r, (0, third, 7), scale)
    return best[1], best[2], best[3]


def encode(note, base, iv, scale):
    """a note -> one byte: tone (2 bits), scale steps (3 bits), octave (3 bits)"""
    rel = note - base                                   # >= 0: base is at or below the lowest note
    oct_, pc = divmod(rel, 12)
    tone = max(k for k in range(3) if iv[k] <= pc)      # the chord's tone at or below it
    deg = [s for s in scale]
    i0 = deg.index(iv[tone]) if iv[tone] in deg else 0
    steps = 0
    while steps < 7 and deg[(i0 + steps) % 7] + 12 * ((i0 + steps) // 7) < pc:
        steps += 1
    if steps > 7 or oct_ > 7:
        raise SystemExit(f"note {note}: too far from the chord (more than 7 octaves or scale steps)")
    return tone | (min(steps, 7) << 2) | (oct_ << 5)


def pattern(path):
    ppq, notes, end = read_midi(path)
    root, iv, scale = file_chord(notes)
    lo = min(n for _t, _l, n in notes)
    base = lo - (lo - root) % 12                         # the chord's root at or below the lowest note
    t0 = min(t for t, _l, _n in notes)
    total = max(end, max(t + ln for t, ln, _n in notes)) - t0
    unit = 0
    for t, ln, _n in notes:
        unit = gcd(gcd(unit, t - t0), ln)
    unit = gcd(unit, total) or ppq
    nsteps = total // unit
    if nsteps > MAX_STEPS:
        raise SystemExit(f"{path}: {nsteps} steps on its grid; at most {MAX_STEPS} (simplify the rhythm or shorten it)")
    steps = [[0, []] for _ in range(nsteps)]
    for t, ln, n in notes:
        s = steps[(t - t0) // unit]
        s[0] = max(s[0], ln // unit)
        if len(s[1]) < MAX_AT_ONCE:
            s[1].append(encode(n, base, iv, scale))
    name = path.stem[:10]
    names = "C C# D D# E F F# G G# A A# B".split()
    info = f"{names[root]}{'' if iv[1] == 4 else 'm'} at MIDI {base}, a step {unit}/{ppq} of a beat, {nsteps} steps"
    return name, steps, info


def main(out, src=None):
    d = Path(src) if src else SRC / "assets" / "arp"
    files = sorted(d.glob("*.mid")) if d.exists() else []
    pats = [pattern(f) for f in files]
    L = ["/* generated by tools/gen_arp_patterns.py: the arpeggiator's written patterns (cf_arp.c) */", "#pragma once",
         "/* a step: its length in steps (0: nothing starts), how many notes start (0..3), the notes: tone | steps << 2 |",
         " * octave << 5 (cf_arp.c cfa_pat_note) */",
         "typedef struct { uint8_t len, n, note[3]; } cf_arp_step_t;",
         "typedef struct { const char *name; uint16_t n; const cf_arp_step_t *step; } cf_arp_pat_t;"]
    for k, (name, steps, info) in enumerate(pats):
        L.append(f"/* {name}: {info} */")
        L.append(f"static const cf_arp_step_t CF_ARP_PAT{k}[{len(steps)}] = {{")
        for ln, ns in steps:
            cnt, ns = len(ns), (ns + [0, 0, 0])[:3]    # (a note byte may be 0: the root at the base octave)
            L.append(f"    {{{ln}, {cnt}, {{{ns[0]}, {ns[1]}, {ns[2]}}}}},")
        L.append("};")
    L.append(f"#define CF_ARP_NPATS {len(pats)}u")
    if pats:
        L.append("static const cf_arp_pat_t CF_ARP_PATS[CF_ARP_NPATS] = {")
        L += [f"    {{\"{name}\", {len(steps)}u, CF_ARP_PAT{k}}}," for k, (name, steps, _i) in enumerate(pats)]
        L.append("};")
    Path(out).write_text("\n".join(L) + "\n")
    for name, _s, info in pats:
        print(f"arp pattern: {name}: {info}")
    if not pats:
        print("arp patterns: none")


if __name__ == "__main__":
    main(*sys.argv[1:3])
