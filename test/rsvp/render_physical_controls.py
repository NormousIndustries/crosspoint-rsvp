"""V1B.1: actual embedded word/UI bitmap preview and collision validation.

Uses the X3 profile's CCW bezel insets, including the observed V1B focus=311.
No host font substitution. Run with the existing Pillow Python.
"""
from pathlib import Path
import itertools
import json
import re
import sys
from PIL import Image, ImageDraw
sys.path.insert(0, str(Path(__file__).resolve().parent))
import render_visual_fixture as words

ROOT = Path(__file__).resolve().parents[2]


def array(text, name):
    body = re.search(r'\b' + name + r'\[.*?\]\s*=\s*\{(.*?)\};', text, re.S)[1]
    body = re.sub(r'//[^\n]*', '', body)
    return [int(n, 0) for n in re.findall(r'-?0x[\da-fA-F]+|-?\d+', body)]


class UiFont(words.Font):
    def __init__(self):
        text = (ROOT / 'lib/EpdFont/builtinFonts/ubuntu_10_regular.h').read_text()
        prefix = 'ubuntu_10_regular'
        self.bitmap = array(text, prefix + 'Bitmaps')
        flat = array(text, prefix + 'Glyphs')
        self.glyphs = [flat[i:i+7] for i in range(0, len(flat), 7)]
        flat = array(text, prefix + 'Intervals')
        self.intervals = [flat[i:i+3] for i in range(0, len(flat), 3)]
        self.left = dict(zip(array(text, prefix + 'KernLeftCodepoints'),
                             array(text, prefix + 'KernLeftClassIds')))
        self.right = dict(zip(array(text, prefix + 'KernRightCodepoints'),
                              array(text, prefix + 'KernRightClassIds')))
        self.rows = array(text, prefix + 'KernRowOffsets')
        self.cols = array(text, prefix + 'KernSparseCols')
        self.kerns = array(text, prefix + 'KernSparseValues')
        assert re.search(r'\b70,\s*24,\s*20,\s*-4,\s*false,', text)

    def kern(self, left, right):
        lc, rc = self.left.get(ord(left), 0), self.right.get(ord(right), 0)
        if lc and rc:
            for i in range(self.rows[lc-1], self.rows[lc]):
                if self.cols[i] == rc-1:
                    return self.kerns[i]
        return 0

    def text(self, text):
        x, advance, previous, positions = 0, 0, None, []
        min_x, max_x = 0, 0
        for char in text:
            if previous is not None:
                x += (advance + self.kern(previous, char) + 8) >> 4
            glyph = self.glyph(char)
            w, h, advance, left, top, length, offset = glyph
            min_x, max_x = min(min_x, x+left), max(max_x, x+left+w)
            positions.append((glyph, x))
            previous = char
        return max_x-min_x, positions


def paste_glyphs(mask, font, positions, x, baseline):
    for glyph, cursor in positions:
        w, h, _, left, top, _, _ = glyph
        if w and h:
            mask.paste(255, (x+cursor+left, baseline-top), font.mask(glyph))


def intersects(a, b):
    return Image.composite(a, Image.new('1', a.size), b).getbbox() is not None


