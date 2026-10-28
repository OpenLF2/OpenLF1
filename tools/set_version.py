#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 OpenLF1

"""Updates the project's version and copyright year of record.

    tools/set_version.py X.Y[.Z] [--year YYYY]

CMakeLists.txt's `project(OpenLF1 VERSION X.Y[.Z] LANGUAGES CXX)` is the only stored version.
packaging/openlf1.rc.in reads CMake's PROJECT_VERSION and PROJECT_VERSION_MAJOR/MINOR/PATCH at
configure time, so the Windows VERSIONINFO follows automatically. The macOS app bundle's
Info.plist uses PROJECT_VERSION for CFBundleVersion and CFBundleShortVersionString. A missing
patch component is recorded as 0 in the numeric Windows file version. The copyright year is
stored in OPENLF1_COPYRIGHT_YEAR and is used by the same VERSIONINFO; it defaults to the current
year.
packaging/io.github.openlf2.OpenLF1.metainfo.xml keeps one AppStream <release> entry in sync with
the version and its release date, without maintaining a changelog. Icon generation does not depend
on either value.
"""
import datetime
import pathlib
import re
import sys

VERSION_PATTERN = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)(?:\.(0|[1-9]\d*))?$")
YEAR_PATTERN = re.compile(r"^\d{4}$")
PROJECT_PATTERN = re.compile(r"project\(OpenLF1 VERSION (\d+\.\d+(?:\.\d+)?) LANGUAGES CXX\)")
COPYRIGHT_PATTERN = re.compile(r"set\(OPENLF1_COPYRIGHT_YEAR (\d{4})\)")
APPSTREAM_PATTERN = re.compile(r'<release version="(\d+\.\d+(?:\.\d+)?)" date="(\d{4}-\d{2}-\d{2})"')
ROOT = pathlib.Path(__file__).resolve().parent.parent


def main(argv):
    args = argv[1:]
    year = None
    if "--year" in args:
        index = args.index("--year")
        if index + 1 >= len(args):
            print(__doc__)
            return 2
        year = args[index + 1]
        del args[index:index + 2]
    if len(args) != 1 or not VERSION_PATTERN.match(args[0]) or (year is not None and not YEAR_PATTERN.match(year)):
        print(__doc__)
        return 2
    new_version = args[0]
    today = datetime.date.today()
    new_year = year or str(today.year)

    path = ROOT / "CMakeLists.txt"
    text = path.read_text(encoding="utf-8")
    versions = PROJECT_PATTERN.findall(text)
    years = COPYRIGHT_PATTERN.findall(text)
    if len(versions) != 1 or len(years) != 1:
        raise SystemExit("set_version: expected exactly one project version and copyright year in %s" % path)
    old_version, old_year = versions[0], years[0]
    version_changed = old_version != new_version

    appstream_path = ROOT / "packaging/io.github.openlf2.OpenLF1.metainfo.xml"
    appstream_text = appstream_path.read_text(encoding="utf-8")
    if len(APPSTREAM_PATTERN.findall(appstream_text)) != 1:
        raise SystemExit("set_version: expected exactly one AppStream release in %s" % appstream_path)
    if version_changed:
        appstream_text = APPSTREAM_PATTERN.sub(
            '<release version="%s" date="%s"' % (new_version, today.isoformat()), appstream_text, count=1)

    if not version_changed and old_year == new_year:
        print("set_version: version is already %s and copyright year is already %s" % (new_version, new_year))
        return 0

    text = PROJECT_PATTERN.sub("project(OpenLF1 VERSION %s LANGUAGES CXX)" % new_version, text, count=1)
    text = COPYRIGHT_PATTERN.sub("set(OPENLF1_COPYRIGHT_YEAR %s)" % new_year, text, count=1)
    path.write_text(text, encoding="utf-8")
    if version_changed:
        appstream_path.write_text(appstream_text, encoding="utf-8")
        print("set_version: version %s -> %s" % (old_version, new_version))
        print("set_version: AppStream release dated %s" % today.isoformat())
    if old_year != new_year:
        print("set_version: copyright year %s -> %s" % (old_year, new_year))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
