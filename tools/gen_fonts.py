"""Render Montserrat subsets into LVGL 9 fmt_txt C sources."""

import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
TTF = ROOT / "tools" / "fonts" / "Montserrat-SemiBold.ttf"
OUT = ROOT / "main" / "fonts"

POLISH = "ąćęłńóśźżĄĆĘŁŃÓŚŹŻ"
ASCII = "".join(chr(c) for c in range(32, 127))
DEGREE = "°"
MINUS = "−"

FONTS = [
    ("font_clock_56", 56, "0123456789:"),
    ("font_clock_72", 72, "0123456789:"),
    ("font_clock_88", 88, "0123456789:"),
    ("font_temp_32", 32, "0123456789 /" + DEGREE + MINUS + "-"),
    ("font_temp_40", 40, "0123456789 /" + DEGREE + MINUS + "-"),
    ("font_temp_48", 48, "0123456789 /" + DEGREE + MINUS + "-"),
    ("font_label_20", 20, ASCII + POLISH + DEGREE + MINUS),
    ("font_label_28", 28, ASCII + POLISH + DEGREE + MINUS),
    ("font_label_36", 36, ASCII + POLISH + DEGREE + MINUS),
]


def cmap_codes(path):
    data = path.read_bytes()
    tables = struct.unpack_from(">H", data, 4)[0]
    cmap_at = None
    for index in range(tables):
        tag, _checksum, offset, _length = struct.unpack_from(">4sIII", data, 12 + index * 16)
        if tag == b"cmap":
            cmap_at = offset
    if cmap_at is None:
        raise SystemExit("Font nie ma tablicy cmap")
    _version, count = struct.unpack_from(">HH", data, cmap_at)
    subtables = []
    for index in range(count):
        platform, encoding, offset = struct.unpack_from(">HHI", data, cmap_at + 4 + index * 8)
        start = cmap_at + offset
        fmt = struct.unpack_from(">H", data, start)[0]
        if fmt in (4, 12) and (platform, encoding) in ((3, 1), (3, 10), (0, 3), (0, 4)):
            subtables.append((fmt, start))
    if not subtables:
        raise SystemExit("Font nie ma cmap Unicode")
    present = set()
    for fmt, start in subtables:
        if fmt == 4:
            _fmt, _length, _language, seg_x2 = struct.unpack_from(">HHHH", data, start)
            segments = seg_x2 // 2
            ends = start + 14
            starts = ends + seg_x2 + 2
            deltas = starts + seg_x2
            offsets = deltas + seg_x2
            for segment in range(segments):
                end = struct.unpack_from(">H", data, ends + segment * 2)[0]
                begin = struct.unpack_from(">H", data, starts + segment * 2)[0]
                delta = struct.unpack_from(">h", data, deltas + segment * 2)[0]
                offset = struct.unpack_from(">H", data, offsets + segment * 2)[0]
                for code in range(begin, end + 1):
                    if offset == 0:
                        glyph = (code + delta) & 0xFFFF
                    else:
                        pos = offsets + segment * 2 + offset + (code - begin) * 2
                        glyph = struct.unpack_from(">H", data, pos)[0]
                        if glyph:
                            glyph = (glyph + delta) & 0xFFFF
                    if glyph:
                        present.add(code)
        else:
            _fmt, _reserved, length, _language, groups = struct.unpack_from(">HHIII", data, start)
            group_at = start + 16
            for _group in range(groups):
                begin, end, glyph = struct.unpack_from(">III", data, group_at)
                group_at += 12
                if glyph:
                    present.update(range(begin, end + 1))
            if group_at - start > length:
                raise SystemExit("Uszkodzona tablica cmap")
    return present


