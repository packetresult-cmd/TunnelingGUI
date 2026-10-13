#!/usr/bin/env bash
# GUI 테스트 (Linux, 화면 없이): 오프스크린 드라이버로 GUI 를 띄워 CLI 를 실행시키고
# 결과(설정 파일)와 화면 캡처(PNG)를 확인한다. 커널/네트워크 도구는 tests/linux/stubs.sh 의 가짜를 쓴다.
#   sudo tests/gui/run.sh build/tgui-gui [캡처 저장 폴더]
set -u
GUI=$(readlink -f "${1:?사용법: $0 <tgui-gui> [출력 폴더]}")
OUT=$(readlink -f "${2:-.}")
BIN=$(dirname "$GUI")/tgui           # GUI 는 같은 폴더의 CLI 를 실행한다
. "$(cd "$(dirname "$0")/../linux" && pwd)/stubs.sh"
mkdir -p "$OUT"

GUILOG=$T/gui.log
# Keep the GUI's own output: when it exits early (no font, no video driver, ...)
# this is the only place that says why.
gui() {
  PATH="$T/bin:$PATH" LD_PRELOAD="$T/shim.so" SDL_VIDEO_DRIVER=offscreen SDL_RENDER_DRIVER=software \
    timeout 120 "$GUI" "$@" >>"$GUILOG" 2>&1
  rc=$?
  if [ $rc -ne 0 ]; then
    echo "  (gui exited $rc, output follows)"
    tail -20 "$GUILOG" | sed 's/^/    /'
    [ -n "${GITHUB_ACTIONS:-}" ] && echo "::error::gui exited $rc: $(tail -3 "$GUILOG" | tr '\n' ' ')"
  fi
  return $rc
}

echo "== environment"
echo "  gui   : $GUI"
echo "  cli   : $BIN ($([ -x "$BIN" ] && echo found || echo MISSING))"
echo "  lang  : $(ls "$(dirname "$GUI")/lang" 2>/dev/null | tr '\n' ' ')"
echo "  fonts : $(fc-list 2>/dev/null | grep -ciE 'nanum|noto sans cjk') CJK font(s)"

echo "== GUI 로 vps-setup 실행 (범위 포트)"
gui --exec "vps-setup -public-ip 203.0.113.5 -udp 7777-7790,27015 -tcp -" --screenshot "$OUT/gui-vps-setup.png"
check "GUI 종료 코드 0" "[ $? -eq 0 ]"
check "서버 설정에 범위 DNAT" 'grep -q -- "--dport 7777:7790 -j DNAT --to-destination 10.8.0.2$" /etc/wireguard/tgui-gw.conf'
check "settings.conf 저장" 'grep -q "^udp=7777-7790,27015$" /etc/tgui/settings.conf'
check "캡처 PNG 생성" '[ -s "$OUT/gui-vps-setup.png" ]'

echo "== GUI 로 ports 실행, 저장된 포트가 화면에 반영"
gui --exec "ports" --screenshot "$OUT/gui-ports.png"
check "캡처 PNG 생성" '[ -s "$OUT/gui-ports.png" ]'

echo "== 각 탭 렌더링"
for t in 0 1 2; do
  gui --demo --tab $t --screenshot "$OUT/gui-tab$t.png"
  check "탭 $t 렌더링" '[ -s "$OUT/gui-tab$t.png" ]'
done

gui --exec "vps-uninstall -force -purge"
check "GUI 로 제거" '[ ! -e /etc/wireguard/tgui-gw.conf ] && [ ! -e /etc/tgui ]'

[ $fail -eq 0 ] && echo "ALL PASSED" || { echo "SOME FAILED"; exit 1; }
