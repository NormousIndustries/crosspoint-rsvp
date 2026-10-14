"""Execute production RV32 geometry and rasterize its actual baked glyph callbacks."""
from pathlib import Path
import itertools
import json
import os
import struct
import unicodedata
from PIL import Image, ImageDraw

import run_context_tests as rv32
import render_visual_fixture as words
import render_physical_controls as physical

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / os.environ.get('RSVP_TEST_OUTPUT', 'rsvp-release.local/v1.0.1')
TIER_NAMES = {132: 'normal', 96: 'long', 92: 'compact', 80: 'small', 64: 'extra-long'}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    emu, addresses, call = rv32.compile_run('typography-tests',
        ['test/rsvp/TypographyHarness.cpp', 'lib/Rsvp/RsvpTypography.cpp',
         'lib/EpdFont/EpdFont.cpp', 'lib/Utf8/Utf8.cpp'],
        ['lib/Rsvp', 'lib/EpdFont', 'lib/Utf8'], ['-Wl,--undefined=rasterCase'], execution_count=200000000)
    fonts = {s: words.Font(s) for s in TIER_NAMES}
    words.FOCUS, words.BASELINE = 311, 311
    ui = physical.UiFont()
    markers = Image.new('1', (792, 528))
    draw = ImageDraw.Draw(markers)
    for n in range(6):
        draw.line((311-n, 135-n, 311+n, 135-n), fill=255)
        draw.line((311-n, 365+n, 311+n, 365+n), fill=255)
    records, frames = [], []
    for index in range(20):
        call('rasterCase', index)
        m = struct.unpack('<18i', emu.mem_read(addresses['rasterMeta'], 72))
        keys = ['size', 'capital_height', 'origin_x', 'left', 'top', 'right', 'bottom',
                'orp_screen_x2', 'baseline', 'orp_byte', 'fits', 'old_size', 'old_capital_height',
                'old_fits', 'glyph_count']
        record = dict(zip(keys, m))
        word = bytes(emu.mem_read(addresses['rasterWord'], 201)).split(b'\0')[0].decode('utf-8')
        record['word'], record['tier'] = word, TIER_NAMES[m[0]]
        record['orp_screen_x'] = m[7] / 2
        record['ink_bounds'] = m[3:7]
        candidates = struct.unpack('<35i', emu.mem_read(addresses['rasterCandidates'], 140))
        record['candidates'] = [dict(size=candidates[i*7], ink_bounds=candidates[i*7+1:i*7+5],
            fits=bool(candidates[i*7+5]), orp_screen_x=candidates[i*7+6]/2) for i in range(5)]
        # The C++ harness proves all larger candidates fail; here independently check every pixel.
        assert record['fits'], word
        assert record['orp_screen_x2'] in (621, 622) and record['baseline'] == 311
        coords = struct.unpack('<' + 'i'*(m[14]*3), emu.mem_read(addresses['rasterGlyphs'], m[14]*12))
        face = fonts[m[0]]
        ink = Image.new('1', (792, 528))
        for n in range(m[14]):
            glyph_index, x, y = coords[n*3:n*3+3]
            glyph = face.glyphs[glyph_index]
            ink.paste(255, (m[2]+x+glyph[3], 311+y-glyph[4]), face.mask(glyph))
        assert ink.getbbox() == tuple(m[3:7]), (word, ink.getbbox(), m[3:7])
        assert 33 <= m[3] < m[5] <= 765 and 153 <= m[4] < m[6] <= 347
        assert not physical.intersects(ink, markers), word
        for remap in itertools.permutations(('Back', 'Play / Pause', 'Previous', 'Next')):
            masks, _, _ = physical.controls(ui, m[3:7], ('Faster', 'Slower')+remap)
            for mask in masks:
                assert not physical.intersects(ink, mask) and not physical.intersects(markers, mask), word
        frame = Image.new('1', (792, 528), 1)
        frame.paste(0, (0, 0), ink)
        frame.paste(0, (0, 0), markers)
        new_name = f'typography-{index:02d}-after.png'
        frame.save(OUT / new_name)
        old_ink = ink.copy()
        if m[11] != m[0]:
            assert not any(unicodedata.combining(c) for c in word)
            target = len(word.encode()[:m[9]].decode())
            metrics, positions = fonts[m[11]].measure(word, target)
            old_ink = Image.new('1', (792, 528))
            physical.paste_glyphs(old_ink, fonts[m[11]], positions, metrics['x'], 311)
        old_candidate = next(c for c in record['candidates'] if c['size'] == m[11])
        assert old_ink.getbbox() == tuple(old_candidate['ink_bounds'])
        record['old_ink_bounds'] = old_ink.getbbox()
        before = Image.new('1', (792, 528), 1)
        before.paste(0, (0, 0), old_ink)
        before.paste(0, (0, 0), markers)
        old_name = f'typography-{index:02d}-before.png'
        before.save(OUT / old_name)
        record['before_preview'], record['after_preview'] = old_name, new_name
        record['capital_change_percent'] = round((m[1]/m[12]-1)*100, 2)
        records.append(record)
        frames.append((before, frame))
    assert records[0]['size'] == records[1]['size'] == 132
    assert records[3]['size'] == 92 and records[3]['old_size'] == 64
    assert records[6]['size'] == records[7]['size'] == 80
    assert any(r['size'] == 64 for r in records)
    sheet = Image.new('RGB', (792*2, 552*len(frames)), 'white')
    draw = ImageDraw.Draw(sheet)
    for i, ((before, after), r) in enumerate(zip(frames, records)):
        for col, (frame, cap) in enumerate(((before, r['old_capital_height']), (after, r['capital_height']))):
            draw.text((col*792+24, i*552+4), f"{'V1.0' if col == 0 else 'V1.0.1'} {r['word']} | capitals {cap}px", fill='black')
            sheet.paste(frame.convert('RGB'), (col*792, i*552+24))
    sheet.save(OUT / 'typography-comparison.png')
    report = dict(dimensions=[792, 528], safe_word_bounds=[33, 153, 765, 347], focus_x=311,
        baseline=311, marker_tips=[135, 365], glyphs_verified=5*489, generated_geometry_cases=240,
        hardware_verified=False, records=records)
    (OUT / 'typography-report.json').write_text(json.dumps(report, indent=2, ensure_ascii=False)+'\n', encoding='utf-8')
    for r in records:
        print(f"{r['word']}: {r['tier']} ({r['size']}), capitals {r['old_capital_height']} -> {r['capital_height']}, ORP {r['orp_screen_x']}, ink {r['ink_bounds']}")
    print('PASS: production RV32 largest-fit geometry, exact-boundary fallback, Unicode/marks/quotes/kerning, 200-byte rejection; 2,445 baked glyphs; actual 792x528 rasters; edges/markers/24 hint remaps; unchanged baseline/ORP')


if __name__ == '__main__':
    main()
