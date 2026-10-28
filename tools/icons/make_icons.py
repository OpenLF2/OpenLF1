#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 OpenLF1 contributors
# Based on OpenLF2's tools/icons/make_icons.py, Copyright (c) 2026 OpenLF2 contributors, MIT.
"""Renders packaging/openlf1.svg into the images the executable embeds.

    make_icons.py OUT_DIR [--svg FILE] [--icns] [--force]

The SVG is the project's only copy of the icon. Python 3 with the standard library is enough: the
small rasterizer below draws the subset of SVG the icon uses and refuses anything else, so a change
that goes beyond it fails here instead of rendering wrongly. It supports rect (with rx/ry), groups,
solid fills, fill-opacity, and clip paths made from a single rect.

Output layout under OUT_DIR:
    window_icon.hpp   128 px raw RGBA pixels (the SDL window icon)
    openlf1.ico       Windows executable icon (16, 24, 32, 48, 64, 128, 256 px)
    openlf1.icns      macOS app icon (16 to 1024 px, with --icns)
"""
import hashlib
import math
import os
import re
import struct
import sys
import zlib
import xml.etree.ElementTree as ET

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_SVG = os.path.join(ROOT, "packaging", "openlf1.svg")
NAMESPACE = "{http://www.w3.org/2000/svg}"
WINDOW_SIZE = 128
ICON_SIZES = (16, 24, 32, 48, 64, 128, 256)
ICNS_SIZES = ((16, "icp4"), (32, "icp5"), (64, "icp6"), (128, "ic07"),
              (256, "ic08"), (512, "ic09"), (1024, "ic10"))
SUBSAMPLES = 4


class Unsupported(Exception):
    pass


def number(value, default=0.0):
    if value is None:
        return default
    return float(re.match(r"\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?)", value).group(1))


def color(value):
    match = re.fullmatch(r"#([0-9a-fA-F]{3}|[0-9a-fA-F]{6})", value.strip())
    if not match:
        raise Unsupported("color %r (use #rgb or #rrggbb)" % value)
    text = match.group(1)
    if len(text) == 3:
        text = "".join(c * 2 for c in text)
    return tuple(int(text[i:i + 2], 16) / 255.0 for i in (0, 2, 4))


def rect_polygon(x, y, width, height, rx, ry):
    if rx <= 0.0 or ry <= 0.0:
        return [(x, y), (x + width, y), (x + width, y + height), (x, y + height)]
    rx, ry = min(rx, width / 2.0), min(ry, height / 2.0)
    steps = 16
    points = []
    for cx, cy, start in ((x + width - rx, y + ry, -90), (x + width - rx, y + height - ry, 0),
                          (x + rx, y + height - ry, 90), (x + rx, y + ry, 180)):
        for step in range(steps + 1):
            angle = math.radians(start + 90.0 * step / steps)
            points.append((cx + rx * math.cos(angle), cy + ry * math.sin(angle)))
    return points


def rect_points(element):
    x, y = number(element.get("x")), number(element.get("y"))
    width, height = number(element.get("width")), number(element.get("height"))
    rx_attr, ry_attr = element.get("rx"), element.get("ry")
    if rx_attr is None and ry_attr is None:
        rx = ry = 0.0
    elif rx_attr is None:
        rx = ry = number(ry_attr)
    elif ry_attr is None:
        rx = ry = number(rx_attr)
    else:
        rx, ry = number(rx_attr), number(ry_attr)
    return rect_polygon(x, y, width, height, rx, ry)


def parse_clip_paths(root):
    clips = {}
    for element in root.iter(NAMESPACE + "clipPath"):
        if element.get("transform"):
            raise Unsupported("clipPath transforms")
        children = [child for child in element if child.tag != NAMESPACE + "title"]
        if len(children) != 1 or children[0].tag != NAMESPACE + "rect":
            raise Unsupported("clipPath contents (only one rect)")
        clips[element.get("id")] = rect_points(children[0])
    return clips


