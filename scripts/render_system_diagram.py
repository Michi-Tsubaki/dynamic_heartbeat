"""Render the bilingual system SVGs using only the Python standard library."""

import base64
from html import escape
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FONT = base64.b64encode(
    (ROOT / "assets/fonts/space-grotesk-medium.woff2").read_bytes()
).decode()

LABELS = {
    "ja": {
        "title": "Dynamic Heartbeat — システム構成図",
        "description": "心拍センサを入力とし、ArduinoがMOSFET経由で空気ポンプと弁1・弁2を制御する。12V ACアダプタが駆動電源を供給し、水冷ポンプは電源へ直結する。コンピュータは心音を再生し、空気が心房と心室を動かし、液体が循環する。",
        "speaker": "スピーカ", "computer": "コンピュータ", "sensor": "心拍センサ", "adapter": "ACアダプタ",
        "heart": "心臓", "air_pump": "空気ポンプ", "atrium": "心房",
        "ventricle": "心室", "valve1": "弁1", "valve2": "弁2", "ambient": "外気",
        "water_pump": "水冷ポンプ", "signal": "制御信号", "air": "空気", "water": "液体",
        "power": "12V電源",
    },
    "en": {
        "title": "Dynamic Heartbeat — System diagram",
        "description": "A heart-rate sensor feeds the Arduino, which controls the air pump and both valves through MOSFETs. A 12V AC adapter supplies actuator power, and the liquid pump connects directly to the power supply. The computer plays heart sounds. Air drives the atrium and ventricle, while liquid circulates.",
        "speaker": "Speaker", "computer": "Computer", "sensor": "Heart-rate sensor", "adapter": "AC adapter",
        "heart": "Artificial heart", "air_pump": "Air pump", "atrium": "Atrium",
        "ventricle": "Ventricle", "valve1": "Valve 1", "valve2": "Valve 2", "ambient": "Atmosphere",
        "water_pump": "Liquid pump", "signal": "Control signal", "air": "Air", "water": "Liquid",
        "power": "12V power",
    },
}


