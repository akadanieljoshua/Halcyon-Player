#!/usr/bin/env python3
"""Draws Halcyon's own page from tools/shot.cpp's frames onto its artwork,
for review: python tools/screens.py <shot binary> <out folder> [every]
writes page-<n>.svg for every n-th frame (default 30, a second apart).
Not the device's renderer."""
import html, json, pathlib, re, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
P = 'halcyon_player'
Q = 1024
ART = (ROOT / 'panel.svg.fragment').read_text()
PALETTE = re.findall(rf'id="{P}_palette_\d+"[^>]*fill="([^"]+)" stroke="([^"]+)" stroke-width="([^"]+)"', ART)


def apply(frame):
    art = re.sub(rf'<circle id="{P}_palette[^>]*/>', '', ART)
    def node(m):
        i = int(m.group(2)); tag = m.group(0)
        x, y, scale, c, s, shade, paint = frame['nodes'][i]
        matrix = f'matrix({scale*c/Q/Q:.4f} {scale*s/Q/Q:.4f} {-scale*s/Q/Q:.4f} {scale*c/Q/Q:.4f} {x/Q:.3f} {y/Q:.3f})'
        tag = re.sub(r'transform="[^"]*"', f'transform="{matrix}"', tag)
        if paint:
            fill, stroke, width = PALETTE[shade]
            tag = re.sub(r'stroke="[^"]*"', f'stroke="{stroke}"', tag, count=1)
            tag = re.sub(r'stroke-width="[^"]*"', f'stroke-width="{width}"', tag)
            if paint == 2 and 'fill="none"' not in tag:
                tag = re.sub(r'fill="[^"]*"', f'fill="{fill}"', tag, count=1)
        return tag
    art = re.sub(rf'<(path|circle) id="{P}_node_(\d+)"[^>]*/>', node, art)
    def text(m):
        i = int(m.group(1)); x, y, scale, chars = frame['texts'][i]
        tag = re.sub(r'transform="[^"]*"', f'transform="matrix({scale/Q:.3f} 0 0 {scale/Q:.3f} {x/Q:.3f} {y/Q:.3f})"', m.group(0))
        return re.sub(r'>[^<]*</text>', f'>{html.escape(chars)}</text>', tag)
    art = re.sub(rf'<text id="{P}_text_(\d+)"[^>]*>[^<]*</text>', text, art)
    art = art.replace('UniversNextPro-Light', 'Helvetica Neue, Arial')
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="480" height="220" viewBox="0 0 480 220">'
            f'<rect width="480" height="220" fill="#000"/>{art}</svg>')


if __name__ == '__main__':
    shot = json.loads(subprocess.run([sys.argv[1]], capture_output=True, text=True, check=True).stdout)
    folder = pathlib.Path(sys.argv[2])
    folder.mkdir(parents=True, exist_ok=True)
    every = int(sys.argv[3]) if len(sys.argv) > 3 else 30
    for n, frame in enumerate(shot['frames']):
        if n % every == 0:
            (folder / f'page-{n:04d}.svg').write_text(apply(frame))
