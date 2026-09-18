#!/usr/bin/env python3
"""Build the assembly guide's SVG/PNG illustrations, Markdown, HTML and PDF.

Usage: python tools/build-assembly-guide.py
Dependencies: docs/assembly/requirements.txt. No network access during builds.
"""
from __future__ import annotations

import html
import json
import os
from pathlib import Path
import re
import xml.etree.ElementTree as ET

import cairosvg
from PIL import Image
from reportlab.graphics import renderPDF
from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.utils import ImageReader
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas
from reportlab.platypus import Paragraph, Table, TableStyle
from svglib.svglib import svg2rlg

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "docs/assembly"
ASSETS = DEST / "assets"
PDF = ROOT / "output/pdf/esp32-s3-supermini-assembly-guide.pdf"
DATA = json.loads((DEST / "guide.json").read_text())
INK = "#19393C"
TEAL = "#007D79"
MINT = "#DFF2E9"
CREAM = "#FAF8F0"
ORANGE = "#D56B30"
YELLOW = "#F8C966"
PURPLE = "#7154A1"
MUTED = "#53696B"
RED = "#B74649"
BLUE = "#316CBA"
ESC = html.escape


class SVG:
    def __init__(self, width, height, title, desc):
        self.width, self.height = width, height
        self.parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" role="img" aria-labelledby="title desc">',
                      f'<title id="title">{ESC(title)}</title><desc id="desc">{ESC(desc)}</desc>']
        self.rect(0, 0, width, height, CREAM, r=20)

    def rect(self, x, y, w, h, fill, stroke="none", r=0, sw=2):
        self.parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>')

    def text(self, x, y, value, size=18, fill=INK, bold=False, anchor="start"):
        self.parts.append(f'<text x="{x}" y="{y}" font-family="Liberation Sans, Arial, sans-serif" font-size="{size}" font-weight="{"bold" if bold else "normal"}" fill="{fill}" text-anchor="{anchor}">{ESC(str(value))}</text>')

    def circle(self, x, y, r, fill, stroke="none", sw=2):
        self.parts.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>')

    def line(self, x1, y1, x2, y2, color=TEAL, sw=3, dashed=False):
        dash = ' stroke-dasharray="6 5"' if dashed else ''
        self.parts.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{color}" stroke-width="{sw}" stroke-linecap="round"{dash}/>')

    def path(self, d, fill="none", stroke=INK, sw=3):
        self.parts.append(f'<path d="{d}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}" stroke-linecap="round" stroke-linejoin="round"/>')

    def save(self, name):
        path = ASSETS / f"{name}.svg"
        path.write_text("\n".join(self.parts + ["</svg>"]) + "\n")
        ET.parse(path)
        cairosvg.svg2png(url=str(path), write_to=str(path.with_suffix(".png")), output_width=self.width * 2, output_height=self.height * 2)


def wire_diagram(name, title, module, connections, note):
    height = 140 + len(connections) * 42
    s = SVG(900, height, title, note)
    s.rect(24, 24, 245, height - 84, INK, r=15)
    s.rect(630, 24, 246, height - 84, PURPLE if name == "dac" else TEAL, r=15)
    s.text(47, 57, "ESP32-S3 / rails", 22, "white", True)
    s.text(654, 57, module, 22, "white", True)
    for i, (left, right, color) in enumerate(connections):
        y = 93 + i * 42
        s.text(48, y + 6, left, 21, "white", True)
        s.text(655, y + 6, right, 21, "white", True)
        s.line(268, y, 630, y, color, 4)
        s.circle(269, y, 6, color)
        s.circle(630, y, 6, color)
        if left.startswith("GPIO"):
            s.rect(363, y - 13, 172, 26, CREAM, r=8)
            s.text(449, y + 6, "signal: " + left, 16, INK, True, "middle")
    footer = "*VIN: verified 5V regulator-equipped board only. Match labels, not pin positions." if name == "dac" else "Match printed signal labels, not header positions. Unplug USB before rewiring."
    s.text(450, height - 15, footer, 17, MUTED, anchor="middle")
    s.save(name)


