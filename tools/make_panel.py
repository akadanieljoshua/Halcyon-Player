#!/usr/bin/env python3
"""Writes Wander Player's page artwork, panel.svg.fragment: Halcyon Wander's
orrery in greys, on the SDK's player page. python tools/make_panel.py

The SDK draws the two-page header above y=38 and lays out the output keyboard
along y=174; the orrery sits between them.
  wander_player_dot_0..11   the scale's places around the held key, root at the top
  wander_player_dot_12..23  the marble, on the place of the note that sounded last
  wander_player_text        the note, or the scale's name while nothing is held
  wander_player_key_*, _played_*  the output keyboard and its activity dots
"""
import math, pathlib, random

ROOT = pathlib.Path(__file__).resolve().parents[1]
P = "wander_player"
CX, CY = 240, 104
RX, RY = 150, 54                     # the orbit
BLACK = {1, 3, 6, 8, 10}             # MIDI pitch classes on black keys
out = [f'<g id="{P}_panel">']


def f(v):
    return f"{v:.3f}".rstrip("0").rstrip(".")


def place(semitone):
    a = -math.pi / 2 + semitone * 2 * math.pi / 12
    return CX + RX * math.cos(a), CY + RY * math.sin(a)


# Stars, static, kept clear of the header and the keyboard.
r = random.Random(3)
for _ in range(48):
    out.append(f'<circle cx="{f(r.uniform(4, 476))}" cy="{f(r.uniform(44, 166))}" r="{r.choice([0.5, 0.6, 0.8])}" '
               f'fill="{r.choice(["#2E2E2E", "#5A5A5A", "#8C8C8C"])}" stroke="none" stroke-width="0"/>')

# The orbit and the sun's faint halo (static).
pts = [(RX * math.cos(2 * math.pi * k / 72), RY * math.sin(2 * math.pi * k / 72)) for k in range(72)]
out.append(f'<path transform="matrix(1 0 0 1 {CX} {CY})" d="M' + " L".join(f"{f(x)},{f(y)}" for x, y in pts) +
           ' Z" fill="none" stroke="#5A5A5A" stroke-width="0.6"/>')
out.append(f'<ellipse cx="{CX}" cy="{CY - 4}" rx="62" ry="26" fill="none" stroke="#2E2E2E" stroke-width="0.8"/>')
# Every place, unlit: a faint ring the scale's dots light over.
for s in range(12):
    x, y = place(s)
    out.append(f'<circle cx="{f(x)}" cy="{f(y)}" r="3" fill="#000000" stroke="#3A3A3A" stroke-width="1"/>')
# The root's tick above its place.
x, y = place(0)
out.append(f'<path d="M{f(x)},{f(y - 9)} L{f(x)},{f(y - 14)}" fill="none" stroke="#8C8C8C" stroke-width="1"/>')

# Dots 0..11: the scale's notes.
for s in range(12):
    x, y = place(s)
    out.append(f'<circle id="{P}_dot_{s}" transform="matrix(1 0 0 1 {f(x)} {f(y)})" cx="0" cy="0" r="3.6" '
               f'fill="#B4B4B4" stroke="#E6E6E6" stroke-width="1"/>')
# Dots 12..23: the marble.
for s in range(12):
    x, y = place(s)
    out.append(f'<circle id="{P}_dot_{12 + s}" transform="matrix(1 0 0 1 {f(x)} {f(y)})" cx="0" cy="0" r="7" '
               f'fill="#FFFFFF" stroke="#FFFFFF" stroke-width="0.5"/>')

# The line: the SDK centres it on x=240.
out.append(f'<text id="{P}_text" transform="matrix(1 0 0 1 240 {CY + 7})" fill="#F2F2F2" '
           f'font-family="UniversNextPro-Light" font-size="20">wander</text>')

# The output keyboard: white keys, then black keys, then activity dots. The SDK
# poses each one every frame; they are authored over F3..E5 for a still picture.
WIDTH = 444 / 14
def white_index(n):
    return sum(1 for m in range(n) if m % 12 not in BLACK)
def key_x(n):
    x = 18 + (white_index(n) - white_index(53)) * WIDTH
    return x + WIDTH * 0.7 if n % 12 in BLACK else x
def off(n):
    return not 53 <= n <= 76
for n in range(128):
    if n % 12 in BLACK:
        continue
    x = 1024 if off(n) else key_x(n)
    out.append(f'<path id="{P}_key_{n}" transform="matrix(1 0 0 1 {f(x)} 174)" d="M0,0 L10,0 L10,44 L0,44 Z" '
               f'fill="#B4B4B4" stroke="#000000" stroke-width="1"/>')
for n in range(128):
    if n % 12 not in BLACK:
        continue
    x = 1024 if off(n) else key_x(n)
    out.append(f'<path id="{P}_key_{n}" transform="matrix(1 0 0 1 {f(x)} 174)" d="M0,0 L6,0 L6,27 L0,27 Z" '
               f'fill="#1E1E1E" stroke="#000000" stroke-width="1"/>')
for n in range(128):
    black = n % 12 in BLACK
    fill = "#F0F0F0" if black else "#000000"
    out.append(f'<circle id="{P}_played_{n}" transform="matrix(1 0 0 1 1024 {195 if black else 211})" cx="0" cy="0" '
               f'r="3" fill="{fill}" stroke="{fill}" stroke-width="0.5"/>')
out.append('</g>')
(ROOT / "panel.svg.fragment").write_text("\n".join(out) + "\n")
print("panel.svg.fragment written")
