# TunnelingGUI (C, Windows + Linux)

Host a game server on your home PC and have it appear on the public internet at your VPS's IP address.
No router port forwarding, no public IP at home — works behind CGNAT.

This is a single C program that replaces five PowerShell scripts (`vps-setup`, `vps-uninstall`,
`gamepc-tunnel`, `gamepc-uninstall`, `verify`) with one WireGuard full-tunnel (Type C) tool.
The same commands work on **both Windows and Linux**. It **installs WireGuard automatically** if it is
missing, and **elevates itself** when it needs more privilege (UAC on Windows, `sudo` on Linux).

```
[players] ──► VPS public IP ──(WireGuard 10.8.0.0/24)──► game PC (10.8.0.2)
                              all IPv4 traffic from the game PC also exits via the VPS
```

## Requirements

- **A VPS with a dedicated, static public IPv4 address.** The provider must not restrict which ports
  you can use and must allow UDP. Shared-IP "NAT VPS" plans will not work.
- Administrator / root on both the VPS and the game PC.
- An internet connection (WireGuard is downloaded on first run if absent).

The VPS and the game PC may run different operating systems (for example, a Linux VPS with a Windows
game PC). **A Linux VPS is recommended** — some Windows VPS images ship without WinNAT (see
[troubleshooting](docs/USAGE.md#invalid-class-error-0x80041010)).

|  | Windows | Linux |
|---|---|---|
| Privileges | `requireAdministrator` manifest → automatic UAC | re-executes itself with `sudo` when not root |
| WireGuard install | official MSI + Authenticode signature verification | distro package (apt/dnf/yum/zypper/pacman/apk) |
| VPS NAT | WinNAT (`New-NetNat`) | iptables MASQUERADE + DNAT (via wg-quick PostUp/PostDown) |
| Tunnel | WireGuard tunnel service | `wg-quick@<name>` (systemd) |
| Firewall | Windows Firewall | ufw / firewalld (when active) |

> If `vps-setup` fails on Windows with **"Invalid class" (0x80041010)**, the cause is one of: a 32-bit
> build or 32-bit PowerShell, a polluted `PSModulePath`, a missing WinNAT, or a corrupted WMI
> repository. **On 64-bit Windows, use the x86_64 build.** `vps-check` diagnoses this before
> installing anything — see the [usage guide](docs/USAGE.md#invalid-class-error-0x80041010).

## GUI (SDL3)

`tgui-gui` exposes the same functionality as buttons. The actual work is still done by the
`tgui` CLI in the same folder, and its output is streamed live into the **Log** panel.

- **Open ports panel** — checkboxes for the ARK defaults (7777 game, 7778 raw UDP, 27015 query,
  27020 RCON) plus free-form UDP/TCP fields that accept lists and ranges (up to 256 ports). Counts
  and syntax errors appear as you type, alongside the ports currently applied in `settings.conf`.
- **VPS tab** — pre-flight check → install/re-apply, status, show ports, uninstall.
- **Game PC tab** — browse for the `.conf` file → install config, tunnel up/down, status, uninstall.
- **Verify tab** — A2S server query and Steam master server list lookup.
- Privileges: on Windows the GUI runs elevated via UAC; on Linux each action authenticates through
  `pkexec` when not running as root.
- The only external dependencies are SDL3 and SDL3_ttf (with bundled FreeType) from the official
  libsdl-org repositories, fetched only when building with `-DTGUI_BUILD_GUI=ON`. **The CLI itself
  has no dependencies.**

## Languages

The GUI ships in **English (default) and Korean**. Every string lives in `lang/<code>.json`
next to the executable; English is also compiled into the binary, so the program still runs
correctly if that folder is missing or damaged.

- Switch language with the button in the top-right corner. The choice is remembered.
- Or force one: `tgui-gui --lang ko`, or set `TGUI_LANG=ko`.

**Adding a language needs no C code.** Copy `lang/en.json`, translate the values, and save it
as `lang/<code>.json` — the GUI picks it up on the next start, and `_name` is what the language
button shows.

One rule: **keep the `%s` / `%d` placeholders exactly as English has them, in the same order.**
This is a C program, so a swapped or missing placeholder would be undefined behaviour. Entries
that do not match are ignored at load time and the English text is used instead, and CI rejects
them outright. Check your file before opening a pull request:

```bash
python3 tools/check_lang.py
```

The CLI's own messages are still Korean only; translating those is tracked separately.

## Documentation

- [Build guide (Linux / Windows)](docs/BUILD.md)
- [Usage guide (running the built binaries, troubleshooting)](docs/USAGE.md)

## Building (CMake)

```bash
# Native Linux binary
cmake -S . -B build -G Ninja && cmake --build build            # -> build/tgui

# Cross-compile a Windows .exe from Linux (requires mingw-w64)
cmake -S . -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake
cmake --build build-win                                         # -> build-win/tgui.exe

# Native Windows build (MSYS2 mingw64 or Visual Studio)
cmake -S . -B build && cmake --build build --config Release
```

Add `-DTGUI_BUILD_GUI=ON` to build the GUI as well — see the
[build guide](docs/BUILD.md#building-the-gui-sdl3).

CI (GitHub Actions) builds and verifies Linux native, Linux→Windows cross-compilation
(x86_64 and i686), Windows MinGW and MSVC, a Windows end-to-end run on a real runner, and the GUI on
both Linux and Windows. Built executables are uploaded as artifacts.

## Usage

Run it with no arguments for an interactive menu, or pass a command for scripted use.
(Windows: `tgui.exe`, Linux: `tgui`.)

| Command | Original script | Description |
|---|---|---|
| `install-wireguard` | (new) | Install WireGuard (only if missing) |
| `vps-check` | (new) | **Pre-flight check**: WinNAT/WMI on Windows, iptables/interfaces on Linux. Changes nothing |
| `vps-setup` | vps-setup.ps1 | Server tunnel, IP forwarding, NAT (SNAT + DNAT), firewall, generates `client-gamepc.conf` |
| `vps-status` | (new) | Tunnel, NAT and firewall status |
| `ports` | (new) | Show open ports (saved settings + what is actually applied); `-udp`/`-tcp` applies changes immediately |
| `vps-uninstall` | vps-uninstall.ps1 | `-purge` `-disable-routing` `-force` |
| `gamepc-install` | gamepc-tunnel -Action Install | Install `client-gamepc.conf` + firewall rules |
| `gamepc-up` / `gamepc-down` / `gamepc-status` | gamepc-tunnel -Action Up/Down/Status | Waits for the handshake, confirms the egress IP |
| `gamepc-uninstall` | gamepc-uninstall.ps1 | `-keep-config` `-force` |
| `verify` | verify.ps1 | `-target IP` for an A2S_INFO query, `-steam-key KEY` to check the master server list |

Common options: `-wg-port -udp 7777,7778,27015 -tcp ... -subnet -server-ip -client-ip -dns -mtu
-public-ip -regenerate -wg-version X.Y.Z -no-auto-install -ext-iface <name>` (Linux).
Both `-name value` and `--name=value` are accepted. Settings and keys are stored in
`C:\ProgramData\TunnelingGUI` on Windows and `/etc/tgui` (mode 0700) on Linux.
`client-gamepc.conf` is readable on either platform, whichever one generated it.

```
tgui vps-check
tgui vps-setup -public-ip 203.0.113.5 -tcp 27020
tgui gamepc-install -config client-gamepc.conf
tgui gamepc-up
tgui verify -target 10.8.0.2
```

## Source layout (`src/`)

- **Platform-independent**: `main.c` entry point · `cli.c`/`menu.c` option and menu handling ·
  `cmd_*.c` one file per command · `wireguard.c` keys and handshakes · `http.c` · `fsutil.c` ·
  `textutil.c` · `ui.c` · `config.c`
- **Platform layer** (same headers, different implementations): `win/` (WinHTTP, services, WinNAT,
  PowerShell, console API) · `posix/` (curl, wg-quick, iptables, ufw/firewalld, fork/exec)
- Each `.h` lists its public functions; `tgui.h` includes them all. To add a new OS, implement the
  counterparts of the files in `win/`.
- **GUI**: `gui/` (`main.c` screens, `ui.c` widgets, `runner.c`/`spawn_*.c` CLI execution,
  `ports.c`, `fonts.c`)
- **Tests**: `tests/linux/run.sh` replaces the kernel and network tools with stubs and exercises the
  full vps/gamepc flow; `tests/gui/run.sh` drives the GUI off-screen into the CLI.

## Dependencies and security

- **No external libraries.** Windows: Win32/WinHTTP/WinTrust/Winsock/IP Helper. Linux: libc plus the
  system tools `wg`, `wg-quick`, `iptables`, `ip` and `curl`.
- The only downloads are from the official WireGuard distribution (the Windows MSI, with mandatory
  Authenticode verification) or from your distribution's own package repositories.
- The private key directory is restricted to Administrators/root (SID-based ACLs on Windows,
  0700/0600 on Linux).
- All user input that reaches an external command (IPs, ports, interface names, API keys, …) is
  validated first.

## Verification status

- **Linux**: `tests/linux/run.sh` exercises vps-check/setup/status/uninstall and the entire gamepc
  flow in CI, with kernel WireGuard and iptables calls replaced by stubs. Real kernel NAT behaviour
  still needs to be confirmed on an actual VPS.
- **Windows**: CI confirms MinGW and MSVC builds and runs `help`/`vps-check`. Try the real tunnel and
  NAT setup on an actual Windows VPS and game PC first.

## License

[MIT](LICENSE)