def walk(element, inherited, clip, shapes, clips):
    tag = element.tag.replace(NAMESPACE, "")
    if tag in ("title", "desc", "metadata", "defs", "clipPath"):
        return
    if element.get("transform") or element.get("style"):
        raise Unsupported("<%s> transform/style attributes" % tag)
    if element.get("clip-path"):
        match = re.fullmatch(r"url\(#([^)]+)\)", element.get("clip-path"))
        if not match or match.group(1) not in clips:
            raise Unsupported("clip-path %r" % element.get("clip-path"))
        clip = clips[match.group(1)]
    if tag in ("svg", "g"):
        style = dict(inherited)
        for name in ("fill", "fill-opacity", "opacity"):
            if element.get(name) is not None:
                style[name] = element.get(name)
        for child in element:
            walk(child, style, clip, shapes, clips)
        return
    if tag != "rect":
        raise Unsupported("element <%s>" % tag)
    fill = element.get("fill", inherited["fill"])
    if fill == "none":
        return
    if fill.startswith("url("):
        raise Unsupported("gradient fills")
    opacity = number(element.get("opacity", inherited["opacity"]), 1.0) * \
        number(element.get("fill-opacity", inherited["fill-opacity"]), 1.0)
    shapes.append(([rect_points(element)], color(fill), opacity, clip))


def coverage(polygons, width, height):
    """Anti-aliased nonzero-winding coverage in pixel space: {row: [coverage per pixel]}."""
    rows = {}
    buckets = {}
    for poly in polygons:
        count = len(poly)
        for i in range(count):
            x0, y0 = poly[i]
            x1, y1 = poly[(i + 1) % count]
            if y0 == y1:
                continue
            direction = 1 if y1 > y0 else -1
            if y0 > y1:
                x0, y0, x1, y1 = x1, y1, x0, y0
            first = max(0, math.ceil(y0 * SUBSAMPLES - 0.5))
            last = min(height * SUBSAMPLES - 1, math.ceil(y1 * SUBSAMPLES - 0.5) - 1)
            slope = (x1 - x0) / (y1 - y0)
            for sub in range(first, last + 1):
                y = (sub + 0.5) / SUBSAMPLES
                buckets.setdefault(sub, []).append((x0 + (y - y0) * slope, direction))
    weight = 1.0 / SUBSAMPLES
    for sub, crossings in buckets.items():
        crossings.sort()
        row = rows.get(sub // SUBSAMPLES)
        if row is None:
            row = rows[sub // SUBSAMPLES] = ([0.0] * (width + 2), [0.0] * (width + 2))
        cov, delta = row
        winding = 0
        for x, direction in crossings:
            before = winding
            winding += direction
            if before == 0 and winding != 0:
                start = x
            elif before != 0 and winding == 0:
                a, b = max(start, 0.0), min(x, float(width))
                if b <= a:
                    continue
                ia, ib = int(a), int(b)
                if ia == ib:
                    cov[ia] += (b - a) * weight
                else:
                    cov[ia] += (ia + 1 - a) * weight
                    delta[ia + 1] += weight
                    delta[ib] -= weight
                    cov[ib] += (b - ib) * weight
    result = {}
    for y, (cov, delta) in rows.items():
        running = 0.0
        line = [0.0] * width
        for x in range(width):
            running += delta[x]
            line[x] = min(1.0, cov[x] + running)
        result[y] = line
    return result


def render(svg_path, size):
    root = ET.parse(svg_path).getroot()
    view = [float(value) for value in root.get("viewBox").split()]
    if view[0] != 0 or view[1] != 0 or view[2] <= 0 or view[3] <= 0:
        raise Unsupported("viewBox")
    if abs(view[2] - view[3]) > 1e-9:
        raise Unsupported("non-square viewBox")
    scale = size / view[2]
    clips = parse_clip_paths(root)
    shapes = []
    walk(root, {"fill": "#000000", "fill-opacity": "1", "opacity": "1"}, None, shapes, clips)
    buffer = [[0.0] * (size * 4) for _ in range(size)]
    masks = {}

    for polygons, paint, opacity, clip in shapes:
        pixel_polygons = [[(x * scale, y * scale) for x, y in polygon] for polygon in polygons]
        mask = None
        if clip is not None:
            if id(clip) not in masks:
                masks[id(clip)] = coverage([[(x * scale, y * scale) for x, y in clip]], size, size)
            mask = masks[id(clip)]
        for y, line in coverage(pixel_polygons, size, size).items():
            row = buffer[y]
            mask_line = mask.get(y) if mask is not None else None
            for x, cover in enumerate(line):
                if cover <= 0.0:
                    continue
                if mask is not None:
                    if mask_line is None:
                        continue
                    cover *= mask_line[x]
                alpha = cover * opacity
                if alpha <= 0.0:
                    continue
                inverse = 1.0 - alpha
                i = x * 4
                row[i] = paint[0] * alpha + row[i] * inverse
                row[i + 1] = paint[1] * alpha + row[i + 1] * inverse
                row[i + 2] = paint[2] * alpha + row[i + 2] * inverse
                row[i + 3] = alpha + row[i + 3] * inverse
    image = []
    for row in buffer:
        line = []
        for i in range(0, size * 4, 4):
            alpha = row[i + 3]
            if alpha <= 0.0:
                line.append((0, 0, 0, 0))
            else:
                line.append(tuple(min(255, int(round(row[i + k] / alpha * 255))) for k in range(3)) +
                            (min(255, int(round(alpha * 255))),))
        image.append(line)
    return image


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)


def png(image):
    height, width = len(image), len(image[0])
    raw = b"".join(b"\0" + bytes(value for pixel in row for value in pixel) for row in image)
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def ico(entries):
    header = struct.pack("<HHH", 0, 1, len(entries))
    offset = 6 + 16 * len(entries)
    directory, data = b"", b""
    for size, payload in entries:
        directory += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(payload), offset + len(data))
        data += payload
    return header + directory + data


