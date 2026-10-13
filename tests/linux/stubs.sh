# tests/linux/stubs.sh - 테스트 공용: 커널/네트워크 도구를 가짜로 대체하는 환경 (source 해서 쓴다)
# 필요한 변수: BIN (tgui 경로). 정의하는 것: T(임시 폴더) LOG(호출 기록) run() check() ok() bad() cleanup_state
# 이 파일의 위치(HERE)를 기준으로 ifindex_shim.c 를 찾는다.
STUBS_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
T=$(mktemp -d)
LOG=$T/calls.log; : > "$LOG"
IFDIR=$T/ifs; mkdir -p "$IFDIR" "$T/bin"
REAL_WG=$(command -v wg) || { echo "wg 가 필요합니다"; exit 1; }
fail=0
ok()   { echo "  ok   - $1"; }
# On GitHub Actions also emit an annotation, so a failure is visible without
# digging through the raw log.
bad()  { echo "  FAIL - $1"; [ -n "${GITHUB_ACTIONS:-}" ] && echo "::error::$1"; fail=1; }
check(){ if eval "$2"; then ok "$1"; else bad "$1"; fi; }

cleanup() { rm -rf /etc/tgui /etc/wireguard/tgui.conf /etc/wireguard/tgui-gw.conf /etc/sysctl.d/99-tgui.conf "$T"; }
trap cleanup EXIT
cleanup_state() { rm -rf /etc/tgui /etc/wireguard/tgui.conf /etc/wireguard/tgui-gw.conf /etc/sysctl.d/99-tgui.conf; }
cleanup_state

gcc -shared -fPIC -o "$T/shim.so" "$STUBS_DIR/ifindex_shim.c" || exit 1

# ---- 가짜 도구들 (호출을 기록만 한다)
mk() { printf '#!/bin/sh\n%s\n' "$2" > "$T/bin/$1"; chmod +x "$T/bin/$1"; }
mk wg-quick  'echo "wg-quick $*" >> "$TGUI_LOG"; case "$1" in up) : > "$TGUI_FAKE_IF_DIR/$2";; down) rm -f "$TGUI_FAKE_IF_DIR/$2";; esac'
mk systemctl 'echo "systemctl $*" >> "$TGUI_LOG"; for a in "$@"; do case "$a" in wg-quick@*) n=${a#wg-quick@};; esac; done
  case "$*" in *restart*) : > "$TGUI_FAKE_IF_DIR/$n";; *disable*) rm -f "$TGUI_FAKE_IF_DIR/$n";; esac'
mk iptables  'echo "iptables $*" >> "$TGUI_LOG"; case "$*" in
  "-t nat -S POSTROUTING") echo "-A POSTROUTING -s 10.8.0.0/24 -o eth0 -j MASQUERADE";;
  "-t nat -S PREROUTING") [ -f /etc/wireguard/tgui-gw.conf ] && sed -n "s/^PostUp = iptables -t nat -A PREROUTING /-A PREROUTING /p" /etc/wireguard/tgui-gw.conf;;
esac'
mk sysctl    'echo "sysctl $*" >> "$TGUI_LOG"'
mk ip        'echo "default via 10.0.0.1 dev eth0 proto dhcp"'
mk curl      'echo "curl $*" >> "$TGUI_LOG"; echo 203.0.113.5'
mk ufw       'echo "ufw $*" >> "$TGUI_LOG"; case "$1" in status) echo "Status: active";; esac'
mk wg        'if [ "$1" = show ]; then printf "PUBKEY\t%s\n" "$(date +%s)"; else exec '"$REAL_WG"' "$@"; fi'

export TGUI_LOG="$LOG" TGUI_FAKE_IF_DIR="$IFDIR"
run() { PATH="$T/bin:$PATH" LD_PRELOAD="$T/shim.so" "$BIN" "$@" 2>&1; }