def render(language):
    labels = LABELS[language]
    parts = [f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1200 740" width="1200" height="740" lang="{language}" role="img" aria-labelledby="diagram-title diagram-description">
  <title id="diagram-title">{labels['title']}</title>
  <desc id="diagram-description">{labels['description']}</desc>
  <defs>
    <style>
      @font-face {{ font-family: 'Space Grotesk'; font-weight: 500; src: url(data:font/woff2;base64,{FONT}) format('woff2'); }}
      text {{ fill: #101010; font-family: 'Space Grotesk', 'Noto Sans CJK JP', 'Hiragino Sans', 'Yu Gothic', sans-serif; font-weight: 500; text-anchor: middle; }}
      .label {{ font-size: 21px; letter-spacing: -.5px; }}
      .detail {{ font-size: 12px; letter-spacing: .4px; }}
      .small {{ font-size: 15px; }}
      .box {{ fill: #ff3549; stroke: #101010; stroke-width: 1.4; }}
      .signal {{ fill: none; stroke: #101010; stroke-width: 1.4; stroke-dasharray: 6 6; }}
      .power {{ fill: none; stroke: #101010; stroke-width: 1.4; }}
      .air {{ fill: none; stroke: #101010; stroke-width: 2.7; }}
      .water {{ fill: none; stroke: #101010; stroke-width: 2.7; stroke-dasharray: 12 5 2 5; }}
    </style>
    <marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse"><path d="M1 1 9 5 1 9" fill="none" stroke="#101010" stroke-width="1.4"/></marker>
  </defs>
  <rect width="1200" height="740" fill="#ff3549"/>
  <rect x="265" y="375" width="630" height="195" fill="#ff5364" stroke="#101010" stroke-width="1.1"/>
  <!-- Digital control and audio -->
  <path class="signal" d="M285 112H212" marker-end="url(#arrow)"/>
  <path class="signal" d="M527 112H623" marker-start="url(#arrow)" marker-end="url(#arrow)"/>
  <path class="signal" d="M955 112H867" marker-end="url(#arrow)"/>
  <path class="signal" d="M745 152V218M125 218H830M125 218V266M510 218V266M830 218V266"/>
  <g fill="#101010"><circle cx="125" cy="218" r="3"/><circle cx="510" cy="218" r="3"/><circle cx="745" cy="218" r="3"/><circle cx="830" cy="218" r="3"/></g>
  <!-- The 12V adapter powers all MOSFET stages and the liquid pump directly. -->
  <path class="power" d="M1065 348V360H35V295H58" marker-end="url(#arrow)"/>
  <path class="power" d="M420 360V295H443" marker-end="url(#arrow)"/>
  <path class="power" d="M740 360V295H763" marker-end="url(#arrow)"/>
  <g fill="#101010"><circle cx="420" cy="360" r="3"/><circle cx="740" cy="360" r="3"/><circle cx="1065" cy="360" r="3"/></g>
  <path class="power" d="M125 323V439" marker-end="url(#arrow)"/>
  <path class="power" d="M510 323V454" marker-end="url(#arrow)"/>
  <path class="power" d="M830 323V454" marker-end="url(#arrow)"/>
  <path class="power" d="M1065 360V592" marker-end="url(#arrow)"/>
  <!-- Pneumatic paths; the lower path feeds the ventricle directly -->
  <path class="air" d="M210 472H298" marker-end="url(#arrow)"/>
  <path class="air" d="M440 472H492" marker-end="url(#arrow)"/>
  <path class="air" d="M528 472H608" marker-end="url(#arrow)"/>
  <path class="air" d="M750 472H812" marker-end="url(#arrow)"/>
  <path class="air" d="M848 472H930" marker-end="url(#arrow)"/>
  <path class="air" d="M125 503V548H680V505" marker-end="url(#arrow)"/>
  <!-- Liquid circulation -->
  <path class="water" d="M970 611H810V572" marker-end="url(#arrow)"/>
  <path class="water" d="M590 570V641H968" marker-end="url(#arrow)"/>
''']

    def text(x, y, value, cls="label", anchor="middle"):
        parts.append(f'  <text x="{x}" y="{y}" class="{cls}" style="text-anchor:{anchor}">{escape(value)}</text>\n')

    def box(x, y, width, height, title, subtitle=None):
        parts.append(f'  <rect class="box" x="{x}" y="{y}" width="{width}" height="{height}"/>\n')
        center = x + width / 2
        text(center, y + (34 if subtitle else height / 2 + 7), title)
        if subtitle:
            text(center, y + 57, subtitle, "detail")

    box(40, 72, 170, 80, labels["speaker"])
    box(285, 72, 240, 80, labels["computer"], "heartbeat_sound.py")
    box(625, 72, 240, 80, "Arduino UNO", "heartbeat.ino")
    box(955, 72, 205, 80, labels["sensor"], "MAX30102")
    box(60, 268, 130, 55, "MOSFET")
    for x in (445, 765):
        box(x, 268, 130, 55, "MOSFET")
    box(970, 268, 190, 80, labels["adapter"], "12 V")
    box(40, 441, 170, 62, labels["air_pump"])
    box(300, 441, 140, 62, labels["atrium"])
    box(610, 441, 140, 62, labels["ventricle"])
    box(970, 594, 190, 62, labels["water_pump"])
    text(285, 405, labels["heart"], "small", "start")
    for x, label in ((510, "valve1"), (830, "valve2")):
        parts.append(f'  <path d="M{x-16} 459L{x+16} 485V459L{x-16} 485Z" fill="#ff5364" stroke="#101010" stroke-width="1.6"/>\n')
        text(x, 522, labels[label], "small")
    text(945, 479, labels["ambient"], "small", "start")
    parts.append('  <path d="M40 687H1160" stroke="#101010" stroke-width="1"/>\n')
    for x, style, label in ((40, "signal", "signal"), (330, "power", "power"), (620, "air", "air"), (850, "water", "water")):
        parts.append(f'  <path class="{style}" d="M{x} 716h44"/>\n')
        text(x + 58, 721, labels[label], "small", "start")
    parts.append('</svg>\n')
    name = "system_diagram.svg" if language == "ja" else "system_diagram.en.svg"
    (ROOT / "figs" / name).write_text("".join(parts))


if __name__ == "__main__":
    for language in LABELS:
        render(language)