def artwork():
    ASSETS.mkdir(parents=True, exist_ok=True)
    s = SVG(900, 485, "Your little music machine", "Illustrated top view: OLED upper center, microphone beside it, joystick left, encoder right, USB and line-out on the front edge.")
    s.circle(112, 90, 44, MINT)
    s.circle(796, 380, 67, "#FBE8BB")
    s.rect(216, 44, 476, 382, "#CCDCD6", r=44)
    s.rect(205, 28, 476, 382, MINT, INK, r=44, sw=4)
    s.rect(356, 88, 190, 100, INK, r=13)
    s.text(374, 116, "JAM / DRUMS", 16, "#9AF4D8", True)
    for row in range(3):
        for col in range(12):
            s.rect(374 + col * 13, 130 + row * 14, 9, 9, "#9AF4D8" if (col + row) % (row + 2) == 0 else "#42645E", r=2)
    for dx, dy in [(0, 0), (-13, 0), (13, 0), (-7, -12), (7, -12), (-7, 12), (7, 12)]:
        s.circle(601 + dx, 142 + dy, 3.8, INK)
    s.circle(320, 281, 60, "#BCD4CC", INK, 2)
    s.circle(320, 278, 39, INK)
    s.circle(310, 268, 26, "#35585A")
    s.circle(555, 282, 48, INK)
    s.circle(555, 275, 43, YELLOW, INK, 3)
    s.line(555, 239, 555, 251, INK, 4)
    s.text(443, 365, "MAKE A LITTLE NOISE", 15, INK, True, "middle")
    s.rect(419, 398, 48, 13, INK, r=5)
    s.circle(580, 403, 10, INK)
    for x, y in [(231, 56), (656, 56), (231, 382), (656, 382)]:
        s.circle(x, y, 4, "#8AA69B")
    for label, x, y, tx, ty in [("OLED", 95, 128, 356, 128), ("MIC", 752, 143, 622, 143), ("JOYSTICK", 45, 302, 255, 302), ("ENCODER", 735, 286, 604, 286)]:
        s.text(x, y, label, 19, INK, True)
        if x < 200:
            s.line(x + (115 if label == "JOYSTICK" else 75), y - 6, tx - 10, ty - 6, TEAL, 2)
        else:
            s.line(tx + 8, ty - 6, x - 10, y - 6, TEAL, 2)
    s.line(443, 417, 443, 445, TEAL, 2)
    s.line(580, 417, 580, 445, TEAL, 2)
    s.text(443, 469, "USB-C", 18, INK, True, "middle")
    s.text(605, 469, "LINE-OUT", 18, INK, True, "middle")
    s.save("hero")

    s = SVG(900, 330, "Meet the seven modules", "Simplified module illustrations, not a pinout or dimension reference.")
    labels = [("ESP32-S3", "the brain"), ("PCM5102A", "line-out"), ("JOYSTICK", "tilt + click"), ("EC11", "turn + click"), ("INMP441", "microphone"), ("SSD1306", "the screen"), ("MPU6050", "optional motion")]
    for i, (name, role) in enumerate(labels):
        row, col = divmod(i, 4)
        x = 25 + col * 224 + (112 if row else 0)
        y = 15 + row * 159
        s.rect(x, y, 202, 143, "white", "#DAE4DF", r=12, sw=1)
        cx, cy = x + 101, y + 49
        if i == 0:
            s.rect(cx - 26, cy - 31, 52, 62, BLUE, r=5)
            s.rect(cx - 18, cy - 23, 36, 31, "#D7DCDA", r=2)
            s.rect(cx - 11, cy + 23, 22, 12, INK, r=3)
        elif i == 1:
            s.rect(cx - 45, cy - 24, 90, 48, PURPLE, r=4)
            s.rect(cx + 15, cy - 15, 36, 30, INK, r=2)
            s.circle(cx + 50, cy, 9, YELLOW)
        elif i == 2:
            s.rect(cx - 37, cy - 29, 74, 61, TEAL, r=3)
            s.circle(cx, cy, 26, INK)
            s.circle(cx - 4, cy - 5, 17, "#426467")
        elif i == 3:
            s.rect(cx - 22, cy - 10, 44, 39, "#A9B6B6", INK, r=3)
            s.rect(cx - 9, cy - 34, 18, 37, "#D4DBD8", INK, r=2)
            for j in [-1, 0, 1]:
                s.line(cx + j * 17, cy + 30, cx + j * 17, cy + 39, INK, 3)
        elif i == 4:
            s.circle(cx, cy, 32, PURPLE)
            s.rect(cx - 12, cy - 9, 24, 18, "#DDE2DF", r=2)
            for j in range(6):
                s.circle(cx - 25 + j * 10, cy + 20, 3, YELLOW)
        elif i == 5:
            s.rect(cx - 44, cy - 30, 88, 60, BLUE, r=4)
            s.rect(cx - 38, cy - 22, 76, 38, INK, r=3)
            s.text(cx, cy + 2, "HELLO!", 15, MINT, True, "middle")
        else:
            s.rect(cx - 28, cy - 31, 56, 62, BLUE, r=4)
            s.rect(cx - 10, cy - 9, 20, 20, INK, r=2)
            s.circle(cx, cy - 23, 4, CREAM)
        s.text(cx, y + 111, name, 18, INK, True, "middle")
        s.text(cx, y + 133, role, 16, MUTED, False, "middle")
    s.save("parts")

    s = SVG(900, 300, "Two rails and one common ground", "USB powers ESP32. 3V3 supplies verified 3.3V modules. Verified USB 5V supplies the regulator-equipped DAC VIN. All grounds join.")
    s.rect(22, 101, 106, 90, INK, r=13)
    s.text(75, 139, "USB", 24, "white", True, "middle")
    s.text(75, 168, "5 V in", 18, "white", False, "middle")
    s.line(128, 145, 175, 145, INK, 5)
    s.rect(175, 38, 260, 229, MINT, TEAL, r=14)
    s.text(195, 74, "ESP32-S3", 23, INK, True)
    for label, y, color, out, sub in [("3V3", 110, RED, "MIC + OLED + JOYSTICK", "Verified 3.3 V-compatible boards"), ("USB 5V*", 176, ORANGE, "PCM5102A VIN*", "*Verify board regulator and VIN rating"), ("GND", 240, INK, "EVERY MODULE GND", "One common reference")]:
        s.text(195, y + 6, label, 21, color, True)
        s.line(325, y, 510, y, color, 4)
        s.circle(510, y, 5, color)
        s.text(535, y - 3, out, 19, color, True)
        s.text(535, y + 21, sub, 15, MUTED)
    s.save("power")

    wire_diagram("display", "OLED wiring", "SSD1306 OLED", [("3V3", "VCC / VDD", RED), ("GND", "GND", INK), ("GPIO12", "SDA", TEAL), ("GPIO13", "SCL", BLUE)], "Four-wire I2C display; match pin labels on your exact board.")
    wire_diagram("microphone", "INMP441 wiring", "INMP441 mic", [("3V3", "VDD", RED), ("GND", "GND", INK), ("GPIO7", "SCK", ORANGE), ("GPIO8", "WS", BLUE), ("GPIO10", "SD", TEAL), ("GND", "L/R", INK)], "Mic SCK goes to GPIO7; mic SD sends audio into GPIO10; L/R tied to ground.")
    wire_diagram("dac", "PCM5102A wiring", "PCM5102A DAC", [("USB 5V*", "VIN*", ORANGE), ("GND", "GND", INK), ("GPIO7", "BCK", ORANGE), ("GPIO8", "LCK / LRCK", BLUE), ("GPIO9", "DIN", TEAL), ("GND", "SCK", INK)], "VIN requires a verified 5V-compatible regulator-equipped board. SCK is grounded, unlike the mic SCK.")

    s = SVG(900, 345, "Joystick and bare encoder logical contacts", "Not physical pin order. Joystick powered from 3V3. Encoder common and one push contact go to ground.")
    for x, name, rows in [(24, "JOYSTICK BREAKOUT", [("VCC", "3V3"), ("GND", "GND"), ("VRy", "GPIO1"), ("VRx", "GPIO2"), ("SW", "GPIO3")]), (461, "BARE EC11 CONTACTS", [("A", "GPIO4"), ("B", "GPIO5"), ("Common C", "GND"), ("Push contact 1", "GPIO6"), ("Push contact 2", "GND")])]:
        s.rect(x, 20, 414, 275, "white", "#C9DDD5", r=12)
        s.text(x + 19, 54, name, 21, INK, True)
        for i, (label, dest) in enumerate(rows):
            y = 95 + i * 39
            s.text(x + 19, y, label, 19, INK, True)
            s.line(x + 175, y - 6, x + 265, y - 6, INK if dest == "GND" else TEAL, 3)
            s.text(x + 285, y, dest, 20, TEAL, True)
    s.text(450, 328, "Logical contacts, not physical pin order. Confirm your EC11 pinout before soldering.", 17, MUTED, anchor="middle")
    s.save("controls")

    s = SVG(900, 380, "Where the parts live", "Shared top-view coordinates. Shell: OLED and microphone above controls. Lid: IMU left, ESP32 front center, DAC front right. USB and jack face the same front edge.")
    for x, title in [(40, "TOP SHELL / component positions"), (482, "BOTTOM LID / component positions")]:
        s.text(x + 174, 32, title, 19, INK, True, "middle")
        s.rect(x, 48, 350, 286, MINT, INK, r=25)
        for dx, dy in [(20, 20), (330, 20), (20, 267), (330, 267)]:
            s.circle(x + dx, 48 + dy, 7, CREAM, INK, 2)
        s.text(x + 174, 363, "FRONT EDGE / USB + AUDIO", 17, TEAL, True, "middle")
    s.rect(170, 93, 96, 63, INK, r=6)
    s.text(218, 129, "OLED", 18, MINT, True, "middle")
    s.circle(313, 141, 24, PURPLE)
    s.text(313, 148, "MIC", 16, "white", True, "middle")
    s.circle(118, 235, 42, INK)
    s.text(118, 242, "JOY", 18, "white", True, "middle")
    s.circle(285, 235, 34, YELLOW, INK, 2)
    s.text(285, 242, "EC11", 17, INK, True, "middle")
    s.rect(528, 120, 70, 62, BLUE, r=5)
    s.text(563, 146, "IMU", 17, "white", True, "middle")
    s.text(563, 167, "optional", 14, "white", False, "middle")
    s.rect(626, 245, 64, 79, BLUE, r=5)
    s.text(658, 275, "ESP32", 16, "white", True, "middle")
    s.rect(641, 315, 34, 16, INK, r=3)
    s.rect(720, 219, 66, 105, PURPLE, r=5)
    s.text(753, 249, "DAC", 18, "white", True, "middle")
    s.circle(735, 326, 9, INK)
    s.save("placement")