def render(font, size, char, source):
    ascent, descent = font.getmetrics()
    advance = font.getlength(source)
    pad = size + 4
    canvas = Image.new("L", (int(advance) + pad * 2 + 8, ascent + descent + 8), 0)
    draw = ImageDraw.Draw(canvas)
    draw.text((pad, ascent), source, font=font, fill=255, anchor="ls")
    box = canvas.getbbox()
    if box is None:
        return advance, 0, 0, 0, 0, b""
    left, top, right, bottom = box
    width = right - left
    height = bottom - top
    pixels = []
    for y in range(top, bottom):
        for x in range(left, right):
            pixels.append((canvas.getpixel((x, y)) * 15 + 127) // 255)
    packed = bytearray()
    high = True
    current = 0
    for value in pixels:
        if high:
            current = (value & 0xF) << 4
        else:
            packed.append(current | (value & 0xF))
        high = not high
    if not high:
        packed.append(current)
    return advance, left - pad, ascent - bottom, width, height, bytes(packed)


def ranges(codepoints):
    runs = []
    start = codepoints[0]
    prev = start
    for code in codepoints[1:]:
        if code == prev + 1:
            prev = code
            continue
        runs.append((start, prev))
        start = code
        prev = code
    runs.append((start, prev))
    return runs


def emit(name, size, alphabet, font, present):
    ordered = []
    seen = set()
    for char in alphabet:
        code = ord(char)
        if code in seen:
            continue
        seen.add(code)
        source = "-" if code == 0x2212 and code not in present else char
        if ord(source) not in present:
            raise SystemExit(f"{name}: brak glifu U+{ord(source):04X}")
        advance, ofs_x, ofs_y, width, height, bitmap = render(font, size, char, source)
        ordered.append((code, advance, ofs_x, ofs_y, width, height, bitmap))

    bitmap = bytearray()
    glyphs = []
    above = font.getmetrics()[0]
    below = font.getmetrics()[1]
    for code, advance, ofs_x, ofs_y, width, height, bits in ordered:
        adv_w = int(round(advance * 16))
        if not (0 <= adv_w <= 4095 and 0 <= width <= 255 and 0 <= height <= 255):
            raise SystemExit(f"{name}: glif U+{code:04X} nie mieści się w opisie LVGL")
        if not (-128 <= ofs_x <= 127 and -128 <= ofs_y <= 127):
            raise SystemExit(f"{name}: przesunięcie U+{code:04X} poza int8 ({ofs_x}, {ofs_y})")
        glyphs.append((len(bitmap), adv_w, width, height, ofs_x, ofs_y, code))
        bitmap.extend(bits)
        above = max(above, ofs_y + height)
        below = max(below, -ofs_y)
    if len(bitmap) >= (1 << 20):
        raise SystemExit(f"{name}: bitmapa przekracza 1 MB")

    lines = [
        f"/* {name}: Montserrat SemiBold {size}px, bpp 4, bez kerningu */",
        '#include "lvgl.h"',
        "",
        "static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {",
    ]
    for index in range(0, len(bitmap), 16):
        chunk = ", ".join(f"0x{byte:02x}" for byte in bitmap[index:index + 16])
        lines.append(f"    {chunk},")
    if not bitmap:
        lines.append("    0x00,")
    lines.append("};")
    lines.append("")
    lines.append("static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {")
    lines.append("    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},")
    for index, adv_w, width, height, ofs_x, ofs_y, code in glyphs:
        lines.append(
            "    {.bitmap_index = %d, .adv_w = %d, .box_w = %d, .box_h = %d, .ofs_x = %d, .ofs_y = %d}, /* U+%04X */"
            % (index, adv_w, width, height, ofs_x, ofs_y, code)
        )
    lines.append("};")
    lines.append("")
    lines.append("static const lv_font_fmt_txt_cmap_t cmaps[] = {")
    glyph_id = 1
    for start, end in ranges([item[0] for item in ordered]):
        length = end - start + 1
        lines.append(
            "    {.range_start = %d, .range_length = %d, .glyph_id_start = %d,"
            % (start, length, glyph_id)
        )
        lines.append(
            "     .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY},"
        )
        glyph_id += length
    lines.append("};")
    lines.append("")
    lines.append("static const lv_font_fmt_txt_dsc_t font_dsc = {")
    lines.append("    .glyph_bitmap = glyph_bitmap,")
    lines.append("    .glyph_dsc = glyph_dsc,")
    lines.append("    .cmaps = cmaps,")
    lines.append("    .kern_dsc = NULL,")
    lines.append("    .kern_scale = 0,")
    lines.append(f"    .cmap_num = {len(ranges([item[0] for item in ordered]))},")
    lines.append("    .bpp = 4,")
    lines.append("    .kern_classes = 0,")
    lines.append("    .bitmap_format = LV_FONT_FMT_TXT_PLAIN,")
    lines.append("    .stride = 0,")
    lines.append("};")
    lines.append("")
    lines.append(f"const lv_font_t {name} = {{")
    lines.append("    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,")
    lines.append("    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,")
    lines.append(f"    .line_height = {above + below},")
    lines.append(f"    .base_line = {below},")
    lines.append("    .subpx = LV_FONT_SUBPX_NONE,")
    lines.append(f"    .underline_position = {-max(1, size // 12)},")
    lines.append(f"    .underline_thickness = {max(1, size // 16)},")
    lines.append("    .dsc = &font_dsc,")
    lines.append("    .fallback = NULL,")
    lines.append("};")
    lines.append("")
    path = OUT / f"{name}.c"
    path.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    print(f"{name}: {len(ordered)} glyphs, {len(bitmap)} bytes, line {above + below}")


def main():
    if not TTF.is_file():
        raise SystemExit(f"Brak fontu {TTF}")
    OUT.mkdir(parents=True, exist_ok=True)
    present = cmap_codes(TTF)
    for name, size, alphabet in FONTS:
        emit(name, size, alphabet, ImageFont.truetype(str(TTF), size), present)


if __name__ == "__main__":
    sys.exit(main())
