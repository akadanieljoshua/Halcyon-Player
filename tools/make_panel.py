#!/usr/bin/env python3
"""Writes Halcyon's page artwork, panel.svg.fragment: Halcyon Wander's
orrery, as near as the SDK's player page allows. python tools/make_panel.py

Halcyon Wander draws its own screen; a note player gets the SDK's player page,
whose only moving parts are 24 dots that show or hide (each may be a path of
many shapes) and one line of text. So the orrery is Halcyon Wander's own
geometry, greys and star field, and the dots carry what changes:

  dot 0..7    the lit places of scale 0..7, on all four rings (root and fifth are static)
  dot 8..10   covers: hide ring 1..3 (its line, places and lit places) beyond Spread
  dot 11..14  the marble at pitch classes b, b+4, b+8 on all four rings
  dot 15..17  masks: hide the marble at pitch classes 4a..4a+3, on every ring
  dot 18..21  masks: hide the marble on ring 0..3
  dot 22      the sun, grown while a key is held
  dot 23      the halo, swelled as a note sounds

A marble at pitch class s on ring r is spoke s % 4, with the other groups' and
other rings' masks shown. The masks sit under the sun, ring lines, places and
stars, so they never cut them; the lit places are drawn as rings without a fill
so that, over the marble, they all but vanish into it. Shapes that must hide
or show without relying on a fill rule are single polygons: a ring runs round
the outside, across a zero-width slit and back round the inside.
"""
import math, pathlib, random

ROOT = pathlib.Path(__file__).resolve().parents[1]
P = "halcyon_player"
FONT = 'font-family="UniversNextPro-Light"'
CX, CY = 240, 130
RINGS = [(68, 30), (106, 45), (144, 60), (182, 75)]       # rx, ry per octave, as Halcyon Wander
SCALES = [
    [0, 2, 3, 5, 7, 8, 10], [0, 2, 3, 5, 7, 9, 10], [0, 2, 3, 5, 7, 8, 11], [0, 1, 3, 5, 7, 8, 10],
    [0, 2, 4, 6, 7, 9, 11], [0, 2, 4, 5, 7, 9, 11], [0, 2, 4, 7, 9], [0, 3, 5, 7, 10],
]
MARBLE, MASK = 7.25, 8.6             # the marble's extent with its stroke; a mask's radius
out = [f'<g id="{P}_panel">']


def f(v):
    text = f"{v:.2f}".rstrip("0").rstrip(".")
    return "0" if text in ("-0", "") else text


def place(ring, s):
    """A place on a ring, relative to the centre."""
    a = -math.pi / 2 + s * 2 * math.pi / 12
    rx, ry = RINGS[ring]
    return rx * math.cos(a), ry * math.sin(a)


def polygon(points):
    return "M" + " L".join(f"{f(x)},{f(y)}" for x, y in points) + " Z"


def disc(x, y, r, sides):
    return polygon([(x + r * math.cos(2 * math.pi * k / sides), y + r * math.sin(2 * math.pi * k / sides))
                    for k in range(sides)])


def annulus(outer, inner):
    """One simple polygon: around the outside, across a zero-width slit, back around the inside.
    It fills as a ring under either fill rule, without relying on holes."""
    return polygon(outer + [outer[0], inner[0]] + inner[::-1])


def round_ring(x, y, r_out, r_in, sides):
    o = [(x + r_out * math.cos(2 * math.pi * k / sides), y + r_out * math.sin(2 * math.pi * k / sides)) for k in range(sides)]
    i = [(x + r_in * math.cos(2 * math.pi * k / sides), y + r_in * math.sin(2 * math.pi * k / sides)) for k in range(sides)]
    return annulus(o, i)


def dot(k, d, fill, stroke, width):
    out.append(f'<path id="{P}_dot_{k}" transform="matrix(1 0 0 1 {CX} {CY})" d="{d}" '
               f'fill="{fill}" stroke="{stroke}" stroke-width="{width}"/>')


positions = [place(r, s) for r in range(4) for s in range(12)]


def mask(r, s):
    """Covers the marble with a pixel to spare for antialiasing; at the top and bottom,
    where the next ring's place is 15 px away, it stays short of that ring's marble."""
    x, y = place(r, s)
    ry = 7.7 if s in (0, 6) else MASK
    return polygon([(x + MASK * math.cos(2 * math.pi * k / 24), y + ry * math.sin(2 * math.pi * k / 24))
                    for k in range(24)])


# --- The marble, under everything but the stars' gaps: spokes 11..14, then the masks
# that leave one of their places.
for b in range(4):
    d = " ".join(disc(*place(r, s), 7, 16) for r in range(4) for s in range(b, 12, 4))
    dot(11 + b, d, "#FFFFFF", "#FFFFFF", "0.5")
for a in range(3):
    d = " ".join(mask(r, s) for r in range(4) for s in range(4 * a, 4 * a + 4))
    dot(15 + a, d, "#000000", "#000000", "0.1")
for r in range(4):
    d = " ".join(mask(r, s) for s in range(12))
    dot(18 + r, d, "#000000", "#000000", "0.1")

