<p align="center"><img src="packaging/openlf1.svg" alt="OpenLF1 logo" width="128" height="128"></p>

# OpenLF1

OpenLF1 is an open-source reimplementation of **Little Fighter (1995)**.
It uses the original game data to recreate the original's introduction, menus, character
selection, and side-view battles.

## Beyond the original

- Keyboard and gamepad support on modern systems
- Reads the original game data directly from its ZIP archive without extracting it
- Alt+Enter fullscreen toggle

## Getting started

Try the [WebAssembly build in your browser](https://openlf2.github.io/OpenLF1/). It can download
the game data from archive.org, stores it in browser storage, and runs locally.

### Downloads

| Platform | Downloads |
| --- | --- |
| Windows | [x86](https://github.com/OpenLF2/OpenLF1/releases/latest/download/openlf1-x86.exe) · [x64](https://github.com/OpenLF2/OpenLF1/releases/latest/download/openlf1-x64.exe) · [ARM64](https://github.com/OpenLF2/OpenLF1/releases/latest/download/openlf1-arm64.exe) |
| Linux | AppImage: [x86](https://github.com/OpenLF2/OpenLF1/releases/latest/download/openlf1-x86.AppImage) · [x64](https://github.com/OpenLF2/OpenLF1/releases/latest/download/openlf1-x64.AppImage) · [ARM64](https://github.com/OpenLF2/OpenLF1/releases/latest/download/openlf1-arm64.AppImage)<br>DEB: [x86](https://github.com/OpenLF2/OpenLF1/releases/latest/download/openlf1-x86.deb) · [x64](https://github.com/OpenLF2/OpenLF1/releases/latest/download/openlf1-x64.deb) · [ARM64](https://github.com/OpenLF2/OpenLF1/releases/latest/download/openlf1-arm64.deb) |
| macOS | [ARM64](https://github.com/OpenLF2/OpenLF1/releases/latest/download/openlf1-macos-arm64.zip) · [x64](https://github.com/OpenLF2/OpenLF1/releases/latest/download/openlf1-macos-x64.zip) |
| DOS | [DOS build](https://github.com/OpenLF2/OpenLF1/releases/latest/download/openlf1-dos.exe) |
| Web | [Browser build](https://github.com/OpenLF2/OpenLF1/releases/latest/download/openlf1-web.zip) |

See the [full release page](https://github.com/OpenLF2/OpenLF1/releases) for release notes.

### Game data

OpenLF1 does not include the original game. Download
[Little Fighter (1995)](https://archive.org/download/msdos_Little_Fighter_1995/Little_Fighter_1995.zip)
and place the ZIP file next to OpenLF1 or in the current working directory. The
[primary download is available from lf2.net](https://lf2.net/lf1.zip). The archive can
have any name and its internal folders can be arranged freely. OpenLF1 reads the game data
directly from the archive; nothing is extracted.

You can also pass the ZIP path explicitly:

```sh
./build/openlf1 /path/to/game.zip
```

### Build from source

To build from source, you need a C++23 compiler, CMake 3.20+, SDL3, and zlib:

```sh
cmake -S . -B build
cmake --build build
./build/openlf1
```

To build SDL3 and zlib from source instead of using system packages, add
`-DOPENLF1_BUNDLE_DEPENDENCIES=ON` when configuring.

## Controls

In menus, Escape exits the program. During a battle, Escape or gamepad Start pauses and any key
resumes. Navigate menus with W/X, I/comma, or numpad 8/2; confirm with Enter, S, K, or numpad 5.
During a battle, the same directional keys move the characters; S, K, or numpad 5
attacks, and Tab, Space, or numpad 0 jumps. F10 or gamepad Back/Select ends the battle. Alt+Enter
toggles fullscreen. Up to three gamepads can be used at once, with the D-pad or left stick and
action buttons.

## Used libraries

OpenLF1 uses two third-party libraries, both under the zlib license:

| Library | Version | Purpose |
| --- | --- | --- |
| [SDL3](https://www.libsdl.org/) | 3.2.28 when bundled, otherwise system-provided | Window, rendering, input, gamepad, and audio support |
| [zlib](https://zlib.net/) | 1.3.1 when bundled, otherwise system-provided | ZIP decompression |

The DOS build uses a DJGPP SDL3 snapshot instead of the 3.2.28 release.

See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) for the copyright and license
notices. The Linux AppImage also carries the C++ runtime and other shared libraries
selected by linuxdeploy; those remain under their own licenses.

## License

OpenLF1's code, scripts, and documentation are MIT-licensed ([LICENSE](LICENSE)). This
license does not cover the original Little Fighter game, its executable, or its assets,
which are not included in this repository.

## The original game

Little Fighter was designed and developed by Marti Wong and released in 1995. Learn more
at the [original game page](https://lf2.net/lf1.html).

## Trademarks

Little Fighter and any associated names are the property of their respective rights
holders. They are used here only to identify the game. OpenLF1 is an independent project
and is not affiliated with or endorsed by the original game's creator.