CSS = """
:root{--ink:#19393c;--teal:#007d79;--cream:#faf8f0;--mint:#dff2e9;--muted:#53696b}
*{box-sizing:border-box}html{scroll-behavior:smooth;scroll-padding-top:5rem}
body{margin:0;background:var(--cream);color:var(--ink);font:17px/1.6 system-ui,sans-serif}
a{color:var(--teal);text-underline-offset:3px}a:hover{text-decoration-thickness:2px}
header{background:var(--ink);color:white;padding:1rem 4vw;display:flex;gap:1rem;align-items:center;justify-content:space-between;position:sticky;top:0;z-index:2}
header a{color:#b8f2dc;font-size:.88rem}header strong{font-size:.95rem}#progress{font-variant-numeric:tabular-nums;font-size:.8rem}
.layout{max-width:1300px;margin:auto;display:grid;grid-template-columns:240px minmax(0,1fr);gap:2.5rem;padding:2rem}
nav{align-self:start;position:sticky;top:6rem;max-height:80vh;overflow:auto;font-size:.85rem}nav a{display:block;padding:.35rem .6rem;text-decoration:none;border-left:2px solid #d9e5df}nav a:hover{background:var(--mint)}
main{min-width:0}section{background:white;padding:2.5rem;border:1px solid #dce6e0;border-radius:20px;margin-bottom:2rem;box-shadow:0 6px 20px #19393c05}
.tag{font-size:.72rem;font-weight:800;letter-spacing:.15em;color:var(--teal);margin:0 0 .6rem}h1,h2{font-size:clamp(1.8rem,3.2vw,2.8rem);line-height:1.08;letter-spacing:-.045em;margin:.5rem 0 1.2rem}h3{font-size:1.1rem;margin:1.5rem 0 .7rem}.intro{font-size:1.16rem;color:var(--muted)}
figure{margin:1.5rem 0}figure img{display:block;width:100%;height:auto;border-radius:12px}figure.photo img{max-height:280px;object-fit:contain;background:#1d1f20}figcaption{font-size:.78rem;color:var(--muted);margin-top:.5rem}.table-scroll{overflow-x:auto}
table{width:100%;border-collapse:collapse;font-size:.9rem;margin:1.2rem 0}th{background:var(--ink);color:white;text-align:left}th,td{padding:.7rem .8rem;vertical-align:top}tbody tr:nth-child(even){background:#f1f6f2}td{border-bottom:1px solid #dde6df}
.callout{border-left:5px solid var(--teal);background:var(--mint);padding:1rem 1.2rem;border-radius:0 10px 10px 0;margin:1.4rem 0}.callout strong{display:block;font-size:.75rem;letter-spacing:.08em;margin-bottom:.4rem}.callout p{margin:0}pre{white-space:pre-wrap;overflow-wrap:anywhere;background:#eff3f1;border:1px solid #d7e2db;padding:1rem;font:12px/1.7 ui-monospace,monospace;border-radius:9px}li{margin-bottom:.65rem}ul,ol{padding-left:1.5rem}
.checks{background:#fff6df;padding:1rem 1.2rem;border-radius:10px;margin:1.3rem 0}.checks label{display:flex;align-items:baseline;gap:.65rem;margin:.7rem 0}.checks input{width:1.1rem;height:1.1rem;flex-shrink:0;accent-color:var(--teal)}.checks label:has(input:checked){color:var(--teal)}.links{font-size:.88rem}footer{padding:0 2rem 3rem;color:var(--muted);text-align:center;font-size:.82rem}button{font:inherit;border:1px solid #a4bcb0;border-radius:7px;background:white;color:var(--ink);padding:.5rem .8rem;cursor:pointer}
@media(max-width:850px){.layout{display:block;padding:1rem}nav{position:static;max-height:none;margin-bottom:1rem;display:flex;flex-wrap:wrap;gap:.25rem}nav a{border:1px solid #d9e5df;border-radius:8px;padding:.3rem .5rem}section{padding:1.4rem}header{flex-wrap:wrap;position:static}.intro{font-size:1.05rem}th,td{padding:.5rem;font-size:.82rem}}
@media print{header,nav,.actions,footer{display:none}.layout{display:block;padding:0}section{break-before:page;box-shadow:none;border:0;padding:0;font-size:10pt;line-height:1.35}section:first-child{break-before:auto}h1,h2{font-size:25pt}figure img{max-height:180px;object-fit:contain}table{font-size:9pt}a{color:inherit}body{background:white}pre{font-size:8pt}}
"""


