"""Create a developer-only EPUB for V1B typography and V1A regression checks."""
from pathlib import Path
from zipfile import ZipFile, ZIP_STORED, ZIP_DEFLATED
from html import escape
from make_fixture import make_fixture

DESTINATION = Path('.pio/rsvp-v1b-acceptance.epub')
WORDS = ('I an the reading morning, something extraordinary "perhaps" well-known '
         "characteristically Hello, don't (perhaps) word. café e\u0301lan AV gyp")

if __name__ == '__main__':
    make_fixture(DESTINATION)
    with ZipFile(DESTINATION) as archive:
        entries = {name: archive.read(name) for name in archive.namelist()}
    for name in ('EPUB/content.opf',):
        entries[name] = entries[name].replace(b'rsvp-v1a-test', b'rsvp-v1b-test').replace(b'V1A acceptance', b'V1B acceptance')
    chapter = entries['EPUB/one.xhtml'].decode('utf-8')
    chapter = chapter.replace('<h1>Chapter-one</h1>', '<p>'+escape(WORDS)+'</p><h1>Chapter-one</h1>', 1)
    entries['EPUB/one.xhtml'] = chapter.encode('utf-8')
    # Terminal overflow is separate from the ordinary playback/chapter-crossing run.
    chapter = entries['EPUB/two.xhtml'].decode('utf-8')
    chapter = chapter.replace('</body>', '<p>'+('W'*200)+'</p></body>')
    entries['EPUB/two.xhtml'] = chapter.encode('utf-8')
    with ZipFile(DESTINATION, 'w') as archive:
        for name, data in entries.items():
            archive.writestr(name, data, compress_type=ZIP_STORED if name == 'mimetype' else ZIP_DEFLATED)
    print(DESTINATION.resolve())