def controls(ui, ink_bounds, labels=('Faster', 'Slower', 'Next', 'Previous', 'Play / Pause', 'Back')):
    anchors = [3+522*(i+1)//5 for i in range(4)]
    ys = [y-12 for y in anchors]
    for i in (1, 2):
        width, _ = ui.text(labels[i+2])
        x0, y0, x1, y1 = ink_bounds
        if x0 < 765 and x1 > 765-width and y0 < ys[i]+24 and y1 > ys[i]:
            ys[i] = 121 if i == 1 else 355
    cells = [(82, 27, 244, 24), (82, 477, 244, 24)]
    cells += [(521, y, 244, 24) for y in ys]
    label_metrics, masks = [], []
    for i, (label, cell) in enumerate(zip(labels, cells)):
        width, positions = ui.text(label)
        x, y, w, h = cell
        assert width <= w
        origin = x+(w-width)//2 if i < 2 else x+w-width
        mask = Image.new('1', (792, 528))
        paste_glyphs(mask, ui, positions, origin, y+20)
        bbox = mask.getbbox()
        assert bbox and x <= bbox[0] < bbox[2] <= x+w and y <= bbox[1] < bbox[3] <= y+h
        label_metrics.append(dict(label=label, origin=[origin, y], width=width, ink_bounds=bbox))
        masks.append(mask)
    width, positions = ui.text('Paused | 100 WPM')
    status_mask = Image.new('1', (792, 528))
    paste_glyphs(status_mask, ui, positions, 33+(732-width)//2, 473)
    masks.append(status_mask)
    leaders = Image.new('1', (792, 528))
    draw = ImageDraw.Draw(leaders)
    draw.line((204, 9, 204, 23), fill=255)
    draw.line((204, 505, 204, 519), fill=255)
    for y, anchor in zip(ys, anchors):
        center = y+12
        draw.line((769, center, 773, center), fill=255)
        draw.line((773, center, 773, anchor), fill=255)
        draw.line((773, anchor, 783, anchor), fill=255)
        draw.line((783, anchor-3, 783, anchor+3), fill=255)
    masks.append(leaders)
    for a, b in itertools.combinations(masks, 2):
        assert not intersects(a, b), 'control/status collision'
    return masks, label_metrics, status_mask.getbbox()


def main():
    ui_source = (ROOT / 'src/components/RsvpReaderUi.cpp').read_text()
    activity = (ROOT / 'src/activities/reader/RsvpReaderActivity.cpp').read_text()
    mapping = (ROOT / 'src/MappedInputManager.cpp').read_text()
    # Link the host fixture to the firmware's anchor, remap, and visibility policy.
    for fragment in ('edgeHeight * (i + 1) / 5', 'layout.word.y - layout.rowHeight - 8',
                     'layout.word.y + layout.word.height + 8',
                     'labels.btn4, labels.btn3, labels.btn2, labels.btn1',
                     'layout.safe.width / 3', 'textRight + 8'):
        assert fragment in ui_source, fragment
    assert 'if (!paused && !oversized) return false;' in activity
    assert 'oversized, mappedInput,' in activity
    for button, event in [('Confirm', 'TOGGLE'), ('Left', 'PREVIOUS'), ('Right', 'NEXT'),
                          ('Down', 'FASTER'), ('Up', 'SLOWER'), ('Back', 'EXIT')]:
        assert f'wasReleased(Button::{button})) incoming |= {event}' in activity
    assert 'mapFrontLabels(tr(STR_BACK), tr(STR_RSVP_PLAY_PAUSE)' in ui_source
    for role in ('Back', 'Confirm', 'Left', 'Right'):
        assert f'(gpio.*fn)(SETTINGS.frontButton{role})' in mapping
    assert 'Side buttons remain fixed for Up/Down' in mapping
    settings = (ROOT / 'src/CrossPointSettings.h').read_text()
    for role in ('Back', 'Confirm', 'Left', 'Right'):
        assert f'frontButton{role} = FRONT_HW_{role.upper()}' in settings
    # All valid front-role permutations: every release has exactly its matching hint.
    actions = ('Back', 'Play / Pause', 'Previous', 'Next')
    for permutation in itertools.permutations(range(4)):
        labels = [actions[permutation.index(hw)] for hw in (3, 2, 1, 0)]
        for index, hw in enumerate((3, 2, 1, 0)):
            assert labels[index] == actions[permutation.index(hw)]

    # SDK defaults rotated CCW: native top/right/bottom/left=9/3/3/3.
    board = (ROOT / 'freeink-sdk/libs/hardware/BoardConfig/include/BoardConfig.h').read_text()
    insets = re.search(r'struct ViewableInsets \{(.*?)\};', board, re.S)[1]
    assert re.findall(r'= (\d+);', insets) == ['9', '3', '3', '3']
    top, right, bottom, left = 3, 3, 3, 9
    safe = (33, 27, 732, 474)
    focus, baseline = 311, 311
    words.FOCUS, words.LEFT, words.RIGHT = focus, 33, 765
    fonts, ui = [words.Font(size) for size in (132, 96, 64)], UiFont()
    edge_x, edge_height = 783, 522
    speed_x, speed_width, row = 204, 244, 24
    anchors = [top + edge_height*(i+1)//5 for i in range(4)]
    marker_mask = Image.new('1', (792, 528))
    draw = ImageDraw.Draw(marker_mask)
    for n in range(6):
        draw.line((focus-n, 135-n, focus+n, 135-n), fill=255)
        draw.line((focus-n, 365+n, focus+n, 365+n), fill=255)
    results, frames = [], []
    samples = [('I', 0), ('reading', 2), ('extraordinary', 3), ('characteristically', 4),
               ('something', 2), ('gyp', 1), ('W'*200, 4)]
    for word, target in samples:
        for font in fonts:
            metrics, positions = font.measure(word, target)
            if metrics['left'] >= 33 and metrics['right'] <= 765:
                break
        oversized = metrics['left'] < 33 or metrics['right'] > 765
        assert metrics['orp_screen_x2'] in (621, 622)
        assert metrics['baseline'] == baseline
        ink = Image.new('1', (792, 528))
        paste_glyphs(ink, font, positions, metrics['x'], baseline)
        clip = Image.new('1', (792, 528))
        ImageDraw.Draw(clip).rectangle((33, 153, 764, 346), fill=255)
        ink = Image.composite(ink, Image.new('1', ink.size), clip)
        bounds = (metrics['left'], min(baseline-font.glyph(c)[4] for c in word),
                  metrics['right'], max(baseline-font.glyph(c)[4]+font.glyph(c)[1] for c in word))
        masks, label_metrics, status_bounds = controls(ui, bounds)
        for permutation in itertools.permutations(actions):
            remap_masks, _, _ = controls(ui, bounds, ('Faster', 'Slower')+permutation)
            for mask in remap_masks:
                assert not intersects(ink, mask) and not intersects(marker_mask, mask), (word, permutation)
        for mask in masks:
            assert not intersects(ink, mask) and not intersects(marker_mask, mask), word
        assert not intersects(ink, marker_mask)
        base = Image.new('1', (792, 528), 1)
        base.paste(0, (0, 0), ink)
        base.paste(0, (0, 0), marker_mask)
        paused = base.copy()
        for mask in masks:
            paused.paste(0, (0, 0), mask)
        if oversized:
            # Existing terminal overflow pauses and substitutes the status, not the word.
            paused.paste(1, (33, 453, 765, 477))
            overflow = Image.new('1', (792, 528))
            width, positions = ui.text('Word too wide; use Back to read')
            paste_glyphs(overflow, ui, positions, 33+(732-width)//2, 473)
            paused.paste(0, (0, 0), overflow)
        assert Image.composite(paused, base, ink).tobytes() == base.tobytes()
        assert Image.composite(paused, base, marker_mask).tobytes() == base.tobytes()
        assert base.crop((0, 0, 792, 129)).getextrema()[0] > 0
        filename = '.pio/rsvp-v1b1-' + ('oversized' if oversized else word) + '-paused.png'
        paused.save(ROOT / filename)
        if word == 'reading':
            base.save(ROOT / '.pio/rsvp-v1b1-playing.png')
        frames.append(paused)
        results.append(dict(word=word[:30], oversized=oversized, ink_bounds=ink.getbbox(), labels=label_metrics, **metrics))
    # Full-resolution contact sheet: first four samples are short/medium/long/extra-long.
    sheet = Image.new('1', (1584, 1056), 1)
    for i, frame in enumerate(frames[:4]):
        sheet.paste(frame, ((i%2)*792, (i//2)*528))
    sheet.save(ROOT / '.pio/rsvp-v1b1-visuals.png')
    report = dict(dimensions=[792, 528], safe=safe, focus=focus, baseline=baseline,
                  right_edge_anchors=[[edge_x, y] for y in anchors], speed_x=speed_x,
                  labels=results[1]['labels'], status_bounds=status_bounds, samples=results,
                  mapping_permutations=24, hardware_verified=False)
    (ROOT / '.pio/rsvp-v1b1-geometry.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    print('PASS: actual UI/word bitmaps; all six labels; no collisions; fixed ORP/baseline/markers; 24 remaps; playing chrome absent')


if __name__ == '__main__':
    main()