def media(page):
    return f'assets/{page["diagram"]}.svg' if page.get("diagram") else page.get("image")


def render_html():
    nav, sections = [], []
    for n, p in enumerate(DATA["pages"], 1):
        nav.append(f'<a href="#{p["id"]}">{n:02} / {ESC(p["title"])}</a>')
        heading = "h1" if n == 1 else "h2"
        body = [f'<section id="{p["id"]}"><p class="tag">{ESC(p["tag"])}</p><{heading}>{ESC(p["title"])}</{heading}><p class="intro">{ESC(p["intro"])}</p>']
        src = media(p)
        if src:
            body.append(f'<figure class="{"photo" if p.get("image") else "diagram"}"><img src="{src}" alt="{ESC(p["caption"])}" loading="lazy"><figcaption>{ESC(p["caption"])}</figcaption></figure>')
        for j, b in enumerate(p["blocks"]):
            typ = b["type"]
            if typ == "p": body.append(f'<p>{ESC(b["text"])}</p>')
            elif typ == "heading": body.append(f'<h3>{ESC(b["text"])}</h3>')
            elif typ == "callout": body.append(f'<aside class="callout"><strong>{ESC(b["label"])}</strong><p>{ESC(b["text"])}</p></aside>')
            elif typ == "code": body.append(f'<pre><code>{ESC(b["text"])}</code></pre>')
            elif typ in ("bullets", "steps"):
                tag = "ol" if typ == "steps" else "ul"
                body.append(f'<{tag}>' + "".join(f'<li>{ESC(x)}</li>' for x in b["items"]) + f'</{tag}>')
            elif typ == "table":
                body.append('<div class="table-scroll"><table><thead><tr>' + "".join(f'<th scope="col">{ESC(x)}</th>' for x in b["headers"]) + '</tr></thead><tbody>' + "".join('<tr>' + "".join(f'<td>{ESC(x)}</td>' for x in row) + '</tr>' for row in b["rows"]) + '</tbody></table></div>')
            elif typ == "check":
                body.append('<div class="checks"><strong>Checkpoint</strong>')
                for k, x in enumerate(b["items"]):
                    key = f'{p["id"]}-{j}-{k}'
                    body.append(f'<label><input type="checkbox" data-key="{key}"><span>{ESC(x)}</span></label>')
                body.append('</div>')
            elif typ == "links": body.append('<ul class="links">' + "".join(f'<li><a href="{ESC(url)}">{ESC(label)}</a></li>' for label, url in b["items"]) + '</ul>')
            else: raise ValueError(typ)
        sections.append("\n".join(body + ["</section>"]))
    script = """
const boxes = [...document.querySelectorAll('input[data-key]')];
const store = 'supermini-assembly-v1:';
for (const box of boxes) {
  try { box.checked = localStorage.getItem(store + box.dataset.key) === '1'; } catch (_) {}
  box.addEventListener('change', () => {
    try { localStorage.setItem(store + box.dataset.key, box.checked ? '1' : '0'); } catch (_) {}
    progress();
  });
}
function progress() {
  document.getElementById('progress').textContent = boxes.filter(x => x.checked).length + ' / ' + boxes.length + ' checkpoints';
}
document.getElementById('reset').addEventListener('click', () => {
  if (!confirm('Clear your assembly checklist progress?')) return;
  for (const box of boxes) { box.checked = false; try { localStorage.removeItem(store + box.dataset.key); } catch (_) {} }
  progress();
});
progress();
"""
    (DEST / "index.html").write_text('<!doctype html>\n<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Make a little noise | SuperMini assembly</title><meta name="description" content="A beginner-friendly ESP32-S3 SuperMini assembly guide with labeled wiring, checkpoints, and a printable PDF."><style>' + CSS + '</style></head><body><header><strong>SUPERMINI / BUILD & PLAY</strong><span id="progress" aria-live="polite"></span><a href="../../output/pdf/esp32-s3-supermini-assembly-guide.pdf">Download the 16-page PDF</a></header><div class="layout"><nav aria-label="Guide chapters">' + "\n".join(nav) + '</nav><main>' + "\n".join(sections) + '</main></div><footer>Prototype edition / September 2026. Checklist stays in this browser when local storage is available.<p class="actions"><button id="reset" type="button">Reset my checklist</button></p></footer><script>' + script + '</script></body></html>\n')


