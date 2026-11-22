#!/usr/bin/env python3
"""Draws Wander Player's page for review from tools/shot.cpp's frames: the
artwork with the dots, line and output keyboard posed as the SDK's player page
poses them (screen_player.hpp), under an approximation of the SDK's two-page
header. Not the device's renderer.
python tools/screens.py <shot binary> <out folder>   (writes page-<n>.svg)"""
import html, json, pathlib, re, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
P = 'wander_player'
ART = (ROOT / 'panel.svg.fragment').read_text()
TITLES = ['rate', 'chance', 'scale', 'gate', 'deja vu', 'length', 'spread', 'jitter']
BLACK = {1, 3, 6, 8, 10}


def white(n):
    return 7 * (n // 12) + [0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6][n % 12]


def keyboard_range(played, lowest=53):
    low = min([lowest] + played)
    high = max([lowest + 23] + played)
    low = 0 if low < 5 else 5 + (low - 5) // 12 * 12
    high = 4 if high < 5 else min(127, 16 + (high - 5) // 12 * 12)
    return low, high, white(high) - white(low) + 1


def pose(art, frame):
    def move(m, x, y, sx=1):
        return re.sub(r'transform="[^"]*"', f'transform="matrix({sx:.3f} 0 0 1 {x:.3f} {y:.3f})"', m)
    def dot(m):
        i = int(m.group(1))
        return m.group(0) if frame['lit'] >> i & 1 else ''
    art = re.sub(rf'<circle id="{P}_dot_(\d+)"[^>]*/>', dot, art)
    art = re.sub(rf'(<text id="{P}_text"[^>]*>)[^<]*(</text>)',
                 lambda m: m.group(1).replace('<text', '<text text-anchor="middle"') + html.escape(frame['text']) + m.group(2), art)
    low, high, whites = keyboard_range(frame['played'])
    width = 444 / whites
    def key(m):
        n = int(m.group(1))
        if not low <= n <= high:
            return ''
        x = 18 + (white(n) - white(low)) * width + (width * 0.7 if n % 12 in BLACK else 0)
        return move(m.group(0), x, 174, width / 10)
    art = re.sub(rf'<path id="{P}_key_(\d+)"[^>]*/>', key, art)
    def played(m):
        n = int(m.group(1))
        if n not in frame['played'] or not low <= n <= high:
            return ''
        black = n % 12 in BLACK
        x = 18 + (white(n) - white(low)) * width + (width if black else width / 2)
        return move(m.group(0), x, 195 if black else 211)
    art = re.sub(rf'<circle id="{P}_played_(\d+)"[^>]*/>', played, art)
    return art


def header(frame):
    out = ['<rect x="0" y="0" width="480" height="38" fill="#000"/>']
    page = frame['page']
    for row in range(2):
        bright = row == page
        y = 15 if row == 0 else 33
        for col in range(4):
            i = 4 * row + col
            colour = '#E6E6E6' if bright else '#5A5A5A'
            size = 15 if bright else 11
            out.append(f'<text x="{5 + 120 * col}" y="{y}" fill="{colour}" font-size="{size - 4}">{TITLES[i]}</text>')
            out.append(f'<text x="{60 + 120 * col}" y="{y}" fill="{colour}" font-size="{size}">{html.escape(frame["values"][i])}</text>')
    return ''.join(out)


frames = json.loads(subprocess.run([sys.argv[1]], capture_output=True, text=True, check=True).stdout)
folder = pathlib.Path(sys.argv[2])
folder.mkdir(parents=True, exist_ok=True)
for n, frame in enumerate(frames, 1):
    art = pose(ART, frame).replace('UniversNextPro-Light', 'Helvetica, Arial, sans-serif')
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="480" height="220" viewBox="0 0 480 220" '
           f'font-family="Helvetica, Arial, sans-serif"><rect width="480" height="220" fill="#000"/>{art}{header(frame)}</svg>')
    (folder / f'page-{n}.svg').write_text(svg)
    print(f'page-{n}.svg: {frame["caption"]}')
