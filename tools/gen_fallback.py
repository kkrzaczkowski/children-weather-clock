import pathlib
import struct
import zlib

W, H = 266, 480


def chunk(tag: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def write_png(path: pathlib.Path, rgba: bytearray) -> None:
    raw = b"".join(b"\x00" + bytes(rgba[y * W * 4 : (y + 1) * W * 4]) for y in range(H))
    ihdr = struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    path.write_bytes(png)


def put(px: bytearray, x: int, y: int, color: tuple[int, int, int, int]) -> None:
    if 0 <= x < W and 0 <= y < H:
        i = (y * W + x) * 4
        px[i : i + 4] = bytes(color)


def fill_ellipse(px, cx, cy, rx, ry, color) -> None:
    for y in range(cy - ry, cy + ry + 1):
        for x in range(cx - rx, cx + rx + 1):
            if rx > 0 and ry > 0 and ((x - cx) * (x - cx)) / (rx * rx) + ((y - cy) * (y - cy)) / (ry * ry) <= 1:
                put(px, x, y, color)


def fill_rect(px, x0, y0, x1, y1, color) -> None:
    for y in range(y0, y1):
        for x in range(x0, x1):
            put(px, x, y, color)


def figure(colors: dict) -> bytearray:
    px = bytearray(W * H * 4)
    skin = (242, 208, 176, 255)
    fill_ellipse(px, 133, 92, 38, 44, skin)
    fill_ellipse(px, 108, 96, 10, 14, skin)
    fill_ellipse(px, 158, 96, 10, 14, skin)
    fill_rect(px, 116, 130, 150, 160, skin)
    fill_ellipse(px, 133, 250, 70, 100, colors["body"])
    fill_rect(px, 78, 210, 188, 250, colors["body"])
    fill_rect(px, 70, 200, 100, 330, colors["arm"])
    fill_rect(px, 166, 200, 196, 330, colors["arm"])
    fill_rect(px, 100, 330, 128, 450, colors["leg"])
    fill_rect(px, 138, 330, 166, 450, colors["leg"])
    if colors.get("hat"):
        fill_ellipse(px, 133, 58, 46, 22, colors["hat"])
        fill_rect(px, 90, 58, 176, 78, colors["hat"])
    if colors.get("scarf"):
        fill_rect(px, 100, 148, 166, 172, colors["scarf"])
    return px


BANDS = [
    {"body": (28, 74, 158, 255), "arm": (20, 54, 122, 255), "leg": (18, 40, 90, 255), "hat": (230, 236, 245, 255), "scarf": (186, 214, 246, 255)},
    {"body": (46, 116, 196, 255), "arm": (36, 92, 168, 255), "leg": (32, 70, 130, 255), "hat": (210, 224, 240, 255), "scarf": (255, 176, 64, 255)},
    {"body": (64, 150, 96, 255), "arm": (48, 122, 78, 255), "leg": (48, 78, 120, 255)},
    {"body": (232, 144, 48, 255), "arm": (214, 120, 36, 255), "leg": (70, 120, 186, 255)},
    {"body": (244, 206, 64, 255), "arm": (242, 208, 176, 255), "leg": (86, 170, 214, 255)},
]


def main() -> None:
    out = pathlib.Path(__file__).resolve().parents[1] / "main" / "assets" / "fallback"
    out.mkdir(parents=True, exist_ok=True)
    for index, colors in enumerate(BANDS, start=1):
        write_png(out / f"{index}.png", figure(colors))
        print(out / f"{index}.png")


if __name__ == "__main__":
    main()