def render_markdown():
    out = ["# Make a little noise.", "", DATA["subtitle"], "", "[Download the printable PDF](../../output/pdf/esp32-s3-supermini-assembly-guide.pdf) · [Open the browser checklist](index.html) · [SVG and PNG artwork](assets/)", "", "> On GitHub, download the repository and open `docs/assembly/index.html` locally for the interactive version. No server or internet connection is needed to read it.", "", "## Contents", ""]
    for i, p in enumerate(DATA["pages"], 1):
        out.append(f'{i}. [{p["title"]}](#{p["id"]})')
    for i, p in enumerate(DATA["pages"], 1):
        out.extend(["", f'<a id="{p["id"]}"></a>', "", f'## {i:02}. {p["title"]}', "", f'*{p["tag"]}*', "", p["intro"], ""])
        src = media(p)
        if src: out.extend([f'![{p["caption"]}]({src})', ""])
        for b in p["blocks"]:
            typ = b["type"]
            if typ == "p": out.append(b["text"])
            elif typ == "heading": out.append("### " + b["text"])
            elif typ == "callout": out.extend([f'> **{b["label"]}**', ">", "> " + b["text"]])
            elif typ == "code": out.extend(["```sh", b["text"], "```"])
            elif typ in ("bullets", "steps", "check"):
                out.extend((f'{k + 1}. ' if typ == "steps" else '- [ ] ' if typ == "check" else '- ') + x for k, x in enumerate(b["items"]))
            elif typ == "table":
                out.append("| " + " | ".join(b["headers"]) + " |")
                out.append("| " + " | ".join("---" for _ in b["headers"]) + " |")
                out.extend("| " + " | ".join(row) + " |" for row in b["rows"])
            elif typ == "links": out.extend(f'- [{label}]({url})' for label, url in b["items"])
            out.append("")
    out.extend(["## Rebuild the guide", "", "The content lives in `guide.json`; the builder creates all illustrations and formats from it. Requires Python 3.10+, Cairo, and Liberation Sans/Mono fonts (or set `KIT_GUIDE_FONT_DIR` to a directory containing the matching TTF files).", "", "```sh", "python3 -m venv /tmp/kit-guide-venv", "/tmp/kit-guide-venv/bin/pip install -r docs/assembly/requirements.txt", "/tmp/kit-guide-venv/bin/python tools/build-assembly-guide.py", "```", "", "The PDF goes to `output/pdf/`. The builder checks the documented GPIO map against `firmware/main/pins.h` and fails on overflowing PDF pages. After edits, render the PDF with `pdftoppm` and visually inspect all pages. Check hardware-specific statements again if firmware or enclosure geometry changes.", ""])
    (DEST / "README.md").write_text("\n".join(out))


