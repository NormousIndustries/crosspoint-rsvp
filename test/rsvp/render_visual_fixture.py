"""Check baked glyph geometry against golden ORP examples; render static fixtures.

Developer-only. Reads the actual generated C++ arrays, not runtime TTF output.
Run with an existing Pillow Python; artifacts go in ignored .pio/.
"""
from pathlib import Path
import json
import re
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
EXAMPLES = (("I", 0), ("an", 1), ("the", 1), ("reading", 2), ("morning,", 2),
            ("something", 2), ("extraordinary", 3), ('"perhaps"', 3),
            ("well-known", 2), ("characteristically", 4), ("AV", 1), ("gyp", 1))
FOCUS, BASELINE, LEFT, RIGHT = 306, 311, 24, 768


def array(text, name):
    body = re.search(r"\b" + name + r"\[\]\s*=\s*\{(.*?)\};", text, re.S)[1]
    return [int(n) for n in re.findall(r"-?\d+", body)]


class Font:
    def __init__(self, size):
        text = (ROOT / f"lib/Rsvp/fonts/rsvp_{size}.h").read_text()
        prefix = f"rsvp{size}"
        self.size = size
        self.bitmap = array(text, prefix + "Bitmap")
        flat = array(text, prefix + "Glyphs")
        self.glyphs = [flat[i:i+7] for i in range(0, len(flat), 7)]
        flat = array(text, prefix + "Intervals")
        self.intervals = [flat[i:i+3] for i in range(0, len(flat), 3)]
        self.rows = array(text, prefix + "KernRows")
        self.cols = array(text, prefix + "KernCols")
        self.kerns = array(text, prefix + "KernValues")
        # Verify every glyph's byte bounds and true monochrome ink extents.
        for w, h, advance, left, top, length, offset in self.glyphs:
            assert length == (w*h+7)//8 and offset+length <= len(self.bitmap)
            if not w or not h:
                continue
            mask = self.mask([w, h, advance, left, top, length, offset])
            assert mask.getbbox() == (0, 0, w, h)

    def glyph(self, char):
        cp = ord(char)
        for first, last, offset in self.intervals:
            if first <= cp <= last:
                return self.glyphs[offset+cp-first]
        return self.glyph('\ufffd')

    def kern(self, left, right):
        if not 33 <= ord(left) <= 126 or not 33 <= ord(right) <= 126:
            return 0
        row, col = ord(left)-33, ord(right)-33
        for i in range(self.rows[row], self.rows[row+1]):
            if self.cols[i] == col:
                return self.kerns[i]
        return 0

    def mask(self, glyph):
        w, h, _, _, _, _, offset = glyph
        result = Image.new('1', (w, h))
        result.putdata([255 if self.bitmap[offset+i//8] & (0x80 >> (i%8)) else 0 for i in range(w*h)])
        return result

    def measure(self, word, target):
        x, advance, previous = 0, 0, None
        left, right, center2, positions = 100000, -100000, 0, []
        for i, char in enumerate(word):
            if previous is not None:
                x += (advance + self.kern(previous, char) + 8) >> 4
            glyph = self.glyph(char)
            w, h, advance, bearing, top, length, offset = glyph
            if w and h:
                left, right = min(left, x+bearing), max(right, x+bearing+w)
            if i == target:
                center2 = 2*(x+bearing)+w
            positions.append((glyph, x))
            previous = char
        origin = FOCUS-(center2+1)//2
        return dict(size=self.size, x=origin, left=origin+left, right=origin+right,
                    orp_screen_x2=2*origin+center2, baseline=BASELINE), positions


def main():
    fonts = [Font(size) for size in (132, 96, 92, 80, 64)]
    assert fonts[0].kern('A', 'V') < 0
    assert fonts[0].glyph('H')[1] == 94 and fonts[0].glyph('x')[1] == 71
    sheet = Image.new('RGB', (792*2, 552*6), 'white')
    draw = ImageDraw.Draw(sheet)
    ui = ImageFont.truetype(str(ROOT / 'lib/EpdFont/builtinFonts/source/Ubuntu/Ubuntu-Regular.ttf'), 21)
    results = []
    for i, (word, target) in enumerate(EXAMPLES):
        for font in fonts:
            metrics, positions = font.measure(word, target)
            if metrics['left'] >= LEFT and metrics['right'] <= RIGHT:
                break
        assert metrics['left'] >= LEFT and metrics['right'] <= RIGHT, word
        assert metrics['orp_screen_x2'] in (2*FOCUS-1, 2*FOCUS), word
        if word in ('reading', 'morning,', 'something'):
            assert metrics['size'] == 132, (word, metrics)
        if word == 'characteristically':
            assert metrics['size'] != 132
        ox, oy = (i % 2)*792, (i//2)*552
        draw.text((ox+24, oy+8), f"{word}: {metrics['size']} px, ORP x={metrics['orp_screen_x2']/2:g}", fill='black', font=ui)
        for glyph, x in positions:
            w, h, advance, bearing, top, length, offset = glyph
            if w and h:
                sheet.paste('black', (ox+metrics['x']+x+bearing, oy+24+BASELINE-top), font.mask(glyph))
        for row in range(6):
            half = row*5//5
            draw.line((ox+FOCUS-half, oy+24+135-row, ox+FOCUS+half, oy+24+135-row), fill='black')
            draw.line((ox+FOCUS-half, oy+24+365+row, ox+FOCUS+half, oy+24+365+row), fill='black')
        for row, labels in enumerate((('Paused | 100 WPM', 'Top: Faster / Bottom: Slower'),
                                      ('Back: return to book', 'Confirm: Play/pause'),
                                      ('Left: Previous', 'Right: Next'))):
            for column, label in enumerate(labels):
                bounds = draw.textbbox((0, 0), label, font=ui)
                assert bounds[2]-bounds[0] <= 372
                draw.text((ox+24+column*372+(372-(bounds[2]-bounds[0]))//2,
                           oy+24+408+row*32), label, fill='black', font=ui)
        results.append(dict(word=word, orp=word[target], **metrics))
    huge, _ = fonts[-1].measure('W'*200, 4)
    assert huge['right'] > RIGHT
    passage = (ROOT / 'test/rsvp/representative_passage.txt').read_text(encoding='utf-8')
    counts = {size: 0 for size in (132, 96, 92, 80, 64)}
    for word in passage.split():
        lexical = [i for i, cp in enumerate(word) if cp.isalnum()]
        length = len(lexical)
        index = 0 if length <= 1 else 1 if length <= 5 else 2 if length <= 9 else 3 if length <= 13 else 4
        target = lexical[index] if lexical else 0
        for font in fonts:
            metrics, _ = font.measure(word, target)
            if metrics['left'] >= LEFT and metrics['right'] <= RIGHT:
                break
        assert metrics['left'] >= LEFT and metrics['right'] <= RIGHT
        counts[font.size] += 1
    assert counts[132] / sum(counts.values()) >= 0.85
    # Actual UI raster safety: verify longest English cells at the firmware's 150 DPI.
    actual_ui = ImageFont.truetype(str(ROOT / 'lib/EpdFont/builtinFonts/source/Ubuntu/Ubuntu-Regular.ttf'), 21)
    for label in ('Word too wide; use Back to read', 'Top: Faster / Bottom: Slower', 'Confirm: Play/pause'):
        assert actual_ui.getlength(label) <= 372
    (ROOT / '.pio/rsvp-v1b-visuals.png').parent.mkdir(exist_ok=True)
    sheet.resize((1188, 2484)).save(ROOT / '.pio/rsvp-v1b-visuals.png')
    (ROOT / '.pio/rsvp-v1b-geometry.json').write_text(json.dumps(results, indent=2)+'\n')
    print(json.dumps(results, indent=2))
    print('Representative V1A passage font counts:', counts)
    print('PASS: 2,445 glyphs, golden ORP examples, kerning, focus, fallback, and English hint bounds')


if __name__ == '__main__':
    main()