# --- The sun, its halo and the faint circle around it; dot 22 the sun a quarter
# larger while a key plays, dot 23 the halo's swell as a note sounds.
out.append(f'<circle cx="{CX}" cy="{CY}" r="22" fill="none" stroke="#2E2E2E" stroke-width="0.8"/>')
out.append(f'<circle cx="{CX}" cy="{CY}" r="17" fill="none" stroke="#8C8C8C" stroke-width="1"/>')
out.append(f'<circle cx="{CX}" cy="{CY}" r="11" fill="#E6E6E6" stroke="#E6E6E6" stroke-width="0.6"/>')
dot(22, disc(0, 0, 13.75, 40), "#E6E6E6", "#E6E6E6", "0.6")
dot(23, round_ring(0, 0, 22.4, 21.4, 48), "#8C8C8C", "#8C8C8C", "0.1")

# --- The rings (static, over the masks so that they are never cut): each stops at the
# lit place's hole, where Halcyon Wander's places covered it.
for ring, (rx, ry) in enumerate(RINGS):
    runs, run = [], []
    for k in range(181):
        a = 2 * math.pi * k / 180
        x, y = rx * math.cos(a), ry * math.sin(a)
        if min(math.hypot(x - px, y - py) for px, py in (place(ring, s) for s in range(12))) < 2.7:
            if len(run) > 1:
                runs.append(run)
            run = []
        else:
            run.append((x, y))
    if len(run) > 1:
        runs.append(run)
    d = " ".join("M" + " L".join(f"{f(x)},{f(y)}" for x, y in r) for r in runs)
    out.append(f'<path transform="matrix(1 0 0 1 {CX} {CY})" d="{d}" fill="none" stroke="#5A5A5A" stroke-width="0.6"/>')

# Every place that a scale may leave unlit, as Halcyon Wander's dim places (the root
# and the fifth are in every scale).
for ring in range(4):
    for s in range(12):
        if s not in (0, 7):
            x, y = place(ring, s)
            out.append(f'<circle cx="{f(CX + x)}" cy="{f(CY + y)}" r="1.6" fill="none" stroke="#2E2E2E" stroke-width="0.3"/>')

# --- Dots 0..7: each scale's lit places on every ring, as Halcyon Wander's lit place
# (radius 3.2, a stroke of 1): a ring from 2.7 to 3.7.
# The root and the fifth are in every scale: those stay lit, as static artwork.
for ring in range(4):
    for s in (0, 7):
        x, y = place(ring, s)
        out.append(f'<path transform="matrix(1 0 0 1 {CX} {CY})" d="{round_ring(x, y, 3.7, 2.7, 10)}" '
                   f'fill="#E6E6E6" stroke="#E6E6E6" stroke-width="0.1"/>')
for k, notes in enumerate(SCALES):
    d = " ".join(round_ring(*place(r, s), 3.7, 2.7, 10) for r in range(4) for s in notes if s not in (0, 7))
    dot(k, d, "#E6E6E6", "#E6E6E6", "0.1")

# --- Dots 8..10: covers for rings 1..3 beyond Spread: the line's band and every place.
for ring in (1, 2, 3):
    rx, ry = RINGS[ring]
    o = [((rx + 1.3) * math.cos(2 * math.pi * k / 64), (ry + 1.3) * math.sin(2 * math.pi * k / 64)) for k in range(64)]
    i = [((rx - 1.3) * math.cos(2 * math.pi * k / 64), (ry - 1.3) * math.sin(2 * math.pi * k / 64)) for k in range(64)]
    d = annulus(o, i) + " " + " ".join(disc(*place(ring, s), 4.3, 10) for s in range(12))
    dot(7 + ring, d, "#000000", "#000000", "0.1")

# --- Stars: Halcyon Wander's own field (the same seed and order), drawn last so that no
# mask ever hides one. Those a mask or the grown sun would reach are left out: in
# Halcyon Wander they sat under or beside a place or the sun.
r = random.Random(3)
for _ in range(70):
    x, y = r.uniform(2, 478), r.uniform(42, 218)
    radius, fill = r.choice([0.5, 0.6, 0.8]), r.choice(["#2E2E2E", "#5A5A5A", "#8C8C8C"])
    near = min(math.hypot(x - CX - px, y - CY - py) for px, py in positions)
    if near < MASK + radius or math.hypot(x - CX, y - CY) < 14.5:
        continue
    out.append(f'<circle cx="{f(x)}" cy="{f(y)}" r="{radius}" fill="{fill}" stroke="none" stroke-width="0"/>')

# The player page's line of text, which Halcyon leaves empty.
# The player page's line of text: "hold" while Hold is on, otherwise empty.
out.append(f'<text id="{P}_text" transform="matrix(1 0 0 1 240 214)" fill="#8C8C8C" {FONT} font-size="12"></text>')
out.append('</g>')
(ROOT / "panel.svg.fragment").write_text("\n".join(out) + "\n")
print("panel.svg.fragment written")