def register_fonts():
    dirs = [Path(os.environ.get("KIT_GUIDE_FONT_DIR", "/usr/share/fonts/liberation")), Path("/usr/share/fonts/truetype/liberation2"), Path("/usr/share/fonts/truetype/liberation")]
    for folder in dirs:
        if (folder / "LiberationSans-Regular.ttf").exists():
            for name, filename in [("Kit", "LiberationSans-Regular.ttf"), ("KitBold", "LiberationSans-Bold.ttf"), ("KitMono", "LiberationMono-Regular.ttf")]:
                pdfmetrics.registerFont(TTFont(name, str(folder / filename)))
            pdfmetrics.registerFontFamily("Kit", normal="Kit", bold="KitBold", italic="Kit", boldItalic="KitBold")
            return
    raise RuntimeError("Install Liberation fonts or set KIT_GUIDE_FONT_DIR. See docs/assembly/README.md.")


def style(size=10.5, leading=14.4, color=INK, bold=False):
    return ParagraphStyle("kit", fontName="KitBold" if bold else "Kit", fontSize=size, leading=leading, textColor=colors.HexColor(color), alignment=TA_LEFT, spaceAfter=0)


def paragraph(text, width, st=None):
    p = Paragraph(text, st or style())
    _, h = p.wrap(width, 1000)
    return p, h


