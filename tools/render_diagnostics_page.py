#!/usr/bin/env python3
"""Render the Prospector diagnostics page for documentation.

The renderer compiles and runs the actual diagnostics LVGL page with host-side
ZMK/Zephyr stubs, then captures the LVGL framebuffer as a PNG.
"""

from __future__ import annotations

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CACHE_DIR = REPO_ROOT / ".render-cache" / "operator"
HOST_SOURCE_DIR = REPO_ROOT / "tools" / "operator_host"


def run(command: list[str], cwd: Path | None = None) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=cwd, check=True)


def find_lvgl_dir(cache_dir: Path, explicit: Path | None = None) -> Path | None:
    candidates: list[Path] = []
    if explicit:
        candidates.append(explicit)
    if "LVGL_DIR" in os.environ:
        candidates.append(Path(os.environ["LVGL_DIR"]))
    candidates.extend(
        [
            cache_dir / "zmk-workspace" / "modules" / "lib" / "gui" / "lvgl",
            REPO_ROOT.parent / "zmk-workspace" / "modules" / "lib" / "gui" / "lvgl",
            REPO_ROOT.parent / "charybdis-wireless-mini-zmk-firmware" / "modules" / "lib" / "gui" / "lvgl",
        ]
    )

    for candidate in candidates:
        if (candidate / "lvgl.h").exists() and (candidate / "CMakeLists.txt").exists():
            return candidate.resolve()
    return None


def write_png_rgb(path: Path, width: int, height: int, pixels: bytes | bytearray) -> None:
    def chunk(kind: bytes, payload: bytes) -> bytes:
        return (
            struct.pack(">I", len(payload))
            + kind
            + payload
            + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
        )

    scanlines = bytearray()
    row_width = width * 3
    for y in range(height):
        scanlines.append(0)
        scanlines.extend(pixels[y * row_width : (y + 1) * row_width])

    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png.extend(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
    png.extend(chunk(b"IDAT", zlib.compress(bytes(scanlines), level=9)))
    png.extend(chunk(b"IEND", b""))
    path.write_bytes(png)


def raw_to_png(raw_path: Path, png_path: Path, scale: int) -> None:
    with raw_path.open("rb") as fp:
        magic = fp.readline().decode("ascii").strip()
        if magic != "LVRAW8888":
            raise RuntimeError(f"Unexpected renderer output format: {magic}")
        width_s, height_s, stride_s = fp.readline().decode("ascii").split()
        width = int(width_s)
        height = int(height_s)
        stride = int(stride_s)
        data = fp.read()

    pixels = bytearray(width * height * 3)
    for y in range(height):
        row = data[y * stride : y * stride + width * 4]
        for x in range(width):
            b, g, r, _a = row[x * 4 : x * 4 + 4]
            offset = (y * width + x) * 3
            pixels[offset : offset + 3] = bytes((r, g, b))

    if scale > 1:
        scaled_width = width * scale
        scaled_height = height * scale
        scaled = bytearray(scaled_width * scaled_height * 3)
        for y in range(height):
            src = pixels[y * width * 3 : (y + 1) * width * 3]
            scaled_row = bytearray()
            for x in range(width):
                px = src[x * 3 : x * 3 + 3]
                scaled_row.extend(px * scale)
            for repeat in range(scale):
                dest_y = y * scale + repeat
                scaled[dest_y * scaled_width * 3 : (dest_y + 1) * scaled_width * 3] = scaled_row
        width = scaled_width
        height = scaled_height
        pixels = scaled

    png_path.parent.mkdir(parents=True, exist_ok=True)
    write_png_rgb(png_path, width, height, pixels)


def main() -> None:
    parser = argparse.ArgumentParser(description="Render a Prospector diagnostics page PNG.")
    parser.add_argument("--lvgl-dir", type=Path, help="Path to an existing LVGL checkout")
    parser.add_argument("--cache-dir", type=Path, default=DEFAULT_CACHE_DIR)
    parser.add_argument("--output", type=Path, default=REPO_ROOT / "docs" / "images" / "diagnostics-page.png")
    parser.add_argument("--scale", type=int, default=2)
    parser.add_argument("--uptime-minutes", type=int, default=(2 * 24 * 60) + (7 * 60) + 42)
    args = parser.parse_args()

    if shutil.which("cmake") is None:
        parser.error("cmake is required to render the diagnostics page")

    lvgl_dir = find_lvgl_dir(args.cache_dir, args.lvgl_dir)
    if lvgl_dir is None:
        parser.error("LVGL not found. Pass --lvgl-dir or set LVGL_DIR.")

    build_dir = args.cache_dir / "build" / "diagnostics"
    build_dir.mkdir(parents=True, exist_ok=True)

    run(
        [
            "cmake",
            "-S",
            str(HOST_SOURCE_DIR),
            "-B",
            str(build_dir),
            f"-DPROSPECTOR_ROOT={REPO_ROOT}",
            f"-DLVGL_DIR={lvgl_dir}",
        ]
    )
    run(["cmake", "--build", str(build_dir), "--target", "diagnostics_renderer"])

    with tempfile.TemporaryDirectory() as tmp:
        raw_path = Path(tmp) / "diagnostics.raw"
        run(
            [
                str(build_dir / "diagnostics_renderer"),
                "--output",
                str(raw_path),
                "--uptime-minutes",
                str(args.uptime_minutes),
            ]
        )
        raw_to_png(raw_path, args.output, args.scale)

    print(args.output)


if __name__ == "__main__":
    sys.exit(main())
