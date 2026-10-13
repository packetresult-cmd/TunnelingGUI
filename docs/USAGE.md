# Usage Guide (`tgui` / `tgui.exe`)

A WireGuard full-tunnel tool that lets you run a game server on your home PC while the world sees it
at your **VPS's public IP address**.

```
[players] ──► VPS public IP ──(WireGuard 10.8.0.0/24)──► game PC (10.8.0.2)
                              all IPv4 traffic from the game PC also exits via the VPS
```

- The VPS and the game PC can each be **Windows or Linux**, and they do not have to match. Every
  command in this guide is identical on both; only the executable name differs
  (`tgui.exe` on Windows, `tgui` on Linux).
- Administrator privileges are required. Windows raises a UAC prompt automatically; on Linux the
  program re-executes itself with `sudo` when it is not running as root.
- If WireGuard is missing, the program **installs it automatically** (official MSI with signature
  verification on Windows, distribution package on Linux). An internet connection is required.
- **A Linux VPS is recommended.** Windows VPS images that lack WinNAT fail with an
  `Invalid class` error — see [troubleshooting](#invalid-class-error-0x80041010).
- For build instructions, see [BUILD.md](BUILD.md).

## Using the GUI (`tgui-gui`)

Running `tgui-gui`(`.exe`) gives you the same functionality as buttons. Keep it **in the same
folder as the `tgui` CLI**.

1. Choose the ports to open in the **Open ports** panel on the right: the default checkboxes
   (7777 / 7778 / 27015, plus RCON 27020) and the extra UDP/TCP fields
   (for example `7779-7782,27016-27017`). Counts and syntax errors appear as you type.
2. **VPS tab** (on the VPS): `1. Pre-flight check` → `2. Install / re-apply`. When it finishes, the
   log shows where the game PC's configuration file was written.
3. **Game PC tab** (on the game PC): `Browse` for `client-gamepc.conf` → `1. Install config` →
   `2. Tunnel up`.
4. To change ports later, edit them and press `Apply port changes only` — on **both** the VPS and
   the game PC.
5. **Verify tab**: run the A2S server query and check registration in the Steam server list.

Privileges: Windows shows a UAC prompt at launch. On Linux, each action raises a `pkexec`
authentication dialog unless you are root (or run `sudo tgui-gui`).
The uninstall buttons ask for confirmation first. The `Copy` button puts the log on your clipboard —
paste it when reporting a problem.

## Language

The GUI starts in English. Use the button in the top-right corner to switch to another
language; the choice is saved per user and restored next time.

To force a language, pass `--lang ko` or set the `TGUI_LANG` environment variable. Language
files are `lang/<code>.json` next to the executable — see the README for how to add one.

Note that the CLI (`tgui`) still prints its own messages in Korean, so the log panel shows
Korean output even when the GUI is in English.

## Running it

- **Menu mode**: double-click `tgui.exe` (no arguments). Pick a number and it prompts for the
  values it needs.
- **Command mode**: `tgui.exe <command> [options]` from an elevated PowerShell or cmd.
  Suited to scripting and automation.
- `tgui.exe help` lists every command and option. A window opened by double-clicking waits
  for Enter before closing.
- Exit codes: `0` success, `1` failure, `2` bad arguments.

## Full walkthrough

### Step 1 — On the VPS

```
tgui vps-check      # pre-flight check (changes nothing). Always run this first on a Windows VPS
tgui vps-setup
```

This sets up the WireGuard server tunnel (`tgui-gw`, 10.8.0.1), IP forwarding, NAT (SNAT plus DNAT
of the game ports to 10.8.0.2), firewall rules, key generation, and writes the game PC's
configuration file.

Windows uses WinNAT; Linux uses iptables (`MASQUERADE` + `DNAT`). On Linux the rules live in the
tunnel's `PostUp`/`PostDown`, so they are restored automatically after a reboot and removed when you
uninstall.

If the outbound interface is not detected on Linux, specify it: `-ext-iface eth0`.

The public IP is detected automatically from the Azure metadata service. If that fails, pass it
explicitly: `-public-ip 203.0.113.5`.
To change ports: `-udp 7777,7778,27015 -tcp 27020 -wg-port 51820`

Next, open UDP `51820` and your game's UDP ports (7777, 7778 and 27015 by default) in your cloud
firewall — **Azure portal → VM → Networking → Inbound port rules**, AWS security groups, and so on.
The program prints the list again when it finishes.

Generated file: `C:\ProgramData\TunnelingGUI\client-gamepc.conf` on Windows,
`/etc/tgui/client-gamepc.conf` on Linux. **It contains a private key.**

### Step 2 — Move the configuration file to the game PC

Use a secure channel such as RDP clipboard/drive redirection or `scp`. Avoid messaging apps and
email attachments, and delete any temporary copies afterwards.

### Step 3 — On the game PC

```
tgui.exe gamepc-install -config C:\path\to\client-gamepc.conf   # once
tgui.exe gamepc-up
```

`gamepc-up` brings the tunnel up, waits up to 30 seconds for a handshake, and then confirms that
**the egress IP matches the VPS IP**. Start your game server only after you see the success message.

If the handshake never arrives, check UDP 51820 in your cloud firewall and confirm that `vps-setup`
completed on the VPS. If you lose internet connectivity, `gamepc-down` restores it.

For ARK, add `MultiHome=10.8.0.2` to the server's launch arguments so that it binds to the tunnel
address. (Restore the original value when uninstalling.)

### Step 4 — Verify

```
# From the VPS: checks the tunnel, the game PC's firewall and the server query
tgui.exe verify -target 10.8.0.2

# From any outside machine: checks the cloud firewall and DNAT (+ Steam server list registration)
tgui.exe verify -target <VPS public IP>
tgui.exe verify -target <VPS public IP> -steam-key <Steam Web API key>
```

A server can take 5–10 minutes to appear in the Steam list after it starts. The query port defaults
to 27015 (`-query-port` to change it).

## Choosing which ports to open (UDP/TCP)

Set the ports with `-udp` / `-tcp`. **Once set, they are saved to `settings.conf` and reused on
later runs.** The limit is 256 ports per protocol, and ranges such as `7777-7790` are accepted.

| Purpose (ARK defaults) | Port | Protocol |
|---|---|---|
| Game | 7777 | UDP |
| Raw UDP (game port + 1) | 7778 | UDP |
| Server list query | 27015 | UDP |
| RCON (optional) | 27020 | TCP |

Running several servers (a cluster) means giving each one different ports and opening all of them.
For example, three servers: `-udp 7777-7782,27015-27017`.

```
tgui ports                                  # show saved settings + the forwarding/firewall rules actually in place
tgui ports -udp 7777-7782,27015-27017       # apply immediately (VPS: rebuild with the same keys; game PC: firewall only)
tgui ports -tcp 27020                       # add TCP ports ('-' means none)
```

When you change ports you must open the same ports on **both the VPS and the game PC**, and in your
cloud firewall.

## Day-to-day use

| Goal | Command |
|---|---|
| Bring the tunnel up before starting the server | `gamepc-up` |
| Take it down when not hosting (back to your home connection) | `gamepc-down` |
| Status (service, handshake, egress IP) | `gamepc-status` |
| VPS status (tunnel, NAT, firewall) | `vps-status` |

Once enabled, the tunnel service comes back automatically after a reboot.

## Uninstalling (order matters)

1. **Game PC first**: `tgui.exe gamepc-uninstall` (`-keep-config` keeps the configuration,
   `-force` skips the prompts)
2. Restore the original `MultiHome` value in your server's launch arguments
3. **Then the VPS**: `tgui.exe vps-uninstall` (`-purge` deletes keys and settings,
   `-disable-routing` turns IP forwarding off)
4. Remove the cloud firewall inbound rules yourself

Removing the VPS first cuts the game PC's internet connection. The program warns you and asks for
confirmation.

## Option reference

| Option | Default | Used by |
|---|---|---|
| `-config <path>` | — | gamepc-install |
| `-public-ip <IP>` | auto-detected | vps-setup |
| `-wg-port <N>` | 51820 | vps-setup |
| `-udp <list>` / `-tcp <list>` | 7777,7778,27015 / none | vps-setup, gamepc-install, ports (ranges like `7777-7790`, up to 256, `-` means none, value is saved) |
| `-subnet` `-server-ip` `-client-ip` | 10.8.0.0/24, .1, .2 | vps-setup |
| `-dns` `-mtu` | 1.1.1.1, 1420 | vps-setup |
| `-regenerate` | — | vps-setup (new keys; the game PC config must be reinstalled) |
| `-purge` `-disable-routing` | — | vps-uninstall |
| `-keep-config` | — | gamepc-uninstall |
| `-force` | — | the uninstall commands (skip confirmation) |
| `-target` `-query-port` `-steam-key` `-timeout` | — / 27015 / — / 3000 ms | verify |
| `-wg-version X.Y.Z` | latest | install-wireguard and automatic installation |
| `-no-auto-install` | — | disables automatic WireGuard installation |

Both `-name value` and `--name=value` are accepted.

## File locations

| Item | Location |
|---|---|
| Keys, settings | Windows `C:\ProgramData\TunnelingGUI\` (Administrators/SYSTEM only) · Linux `/etc/tgui/` (0700) |
| VPS tunnel | Windows service `WireGuardTunnel$tgui-gw` · Linux `wg-quick@tgui-gw`, `/etc/wireguard/tgui-gw.conf` |
| Game PC tunnel | Windows service `WireGuardTunnel$tgui` · Linux `wg-quick@tgui`, `/etc/wireguard/tgui.conf` |
| NAT / firewall | Windows: WinNAT `TunnelingGUI`, firewall group `TunnelingGUI` · Linux: iptables rules (inside the tunnel config), `/etc/tgui/firewall.rules` (ufw/firewalld record), `/etc/sysctl.d/99-tgui.conf` |
| WireGuard | Windows `C:\Program Files\WireGuard\` · Linux distribution package `wireguard-tools` |

The names and paths match the original PowerShell scripts, so the two can be used interchangeably.

## Troubleshooting

### Invalid class (error 0x80041010)

If `vps-setup` or `vps-check` reports `Invalid class`, PowerShell could not find the WMI class it
needs (`MSFT_NetNat` and friends).

**If `Get-NetNat` works fine in an elevated PowerShell on the same machine but this program still
fails, the problem is the execution environment, not a missing Windows feature.** Likely causes, in
order:

1. **A 32-bit process or 32-bit PowerShell.** On 64-bit Windows the NetNat/NetTCPIP WMI providers
   are 64-bit only, so a 32-bit PowerShell raises this error. With a 32-bit (`i686`) build,
   `System32` is redirected to `SysWOW64`, which is what launched the 32-bit PowerShell.
   **The current version launches 64-bit PowerShell (`Sysnative`) even from a 32-bit process**, but
   you should still **use the x86_64 build on 64-bit Windows.**
   To confirm, in an elevated PowerShell:
   `& "$env:windir\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" -Command Get-NetNat`
   If that reports `Invalid class` while a plain `Get-NetNat` succeeds, this is your cause.
2. **A polluted `PSModulePath`.** Launching from PowerShell 7 or MSYS2 passes the parent shell's
   module path down to PowerShell 5.1. The current version resets it to the system default before
   invoking PowerShell.
3. **This Windows genuinely has no WinNAT** — an edition or VPS image without the feature that
   provides NAT (Hyper-V and friends). In that case `Get-NetNat` fails the same way even in an
   elevated PowerShell.
   - Install Hyper-V and reboot (Server: `Install-WindowsFeature Hyper-V -IncludeManagementTools`;
     Windows 10/11: `Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V-All`)
   - Or run the same commands on a **Linux VPS** (`tgui vps-check` → `vps-setup`), which uses
     iptables and needs no WinNAT.
4. **A corrupted WMI repository.** Last resort — this resets WMI data. In an elevated cmd:
   `net stop winmgmt /y` → `winmgmt /salvagerepository` → `winmgmt /resetrepository` → reboot.

When it fails, `vps-check` also prints **diagnostics**: the PowerShell version, whether the process
is 64-bit, and the raw error message. If the cause is not obvious, include that output when asking
for help.

If an earlier version of `vps-setup` failed partway through, some of the configuration (tunnel,
forwarding) is already applied. Run `tgui.exe vps-uninstall -force` to roll back, then try
again.

### Linux

- **`cannot use the iptables nat table`** — restricted VPS types such as containers or OpenVZ may
  lack the NAT modules. Use a KVM-based VPS.
- **WireGuard kernel module** — older kernels (below 5.6) may need `wireguard-dkms`. Check with
  `modprobe wireguard`.
- **Interface auto-detection failed** — specify it with `-ext-iface eth0`.
- **DNS on a Linux game PC** — if `resolvconf` is absent, the `DNS` line is dropped from the
  configuration and the system resolver is used instead.
- Without systemd (containers and similar) the tunnel is brought up directly through `wg-quick`, so
  it does not start automatically after a reboot.

### General

- **No handshake** — check UDP 51820 in the cloud firewall, then `vps-status` on the VPS and
  `gamepc-status` on the game PC.
- **Egress IP does not match the VPS** — check whether the VPS public IP changed (a static IP is
  strongly recommended). Do not start the server in this state.
- **`verify` gets no response** — a failure against `10.8.0.2` points at the tunnel, the game PC's
  firewall, or the game's query port. A failure against the VPS public IP points at the cloud
  firewall, the VPS firewall, or the NAT mapping.
- **`vps-setup` stops because another NAT exists (Windows)** — WinNAT supports only one. Remove the
  existing NAT and run it again.
- **SmartScreen or antivirus warnings** — expected for an unsigned tool that requires administrator
  privileges. Verify that you built the binary yourself.
- **Automatic WireGuard installation failed** — check your internet connection and proxy settings,
  or install manually from https://www.wireguard.com/install/ and run the command again.
  Installation is aborted if signature verification fails.

## Security notes

- `client-gamepc.conf` and `keys.json` contain private keys. Do not share them or commit them to a
  repository.
- Regenerating keys (`-regenerate`) invalidates every existing game PC configuration.