def flow_blocks(page, width):
    """Return measured draw operations so every chapter stays on one page."""
    ops = []
    def para(text, size=10.4, leading=14.1, bold=False, color=INK, gap=8):
        obj, height = paragraph(ESC(text), width, style(size, leading, color, bold))
        ops.append(("flow", obj, height, gap))
    for block in page["blocks"]:
        typ = block["type"]
        if typ == "p": para(block["text"])
        elif typ == "heading": para(block["text"], 12, 15, True, gap=5)
        elif typ == "callout":
            head, hh = paragraph(ESC(block["label"]), width - 28, style(8.7, 12, TEAL, True))
            body, bh = paragraph(ESC(block["text"]), width - 28, style(10.1, 13.6))
            ops.append(("callout", (head, hh, body, bh), hh + bh + 23, 9))
        elif typ in ("bullets", "steps", "check"):
            for i, text in enumerate(block["items"]):
                obj, height = paragraph(ESC(text), width - 23, style(10.4, 14.1))
                ops.append(("item", (obj, str(i + 1) if typ == "steps" else "box" if typ == "check" else "dot"), height, 7))
        elif typ == "code":
            # Break only at whitespace and preserve every command on one line where possible.
            lines = block["text"].splitlines()
            code_size = 8.1
            max_width = max(pdfmetrics.stringWidth(x, "KitMono", code_size) for x in lines)
            if max_width > width - 24:
                code_size *= (width - 24) / max_width
            if code_size < 7.1:
                raise ValueError("Command line too long; rewrite it for legible printing")
            ops.append(("code", (lines, code_size), len(lines) * 12 + 20, 9))
        elif typ == "links":
            for label, url in block["items"]:
                obj, height = paragraph(f'<link href="{ESC(url)}" color="{TEAL}">{ESC(label)}</link>', width, style(9.1, 12.2))
                ops.append(("flow", obj, height, 3))
            ops.append(("space", None, 3, 0))
        elif typ == "table":
            columns = len(block["headers"])
            if columns == 3:
                widths = [width * .10, width * .30, width * .60] if block["headers"][0] == "Qty" else [width * .28, width * .60, width * .12]
            else:
                widths = [width * .34, width * .66]
            rows = [[Paragraph(ESC(t), style(9.1, 11.6, "#FFFFFF", True)) for t in block["headers"]]]
            for row in block["rows"]:
                rows.append([Paragraph(ESC(t), style(9.3, 12.1)) for t in row])
            table = Table(rows, colWidths=widths, hAlign="LEFT")
            table.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, 0), colors.HexColor(INK)), ("VALIGN", (0, 0), (-1, -1), "TOP"), ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#F0F5F1")]), ("LEFTPADDING", (0, 0), (-1, -1), 8), ("RIGHTPADDING", (0, 0), (-1, -1), 8), ("TOPPADDING", (0, 0), (-1, -1), 6), ("BOTTOMPADDING", (0, 0), (-1, -1), 6), ("LINEBELOW", (0, 0), (-1, 0), .4, colors.HexColor(INK)), ("LINEBELOW", (0, 1), (-1, -1), .3, colors.HexColor("#DCE5DE"))]))
            _, height = table.wrap(width, 1000)
            ops.append(("flow", table, height, 10))
        else: raise ValueError(typ)
    return ops


