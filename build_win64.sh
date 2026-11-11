#!/bin/sh
# Single exe (window + console packer), cross-compiled for Windows x64 with zig (pip install ziglang)
cd "$(dirname "$0")" && python3 -m ziglang c++ -target x86_64-windows-gnu -std=c++17 -O2 -w -Dregister= -fpermissive -DXR_EMBED \
  -Ishim -Ilzo -mwindows -Wl,--subsystem,windows -static \
  src/main.cpp gui/gui_win.cpp gui/gui.rc lzo/lzhuf_core.cpp lzo/rt_lzo1x_9x.cpp lzo/rt_lzo1x_1.cpp lzo/rt_lzo_init.cpp lzo/rt_lzo1x_d1.cpp lzo/rt_lzo1x_d2.cpp \
  -luser32 -lgdi32 -lcomctl32 -lcomdlg32 -lshell32 -lole32 -ladvapi32 -o xrCompressorMT.exe
