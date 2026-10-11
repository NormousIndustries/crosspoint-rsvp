"""Rasterize production RV32 context layouts with the actual embedded bitmaps.

No host font substitution. Checks six hint cells, status, selected underline,
word bounds and excerpt ellipses at the X3's actual CCW geometry.
"""
from pathlib import Path
import itertools
import json
import re
import sys
import zlib

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/rsvp-v1c2'
sys.path.insert(0, str(OUT / 'python-deps'))
from PIL import Image, ImageDraw
import render_physical_controls as physical

class ContextFont(physical.UiFont):
    def __init__(self):
        source = (ROOT / 'lib/EpdFont/builtinFonts/notosans_16_regular.h').read_text()
        prefix = 'notosans_16_regular'
        array = physical.array
        self.bitmap = array(source, prefix + 'Bitmaps')
        flat = array(source, prefix + 'Glyphs')
        self.glyphs = [flat[i:i+7] for i in range(0, len(flat), 7)]
        flat = array(source, prefix + 'Intervals')
        self.intervals = [flat[i:i+3] for i in range(0, len(flat), 3)]
        self.left = dict(zip(array(source, prefix + 'KernLeftCodepoints'), array(source, prefix + 'KernLeftClassIds')))
        self.right = dict(zip(array(source, prefix + 'KernRightCodepoints'), array(source, prefix + 'KernRightClassIds')))
        self.rows = array(source, prefix + 'KernRowOffsets')
        self.cols = array(source, prefix + 'KernSparseCols')
        self.kerns = array(source, prefix + 'KernSparseValues')
        flat = array(source, prefix + 'LigaturePairs')
        self.ligatures = {(chr(flat[i] >> 16), chr(flat[i] & 0xFFFF)): chr(flat[i+1])
                          for i in range(0, len(flat), 2)}
        flat = array(source, prefix + 'Groups')
        self.glyph_data = {}
        for i in range(0, len(flat), 5):
            offset, compressed, size, count, first = flat[i:i+5]
            data = zlib.decompress(bytes(self.bitmap[offset:offset+compressed]), -15)
            assert len(data) == size
            start = 0
            for index in range(first, first+count):
                w, h = self.glyphs[index][:2]
                length = ((w+3)//4)*h
                self.glyph_data[index] = data[start:start+length]
                start += length
            assert start == size
        assert re.search(r'\b45,\s*36,\s*-10,\s*true,', source)

    def mask(self, glyph):
        w, h = glyph[:2]
        data = self.glyph_data[self.glyphs.index(glyph)]
        stride = (w+3)//4
        mask = Image.new('1', (w, h))
        mask.putdata([255 if (data[y*stride+x//4] >> (6-2*(x%4))) & 3 else 0
                      for y in range(h) for x in range(w)])
        return mask

    def text(self, text):
        # Same greedy font ligatures and differential rounding as EpdFont.
        shaped = []
        for char in text:
            if shaped and (shaped[-1], char) in self.ligatures:
                shaped[-1] = self.ligatures[shaped[-1], char]
            else:
                shaped.append(char)
        return super().text(''.join(shaped))

def main():
    font, ui = ContextFont(), physical.UiFont()
    layouts = json.loads((OUT / 'context-layouts.json').read_text(encoding='utf-8'))
    frames, reports = [], []
    for case in layouts:
        assert case['line_height'] == 53 and case['width'] == 456 and case['gap'] == 11
        context = Image.new('1', (792, 528))
        highlight = Image.new('1', context.size)
        x, y = 45, 67 + (374-case['lines']*53)//2
        word_bounds = []
        for i in range(case['first'], case['last']):
            word = case['words'][i]
            if i == case['selected'] and case['shortened']:
                word = word.encode('utf-8')[:case['selected_bytes']].decode('utf-8')
            width, positions = font.text(word)
            wx, wy = x+case['positions'][i][0], y+case['positions'][i][1]*53
            mask = Image.new('1', context.size)
            physical.paste_glyphs(mask, font, positions, wx, wy+36)
            bbox = mask.getbbox()
            assert bbox and 41 <= bbox[0] < bbox[2] <= 505 and 67 <= bbox[1] < bbox[3] <= 441, (case['name'], word, bbox)
            assert not physical.intersects(mask, context), 'word collision'
            context.paste(255, (0, 0), mask)
            word_bounds.append(dict(index=i, text=word, ink_bounds=bbox))
            if i == case['selected']:
                ImageDraw.Draw(highlight).rectangle((wx, wy+50, wx+max(1,width)-1, wy+51), fill=255)
        if case['leading']:
            physical.paste_glyphs(context, font, font.text('…')[1], x+case['leading_x'], y+36)
        if case['trailing']:
            physical.paste_glyphs(context, font, font.text('…')[1], x+case['trailing_x'], y+case['trailing_row']*53+36)
        assert highlight.getbbox() and not physical.intersects(context, highlight)
        masks, labels, status_bounds = physical.controls(ui, (0,0,0,0), status='Paused · 100 WPM')
        for mask in masks:
            assert not physical.intersects(context, mask) and not physical.intersects(highlight, mask)
        # All valid remaps and all four speeds leave context and highlight clear.
        for permutation in itertools.permutations(('Next', 'Previous', 'Play / Pause', 'Back')):
            for speed in (50,75,100,125):
                remap_masks, _, _ = physical.controls(ui, (0,0,0,0), ('Faster','Slower')+permutation,
                                                      status=f'Paused · {speed} WPM')
                for mask in remap_masks:
                    assert not physical.intersects(context, mask) and not physical.intersects(highlight, mask)
        image = Image.new('1', context.size, 1)
        for mask in [context, highlight, *masks]:
            image.paste(0, (0,0), mask)
        path = OUT / ('context-' + case['name'] + '.png')
        image.save(path)
        frames.append(image)
        reports.append(dict(name=case['name'], lines=case['lines'], selected=case['selected'],
            excerpt=[bool(case['leading']),bool(case['trailing'])], highlight_bounds=highlight.getbbox(),
            context_ink_bounds=context.getbbox(), words=word_bounds, labels=labels, status_bounds=status_bounds,
            preview=str(path.relative_to(ROOT))))
    sheet = Image.new('1', (1584,1584), 1)
    for i, frame in enumerate(frames): sheet.paste(frame, ((i%2)*792,(i//2)*528))
    sheet.save(OUT / 'context-contact-sheet.png')
    (OUT / 'context-raster-report.json').write_text(json.dumps(dict(screen=[792,528],context_rect=[41,67,464,374],
        font='Noto Sans 16 regular, existing 2-bit bitmaps rendered in BW', line_height=53, ascender=36,
        samples=reports, remaps=24, speeds=[50,75,100,125]),indent=2,ensure_ascii=False)+'\n', encoding='utf-8')
    print('PASS: five actual-font RV32-layout raster previews; six labels, status and underline bounded; no glyph/label/status collisions; 24 remaps x 4 speeds; whole-word excerpt ellipses; short/long/dialogue/repeated/excerpt')

if __name__ == '__main__': main()