def icns(entries):
    body = b"".join(kind.encode("ascii") + struct.pack(">I", len(payload) + 8) + payload
                    for kind, payload in entries)
    return b"icns" + struct.pack(">I", len(body) + 8) + body


def c_array(name, data):
    lines = []
    for i in range(0, len(data), 24):
        lines.append("    " + ", ".join("0x%02x" % byte for byte in data[i:i + 24]) + ",")
    return ("// SPDX-License-Identifier: MIT\n// Copyright (c) 2026 OpenLF1\n\n"
            "// Generated by tools/icons/make_icons.py from packaging/openlf1.svg; not committed.\n"
            "#pragma once\n#include <array>\n\nnamespace openlf1::icon {\n"
            "inline constexpr int window_size = %d;\n"
            "inline constexpr std::array<unsigned char, %d> window_rgba{\n%s\n};\n}\n"
            % (WINDOW_SIZE, len(data), "\n".join(lines)))


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as output:
        output.write(data)


def generate(svg, out, with_icns=False):
    cache = {}

    def image(size):
        if size not in cache:
            cache[size] = render(svg, size)
        return cache[size]

    write(os.path.join(out, "window_icon.hpp"),
          c_array("window_rgba", bytes(value for row in image(WINDOW_SIZE) for pixel in row for value in pixel)).encode())
    write(os.path.join(out, "openlf1.ico"), ico([(size, png(image(size))) for size in ICON_SIZES]))
    if with_icns:
        write(os.path.join(out, "openlf1.icns"),
              icns([(kind, png(image(size))) for size, kind in ICNS_SIZES]))


def main(argv):
    args = argv[1:]
    force = "--force" in args
    with_icns = "--icns" in args
    svg = DEFAULT_SVG
    if "--svg" in args:
        svg = os.path.abspath(args[args.index("--svg") + 1])
    positional = [arg for i, arg in enumerate(args) if not arg.startswith("--") and (i == 0 or args[i - 1] != "--svg")]
    if len(positional) != 1:
        print(__doc__)
        return 2
    out = os.path.abspath(positional[0])
    digest = hashlib.sha256()
    for path in (svg, os.path.abspath(__file__)):
        with open(path, "rb") as source:
            digest.update(source.read())
    digest.update(b"icns" if with_icns else b"")
    stamp = os.path.join(out, ".stamp")
    outputs = [os.path.join(out, "window_icon.hpp"), os.path.join(out, "openlf1.ico")]
    if with_icns:
        outputs.append(os.path.join(out, "openlf1.icns"))
    if not force and os.path.exists(stamp) and all(os.path.exists(path) for path in outputs):
        with open(stamp) as source:
            if source.read() == digest.hexdigest():
                return 0
    os.makedirs(out, exist_ok=True)
    try:
        generate(svg, out, with_icns)
    except Unsupported as error:
        print("make_icons: the SVG uses something the built-in renderer does not draw: %s" % error,
              file=sys.stderr)
        return 1
    with open(stamp, "w") as target:
        target.write(digest.hexdigest())
    print("make_icons: wrote icons into %s" % out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
