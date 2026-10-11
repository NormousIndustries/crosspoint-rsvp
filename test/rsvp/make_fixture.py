"""Generate a small, numbered EPUB for the manual V1A acceptance procedure."""

import argparse
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile


def make_fixture(destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    container = """<?xml version="1.0"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
<rootfiles><rootfile full-path="EPUB/content.opf" media-type="application/oebps-package+xml"/></rootfiles>
</container>"""
    package = """<?xml version="1.0" encoding="UTF-8"?>
<package version="3.0" unique-identifier="book-id" xmlns="http://www.idpf.org/2007/opf">
<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
<dc:identifier id="book-id">crosspoint-rsvp-v1a-test</dc:identifier>
<dc:title>RSVP V1A acceptance</dc:title><dc:creator>CrossPoint test fixture</dc:creator>
<dc:language>en</dc:language><meta property="dcterms:modified">2026-10-11T00:00:00Z</meta>
</metadata><manifest>
<item id="one" href="one.xhtml" media-type="application/xhtml+xml"/>
<item id="empty" href="empty.xhtml" media-type="application/xhtml+xml"/>
<item id="two" href="two.xhtml" media-type="application/xhtml+xml"/>
<item id="nav" href="nav.xhtml" properties="nav" media-type="application/xhtml+xml"/>
</manifest><spine><itemref idref="one"/><itemref idref="empty"/><itemref idref="two"/></spine></package>"""
    nav = """<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
<head><title>Contents</title></head><body><nav epub:type="toc"><ol>
<li><a href="one.xhtml">Chapter one</a></li><li><a href="two.xhtml">Chapter two</a></li>
</ol></nav></body></html>"""

    def chapter(label: str, start: int, end: int) -> str:
        words = [f"T{number:04d}" for number in range(start, end + 1)]
        paragraphs = "".join(f"<p>{' '.join(words[i:i + 20])}</p>" for i in range(0, len(words), 20))
        return (
            '<?xml version="1.0" encoding="UTF-8"?>'
            '<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Test</title></head>'
            f"<body><h1>{label}</h1>{paragraphs}"
            "<p>don't morning, well-known café</p></body></html>"
        )

    empty = ('<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Empty chapter</title></head>'
             '<body><p> \n </p></body></html>')
    with ZipFile(destination, "w") as archive:
        archive.writestr("mimetype", "application/epub+zip", compress_type=ZIP_STORED)
        for name, contents in {
            "META-INF/container.xml": container,
            "EPUB/content.opf": package,
            "EPUB/nav.xhtml": nav,
            "EPUB/one.xhtml": chapter("Chapter-one", 1, 400),
            "EPUB/empty.xhtml": empty,
            "EPUB/two.xhtml": chapter("Chapter-two", 401, 800),
        }.items():
            archive.writestr(name, contents.encode("utf-8"), compress_type=ZIP_DEFLATED)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", nargs="?", type=Path, default=Path(".pio/rsvp-v1a-acceptance.epub"))
    args = parser.parse_args()
    make_fixture(args.destination)
    print(args.destination.resolve())
