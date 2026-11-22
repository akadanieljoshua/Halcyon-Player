#!/usr/bin/env python3
"""Draws Halcyon's page for review from tools/shot.cpp's frames: the
artwork with its dots shown or hidden as the SDK's player page does
(screen_player.hpp), under the SDK's two-page header (its template, posed by opxy/page_header.hpp).
Not the device's renderer.
python tools/screens.py <shot binary> <out folder>   (writes page-<n>.svg)"""
import html, json, pathlib, re, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
P = 'halcyon_player'
ART = (ROOT / 'panel.svg.fragment').read_text()


def pose(art, frame):
    def dot(m):
        return m.group(0) if frame['lit'] >> int(m.group(1)) & 1 else ''
    art = re.sub(rf'<path id="{P}_dot_(\d+)"[^>]*/>', dot, art)
    # The line of text, centred on x=240 as the SDK centres it.
    return re.sub(rf'(<text id="{P}_text")([^>]*)></text>',
                  lambda m: f'{m.group(1)} text-anchor="middle"{m.group(2)}>{html.escape(frame["text"])}</text>', art)


HEADER = (ROOT.parent / 'open-xy/sdk/templates/shared/page_header.svg.in')
Q = 1024


def header(frame):
    """The SDK's two-page header template, posed from opxy/page_header.hpp's frame."""
    art = HEADER.read_text().replace('@ID@', 'h')
    for i in range(8):
        art = art.replace(f'@NODE_{i}@', str(i))
    def node(m):
        x, y, scale = frame['nodes'][int(m.group(1))]
        return re.sub(r'transform="[^"]*"', f'transform="matrix({scale / Q:.4f} 0 0 {scale / Q:.4f} {x / Q:.3f} {y / Q:.3f})"', m.group(0))
    art = re.sub(r'<path id="h_node_(\d+)"[^>]*/>', node, art)
    def text(m):
        x, y, scale, chars = frame['texts'][int(m.group(1))]
        tag = re.sub(r'transform="[^"]*"', f'transform="matrix({scale / Q:.3f} 0 0 {scale / Q:.3f} {x / Q:.3f} {y / Q:.3f})"', m.group(0))
        return re.sub(r'>[^<]*</text>', f'>{html.escape(chars)}</text>', tag)
    art = re.sub(r'<text id="h_text_(\d+)"[^>]*>[^<]*</text>', text, art)
    return re.sub(r'<!--.*?-->', '', art)


frames = json.loads(subprocess.run([sys.argv[1]], capture_output=True, text=True, check=True).stdout)
folder = pathlib.Path(sys.argv[2])
folder.mkdir(parents=True, exist_ok=True)
for n, frame in enumerate(frames, 1):
    art = pose(ART, frame).replace('UniversNextPro-Light', 'Helvetica, Arial, sans-serif')
    top = header(frame).replace('UniversNextPro-Light', 'Helvetica, Arial, sans-serif')
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="480" height="220" viewBox="0 0 480 220" '
           f'font-family="Helvetica, Arial, sans-serif"><rect width="480" height="220" fill="#000"/>{top}{art}</svg>')
    (folder / f'page-{n}.svg').write_text(svg)
    print(f'page-{n}.svg: {frame["caption"]}')
