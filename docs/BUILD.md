# Build Guide

The same source tree produces both the **Windows binary (`tgui.exe`)** and the
**Linux binary (`tgui`)**. On Linux you can either build **natively** for Linux or
**cross-compile** for Windows; on Windows you build **natively**. No external libraries are
required (the CLI uses only the Win32 API and libc) — a compiler and CMake are enough.

| Requirement | Version |
|---|---|
| CMake | 3.16 or newer |
| Compiler | GCC or Clang (Linux); MinGW-w64 GCC or MSVC (Windows). All are built in CI |
| Build tool | Ninja recommended (make also works) |

---

## A0. Linux → Linux (native)

```bash
# Debian / Ubuntu
sudo apt install -y cmake ninja-build gcc
# Fedora:  sudo dnf install -y cmake ninja-build gcc
# Arch:    sudo pacman -S cmake ninja gcc

git clone https://github.com/packetresult-cmd/TunnelingGUI.git
cd TunnelingGUI
cmake -S . -B build -G Ninja
cmake --build build                                       # -> build/tgui
sudo install -m 0755 build/tgui /usr/local/bin/    # (optional) install
```

The Linux binary depends on nothing beyond libc. The runtime tools it needs — `wg`, `iptables`,
`ip` and `curl` — are installed automatically through your distribution's package manager if they
are missing.

Regression test: `sudo apt install -y wireguard-tools && sudo tests/linux/run.sh build/tgui`

---

## A. Linux → Windows (cross-compile)

### 1) Install the toolchain

```bash
# Debian / Ubuntu
sudo apt update
sudo apt install -y cmake ninja-build gcc-mingw-w64-x86-64          # 64-bit
sudo apt install -y gcc-mingw-w64-i686                              # (optional) 32-bit

# Fedora
sudo dnf install -y cmake ninja-build mingw64-gcc                   # 32-bit: mingw32-gcc

# Arch
sudo pacman -S cmake ninja mingw-w64-gcc
```

Verify with `x86_64-w64-mingw32-gcc --version` and `x86_64-w64-mingw32-windres --version`.

### 2) Build

```bash
git clone https://github.com/packetresult-cmd/TunnelingGUI.git
cd TunnelingGUI

cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake
cmake --build build
```

Output: `build/tgui.exe` (around 90 KB; statically linked, so it runs without extra DLLs).

### 3) 32-bit / ARM64

```bash
# 32-bit (x86)
cmake -S . -B build-x86 -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake -DMINGW_TRIPLET=i686-w64-mingw32
cmake --build build-x86

# ARM64 (requires llvm-mingw; aarch64-w64-mingw32-gcc must be on PATH / untested)
cmake -S . -B build-arm64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake -DMINGW_TRIPLET=aarch64-w64-mingw32
```

### 4) Smoke test (optional)

Setting up a real tunnel only works on Windows, but you can check the CLI and `verify` under wine:

```bash
wine build/tgui.exe help
file build/tgui.exe        # PE32+ executable (console) x86-64
```

---

## B. Building on Windows

### Option 1: MSYS2 + MinGW-w64 (recommended)

1. Install MSYS2 from https://www.msys2.org
2. Open the **MSYS2 MINGW64** terminal from the Start menu:

```bash
pacman -Syu                      # if the window closes, reopen it and run this once more
pacman -S --needed git mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja

git clone https://github.com/packetresult-cmd/TunnelingGUI.git
cd TunnelingGUI
cmake -S . -B build -G Ninja
cmake --build build
```

Output: `build\tgui.exe` (no toolchain file is used here).

### Option 2: Visual Studio

With the "Desktop development with C++" workload installed, open the
**x64 Native Tools Command Prompt**:

```bat
cmake -S . -B build -G Ninja
cmake --build build --config Release
```

For MSVC the project sets `/MT` (static CRT) and `/MANIFEST:NO` (to avoid duplicating the
administrator manifest). This build is verified in CI (GitHub Actions).

---

## Building the GUI (SDL3)

The GUI is optional. With `-DTGUI_BUILD_GUI=ON`, CMake fetches **SDL3 3.4.18** and
**SDL3_ttf 3.2.2** (FreeType included) from the official repositories at pinned tags and links them
statically. The first build takes a few minutes.

Output: `tgui-gui`(`.exe`) — it **must sit in the same folder as the `tgui` CLI**
(they are built together).

```bash
# Linux (Debian/Ubuntu): SDL3's X11/Wayland development headers + a CJK font
sudo apt install -y cmake ninja-build gcc fonts-nanum \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxfixes-dev libxtst-dev \
  libxkbcommon-dev libwayland-dev wayland-protocols libegl-dev libgl-dev
cmake -S . -B build -G Ninja -DTGUI_BUILD_GUI=ON
cmake --build build                 # -> build/tgui, build/tgui-gui
```

```bat
:: Windows (Visual Studio x64 Native Tools Command Prompt)
cmake -S . -B build -A x64 -DTGUI_BUILD_GUI=ON
cmake --build build --config Release   :: -> build\Release\tgui.exe, tgui-gui.exe
```

```bash
# Windows (MSYS2 MINGW64)
cmake -S . -B build -G Ninja -DTGUI_BUILD_GUI=ON && cmake --build build
```

- The build copies `lang/` next to `tgui-gui`, and `cmake --install` places it in `bin/lang`.
  The GUI reads `lang/<code>.json` from its own folder; English is compiled in, so a missing
  folder only means no other languages are offered.
- To link against an SDL3 / SDL3_ttf you already have installed, use `-DTGUI_USE_SYSTEM_SDL=ON`.
- Fonts: Windows uses Malgun Gothic automatically. On Linux the GUI looks for Noto Sans CJK or
  NanumGothic; if neither is present, point it at a font with `TGUI_GUI_FONT=/path/to/font.ttf`.
- Headless check: `SDL_VIDEO_DRIVER=offscreen tgui-gui --demo --screenshot shot.png`

## Build options

| Option | Description |
|---|---|
| `-DCMAKE_BUILD_TYPE=Debug` | Debug build (the default is Release, symbols stripped) |
| `-DMINGW_TRIPLET=...` | Cross-compilation target (when using the toolchain file) |
| `cmake --install build --prefix out` | Copies the binary to `out/bin/tgui.exe` |

## Troubleshooting

- `x86_64-w64-mingw32-gcc: not found` — mingw-w64 is not installed, or your distribution names the
  package differently. Check the install commands above.
- `windres: not found` / RC compiler errors — mingw-w64 binutils is missing (it usually ships with
  the gcc package).
- **You got a Linux binary instead of an `.exe`** — for a Windows build, do not omit
  `-DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake`.
- Stale build directory: `rm -rf build` and configure again.
- **Antivirus may flag a freshly built executable.** This is an unsigned tool that requests
  administrator privileges and reconfigures network interfaces, which is exactly the profile
  heuristics look for. Confirm that you built it yourself from source, then add an exclusion.