def render_pdf():
    register_fonts()
    PDF.parent.mkdir(parents=True, exist_ok=True)
    W, H = 595.276, 841.890  # A4
    left, width = 43, W - 86
    c = canvas.Canvas(str(PDF), pagesize=(W, H), pageCompression=1, invariant=1)
    c.setTitle("Make a little noise - ESP32-S3 SuperMini assembly guide")
    c.setAuthor("ESP32-S3 SuperMini Audio Kit")
    c.setSubject("Beginner-friendly prototype assembly, wiring, firmware, enclosure and first beat")
    layout = []
    for n, p in enumerate(DATA["pages"], 1):
        c.bookmarkPage(p["id"])
        c.addOutlineEntry(p["title"], p["id"], level=0)
        c.setFillColor(colors.HexColor(CREAM))
        c.rect(0, 0, W, H, fill=1, stroke=0)
        c.setFillColor(colors.HexColor(TEAL))
        c.rect(0, H - 7, W * n / len(DATA["pages"]), 7, fill=1, stroke=0)
        c.setFont("KitBold", 8.2)
        c.drawString(left, H - 34, "SUPERMINI / BUILD & PLAY")
        c.setFillColor(colors.HexColor(MUTED))
        c.setFont("Kit", 8)
        c.drawRightString(W - left, H - 34, "PROTOTYPE EDITION  /  SEPT 2026")
        c.setFillColor(colors.HexColor(TEAL))
        c.setFont("KitBold", 8.5)
        c.drawString(left, H - 66, p["tag"])
        title, th = paragraph(ESC(p["title"]), width, style(37 if n == 1 else 29, 40 if n == 1 else 33, INK, True))
        y = H - 80 - th
        title.drawOn(c, left, y)
        intro, ih = paragraph(ESC(p["intro"]), width, style(12.1, 16.4, MUTED))
        y -= ih + 11
        intro.drawOn(c, left, y)
        y -= 16
        ops = flow_blocks(p, width)
        body_height = sum(h + gap for _, _, h, gap in ops)
        src = media(p)
        if src:
            caption, ch = paragraph(ESC(p["caption"]), width, style(8.1, 10.3, MUTED))
            available = y - 57 - body_height - ch - 16
            if p.get("diagram"):
                drawing = svg2rlg(str(DEST / src))
                preferred = min(280 if n == 1 else 195, drawing.height * width / drawing.width)
                visual_height = min(preferred, available)
                if visual_height < 105:
                    raise ValueError(f'Page {n} image would be too small: {visual_height:.1f}pt')
                scale = min(width / drawing.width, visual_height / drawing.height)
                drawing.scale(scale, scale)
                dw, dh = drawing.width * scale, drawing.height * scale
                renderPDF.draw(drawing, c, left + (width - dw) / 2, y - dh)
                y -= dh
            else:
                img = Image.open(DEST / src)
                preferred = 215 if p["id"] == "case-prep" else 92
                visual_height = min(preferred, available)
                if visual_height < 60:
                    raise ValueError(f'Page {n} image space too small: {visual_height:.1f}pt')
                scale = min(width / img.width, visual_height / img.height)
                iw, ih = img.width * scale, img.height * scale
                c.setFillColor(colors.HexColor("#1D1F20"))
                c.roundRect(left, y - ih, width, ih, 9, fill=1, stroke=0)
                c.drawImage(ImageReader(img), left + (width - iw) / 2, y - ih, iw, ih, mask="auto")
                y -= ih
            y -= ch + 5
            caption.drawOn(c, left, y)
            y -= 11
        for kind, obj, height, gap in ops:
            if kind == "flow": obj.drawOn(c, left, y - height)
            elif kind == "callout":
                head, hh, body, bh = obj
                c.setFillColor(colors.HexColor(MINT))
                c.roundRect(left, y - height, width, height, 7, fill=1, stroke=0)
                c.setFillColor(colors.HexColor(TEAL))
                c.rect(left, y - height, 3, height, fill=1, stroke=0)
                head.drawOn(c, left + 14, y - 10 - hh)
                body.drawOn(c, left + 14, y - 13 - hh - bh)
            elif kind == "item":
                paragraph_obj, symbol = obj
                c.setStrokeColor(colors.HexColor(TEAL))
                c.setFillColor(colors.HexColor(TEAL))
                if symbol == "box": c.roundRect(left + 2, y - 11, 9, 9, 1.5, fill=0, stroke=1)
                elif symbol == "dot": c.circle(left + 6, y - 6, 2, fill=1, stroke=0)
                else:
                    c.circle(left + 7, y - 6, 7, fill=1, stroke=0)
                    c.setFillColor(colors.white)
                    c.setFont("KitBold", 8)
                    c.drawCentredString(left + 7, y - 9, symbol)
                paragraph_obj.drawOn(c, left + 23, y - height)
            elif kind == "code":
                lines, size = obj
                c.setFillColor(colors.HexColor("#EAF0EC"))
                c.roundRect(left, y - height, width, height, 6, fill=1, stroke=0)
                c.setFont("KitMono", size)
                c.setFillColor(colors.HexColor(INK))
                for i, line in enumerate(lines): c.drawString(left + 12, y - 15 - i * 12, line)
            y -= height + gap
        if y < 50:
            raise ValueError(f'Page {n} overflows: last content at {y:.1f}pt')
        c.setStrokeColor(colors.HexColor("#CCDAD1"))
        c.line(left, 39, W - left, 39)
        c.setFont("Kit", 8)
        c.setFillColor(colors.HexColor(MUTED))
        c.drawString(left, 25, "ESP32-S3 SUPERMINI  /  UNPLUG BEFORE REWIRING")
        c.setFont("KitBold", 9)
        c.drawRightString(W - left, 25, f"{n:02} / {len(DATA['pages']):02}")
        layout.append({"page": n, "chapter": p["id"], "content_bottom_pt": round(y, 1)})
        c.showPage()
    c.save()
    return layout


def verify_pinmap():
    expected = {"JOY_X": 2, "JOY_Y": 1, "JOY_SW": 3, "ENC_A": 4, "ENC_B": 5, "ENC_SW": 6, "I2S_BCLK": 7, "I2S_WS": 8, "I2S_DOUT": 9, "I2S_DIN": 10, "AMP_SD": 11, "I2C_SDA": 12, "I2C_SCL": 13}
    source = (ROOT / "firmware/main/pins.h").read_text()
    actual = {k: int(v) for k, v in re.findall(r"#define PIN_(\w+)\s+(\d+)", source)}
    for name, pin in expected.items():
        if actual.get(name) != pin:
            raise ValueError(f"Firmware pin changed: {name}. Review guide.json and SVG connections before publishing.")


if __name__ == "__main__":
    verify_pinmap()
    artwork()
    render_html()
    render_markdown()
    layout = render_pdf()
    print(json.dumps({"pdf": str(PDF.relative_to(ROOT)), "pages": layout, "artwork": len(list(ASSETS.glob('*.svg')))}, indent=2))
