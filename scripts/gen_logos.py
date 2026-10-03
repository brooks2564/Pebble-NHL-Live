#!/usr/bin/env python3
"""Fetch ESPN NHL logos and write opaque-black-flattened PNGs for Pebble.

Sizes: emery LG 36 / SM 22, basalt LG 26 / SM 16 (same as the NFL/MLB apps).
Each logo is cropped to its artwork bounding box first (ESPN's canvases have
inconsistent padding), then scaled to fit and centered on a black square.
"""
import io, os, urllib.request
from PIL import Image

TEAMS = ["ANA","BOS","BUF","CAR","CBJ","CGY","CHI","COL","DAL","DET","EDM","FLA",
         "LAK","MIN","MTL","NJD","NSH","NYI","NYR","OTT","PHI","PIT","SEA","SJS",
         "STL","TBL","TOR","UTA","VAN","VGK","WSH","WPG"]
ESPN = {"LAK":"la","NJD":"nj","SJS":"sj","TBL":"tb","UTA":"utah"}
SIZES = {"EM_LG":36, "EM_SM":22, "BA_LG":26, "BA_SM":16}
OUT = os.path.join(os.path.dirname(__file__), "..", "resources", "images", "logos")

def fetch(code):
    for kind in ("500-dark", "500"):
        url = "https://a.espncdn.com/i/teamlogos/nhl/%s/%s.png" % (kind, code)
        try:
            return Image.open(io.BytesIO(urllib.request.urlopen(url, timeout=20).read())).convert("RGBA")
        except Exception:
            pass
    raise SystemExit("no logo for " + code)

os.makedirs(OUT, exist_ok=True)
for abbr in TEAMS:
    src = fetch(ESPN.get(abbr, abbr.lower()))
    bbox = src.getchannel("A").point(lambda a: 255 if a > 16 else 0).getbbox()
    if bbox: src = src.crop(bbox)
    for tag, n in SIZES.items():
        scale = min(n / src.width, n / src.height)
        sz = (max(1, round(src.width * scale)), max(1, round(src.height * scale)))
        img = src.resize(sz, Image.LANCZOS)
        canvas = Image.new("RGBA", (n, n), (0, 0, 0, 255))
        canvas.alpha_composite(img, ((n - sz[0]) // 2, (n - sz[1]) // 2))
        canvas.convert("RGB").save(os.path.join(OUT, "%s_%s.png" % (abbr, tag)))
    print(abbr, end=" ", flush=True)
print()
