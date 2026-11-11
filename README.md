# xrCompressorMT

**Multithreaded x64 packer / unpacker / verifier for S.T.A.L.K.E.R. `.db` / `.xdb` archives.**

One `.exe`: double-click for a GUI (UA / EN), or pass arguments for the console packer.

**English** | [Українська](README.ua.md)

## Features

- **x64 + multithreaded** — LZO compression runs on all CPU cores (`-threads N` to limit)
- **Single executable** — GUI when started without arguments, CLI when started with arguments
- **Pack, unpack, verify** — CRC32 of every file is checked after writing (can be disabled with `-noverify`)
- **Auto-splitting** — archives are split at `-max_size` MB (default 1900, hard cap 2047) into `name.xdb0`, `name.xdb1`, … (up to 36 parts)
- **Deduplication** — identical files inside one archive are stored once
- **Engine-safe "store" list** — files streamed by the engine are never compressed: `*.geom`, `*.geomx`, `*.ogm`, `*.ogg`, `level.ai`, `game.graph` (compressing them crashes OGSR / OpenXRay); extendable from `datapack.ltx`
- **`datapack.ltx` support** — `[header]`, `[include_folders]`, `[exclude_folders]`, `[include_files]`, `[exclude_files]`, `[options]`, `[store_files]`
- **Unicode paths** (Cyrillic etc.) in both GUI and CLI
- Modes: `lzo1x_999` (max compression, default), `-fast` (`lzo1x_1`), `-store` (no compression)

## Compatibility

COC, COP, Monolith (xray-monolith), OpenXRay, OGSR (`.xdb` in the mod folder), SOC.

> **SoC (original exe):** put `.xdb` into the mods folder (`$mod_dir$`) — from the game root SoC reads only `.db`.

## Usage

### GUI
Run `xrCompressorMT.exe` without arguments. Put `datapack.ltx` next to the exe — the archive header is taken from it automatically.

### Console
```
xrCompressorMT <folder> [-ltx datapack.ltx] [-xdb] [-max_size MB] [-fast | -store] -out <name.xdb>
               [-threads N] [-mem MB] [-noverify] [-v]
xrCompressorMT -check  <archive> [archive ...]
xrCompressorMT -unpack <archive> <output_folder>
```

| Option | Description |
|---|---|
| `-ltx <file>` | file list / header / rules (`datapack.ltx`) |
| `-xdb` | write `.xdb` instead of `.db` |
| `-max_size <MB>` | per-archive limit (default 1900, max 2047) |
| `-fast` / `-store` | fast LZO / no compression |
| `-threads <N>` | worker threads (default: all logical cores) |
| `-mem <MB>` | max MB of file data kept in RAM by the pipeline (default 1024) |
| `-noverify` | skip the automatic check after packing |
| `-check` | verify existing archives (CRC of every file) |
| `-unpack` | extract an archive |
| `-v` | verbose per-file output |

On Windows run it from `cmd` as `start /wait xrCompressorMT.exe ...` to get the console output.

Example:
```
xrCompressorMT.exe "D:\mod\gamedata" -ltx datapack.ltx -xdb -max_size 1900 -out mymod
```

### datapack.ltx
```ini
[header]
auto_load = true
entry_point = $fs_root$\gamedata\

[options]
exclude_exts = *.bak,*.tmp
store_exts = *.dds,foo.bin     ; extra patterns kept uncompressed
store_defaults = false         ; disable the built-in no-compress list

[store_files]
levels\l1\level.cform          ; one path / pattern per line
```

## Archive format

```
chunk 666 : ini header ([header])        (always written)
chunk 0   : raw data of all files (lzo1x or stored)
chunk 1   : file table, LzHuf-compressed (type | 0x80000000)
table record: u16 size | u32 size_real | u32 size_compr | u32 crc32 | name | u32 ptr
size_real == size_compr  =>  stored uncompressed
```
Compatible with the CoC/CoP and xray-monolith readers (`LocatorAPI::ProcessArchive`).

## Building

Requires Python + [zig](https://ziglang.org/) used as a cross-compiler:

```sh
pip install ziglang
sh build_win64.sh      # Windows x64: xrCompressorMT.exe (GUI + console)
sh build_linux.sh      # Linux: console-only build (g++)
```

## Repository layout

```
src/      main.cpp          packer / checker / unpacker (CLI core)
gui/      gui_win.cpp       Win32 GUI, resources, manifest, icon
lzo/      LZO + LzHuf sources
shim/     stdafx.h          minimal X-Ray core shim
```

## License

Released under **GPL-2.0** (see `LICENSE`), because it includes the LZO library by Markus F.X.J. Oberhumer, which is GPLv2. LZO is © 1996–2005 Markus Franz Xaver Johannes Oberhumer.

## Author

Ivan Danko, 2026 · Telegram: [t.me/frozen_zone](https://t.me/frozen_zone)
