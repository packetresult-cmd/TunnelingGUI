#!/usr/bin/env bash
# Linux 회귀 테스트: 커널/네트워크를 건드리지 않고 vps-setup / gamepc-* 흐름을 검증한다.
#   sudo tests/linux/run.sh path/to/tgui
# 필요: root, wireguard-tools(wg), gcc.  /etc/tgui, /etc/wireguard/tgui*.conf, /etc/sysctl.d/99-tgui.conf 를 만들고 지운다.
set -u
BIN=$(readlink -f "${1:?사용법: $0 <tgui 바이너리>}")
HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/stubs.sh"

echo "== vps-check"
out=$(run vps-check); check "vps-check 통과" '[ $? -eq 0 ] && echo "$out" | grep -q "외부 인터페이스: eth0"'

echo "== vps-setup"
out=$(run vps-setup -public-ip 203.0.113.5 -udp 7777,27015 -tcp 27020); rc=$?
check "종료 코드 0" "[ $rc -eq 0 ]"
check "서버 설정 /etc/wireguard/tgui-gw.conf (0600)" '[ "$(stat -c %a /etc/wireguard/tgui-gw.conf)" = 600 ]'
check "MASQUERADE PostUp" 'grep -q "PostUp = iptables -t nat -A POSTROUTING -s 10.8.0.0/24 -o eth0 -j MASQUERADE" /etc/wireguard/tgui-gw.conf'
check "UDP DNAT 7777" 'grep -q -- "-p udp --dport 7777 -j DNAT --to-destination 10.8.0.2:7777" /etc/wireguard/tgui-gw.conf'
check "TCP DNAT 27020" 'grep -q -- "-p tcp --dport 27020 -j DNAT --to-destination 10.8.0.2:27020" /etc/wireguard/tgui-gw.conf'
check "PostDown 규칙 대칭" 'grep -q "PostDown = iptables -t nat -D POSTROUTING" /etc/wireguard/tgui-gw.conf'
check "설정에 CR 없음" '! grep -q $'"'"'\r'"'"' /etc/wireguard/tgui-gw.conf'
check "keys.json (0600)" '[ "$(stat -c %a /etc/tgui/keys.json)" = 600 ]'
check "client-gamepc.conf Endpoint" 'grep -q "Endpoint = 203.0.113.5:51820" /etc/tgui/client-gamepc.conf'
check "client-gamepc.conf AllowedIPs /1 두 개" 'grep -q "AllowedIPs = 0.0.0.0/1, 128.0.0.0/1" /etc/tgui/client-gamepc.conf'
check "sysctl 영구 설정" 'grep -q "net.ipv4.ip_forward = 1" /etc/sysctl.d/99-tgui.conf'
check "터널 기동 호출" 'grep -Eq "wg-quick up tgui-gw|systemctl restart wg-quick@tgui-gw" "$LOG"'
check "ufw 51820/udp 허용" 'grep -q "ufw allow 51820/udp" "$LOG"'
check "ufw 27020/tcp 허용" 'grep -q "ufw allow 27020/tcp" "$LOG"'
check "방화벽 기록 4건 (WireGuard + UDP 2 + TCP 1)" '[ "$(wc -l < /etc/tgui/firewall.rules)" -eq 4 ]'
PRIV1=$(grep -o "PrivateKey = .*" /etc/tgui/client-gamepc.conf)
run vps-setup -public-ip 203.0.113.5 >/dev/null
check "재실행 시 키 재사용" '[ "$PRIV1" = "$(grep -o "PrivateKey = .*" /etc/tgui/client-gamepc.conf)" ]'

echo "== 포트 설정 저장 / ports 명령"
check "settings.conf 저장 (0644)" '[ "$(stat -c %a /etc/tgui/settings.conf)" = 644 ] && grep -q "^udp=7777,27015$" /etc/tgui/settings.conf && grep -q "^tcp=27020$" /etc/tgui/settings.conf'
check "keys.json 은 여전히 0600" '[ "$(stat -c %a /etc/tgui/keys.json)" = 600 ]'
run vps-setup -public-ip 203.0.113.5 >/dev/null
check "옵션 없이 재실행해도 포트 유지" 'grep -q -- "--dport 27015 -j DNAT" /etc/wireguard/tgui-gw.conf && grep -q -- "-p tcp --dport 27020" /etc/wireguard/tgui-gw.conf'
out=$(run ports); rc=$?
check "ports 조회" '[ $rc -eq 0 ] && echo "$out" | grep -q "UDP       : 7777,27015" && echo "$out" | grep -q "UDP 27015 -> 10.8.0.2:27015" && echo "$out" | grep -q "TCP 27020 -> 10.8.0.2:27020"'
out=$(run ports -udp 7777-7790,27015-27016,30000 -tcp -); rc=$?
check "ports 로 범위 변경 적용" '[ $rc -eq 0 ] && grep -q -- "-p udp --dport 7777:7790 -j DNAT --to-destination 10.8.0.2$" /etc/wireguard/tgui-gw.conf'
check "범위 PostDown 대칭" 'grep -q -- "PostDown = iptables -t nat -D PREROUTING -i eth0 -p udp --dport 7777:7790" /etc/wireguard/tgui-gw.conf'
check "TCP 제거('-')" '! grep -q -- "-p tcp" /etc/wireguard/tgui-gw.conf && grep -q "^tcp=$" /etc/tgui/settings.conf'
check "settings 범위 표기" 'grep -q "^udp=7777-7790,27015,27016,30000$" /etc/tgui/settings.conf'
check "ufw 범위 문법(7777:7790/udp)" 'grep -q "ufw allow 7777:7790/udp" "$LOG"'
out=$(run ports); check "ports 범위 표시" 'echo "$out" | grep -q "UDP 7777-7790 -> 10.8.0.2"'
many=$(seq -s, 40000 2 40078)   # 40개, 연속 아님
out=$(run ports -udp "$many"); rc=$?
check "포트 40개 (3개 초과) 적용" '[ $rc -eq 0 ] && [ "$(grep -c "PostUp = iptables -t nat -A PREROUTING" /etc/wireguard/tgui-gw.conf)" -eq 40 ]'
out=$(run ports -udp 1-300); check "256개 초과 거부" '[ $? -eq 2 ]'
out=$(run ports -udp 7790-7777); check "거꾸로 된 범위 거부" '[ $? -eq 2 ]'
run ports -udp 7777,27015 -tcp 27020 >/dev/null

echo "== vps-status"
out=$(run vps-status); check "상태 출력" 'echo "$out" | grep -q "Running" && echo "$out" | grep -q "NAT (10.8.0.0/24) : 있음"'

cp /etc/tgui/client-gamepc.conf "$T/client.conf"
sed -i 's/$/\r/' "$T/client.conf"      # Windows 에서 만든 파일(CRLF) 흉내

echo "== vps-uninstall (같은 PC 에 게임 PC 터널이 없을 때)"
out=$(run vps-uninstall -force -purge -disable-routing); rc=$?
check "종료 코드 0" "[ $rc -eq 0 ]"
check "서버 설정 삭제" '[ ! -e /etc/wireguard/tgui-gw.conf ]'
check "설정 폴더 삭제(-purge)" '[ ! -e /etc/tgui ]'
check "sysctl 파일 삭제" '[ ! -e /etc/sysctl.d/99-tgui.conf ]'
check "ufw 규칙 삭제 호출" 'grep -q "ufw delete allow 51820/udp" "$LOG"'
check "터널 중지 호출" 'grep -Eq "wg-quick down tgui-gw|systemctl disable --now wg-quick@tgui-gw" "$LOG"'

echo "== gamepc"
out=$(run gamepc-install -config "$T/client.conf" -udp 7777); rc=$?
check "gamepc-install 종료 코드 0" "[ $rc -eq 0 ]"
check "CR 제거됨" '! grep -q $'"'"'\r'"'"' /etc/tgui/tgui.conf'
check "ufw 7777/udp 허용" 'grep -q "ufw allow 7777/udp" "$LOG"'
out=$(run gamepc-up); rc=$?
check "gamepc-up 성공(출구 IP == VPS IP)" '[ $rc -eq 0 ] && echo "$out" | grep -q "성공"'
check "/etc/wireguard/tgui.conf (0600)" '[ "$(stat -c %a /etc/wireguard/tgui.conf)" = 600 ]'
out=$(run gamepc-status); check "gamepc-status Running" 'echo "$out" | grep -q "Running"'
out=$(run gamepc-down); check "gamepc-down" '[ $? -eq 0 ] && [ ! -e /etc/wireguard/tgui.conf ]'
out=$(run gamepc-uninstall -force); check "gamepc-uninstall" '[ $? -eq 0 ] && [ ! -e /etc/tgui ]'

echo "== 입력 검증"
out=$(run vps-setup -udp abc); check "잘못된 포트 거부" '[ $? -eq 2 ]'
out=$(run vps-setup -ext-iface "eth0;rm"); check "ext-iface 인젝션 거부" '[ $? -eq 2 ]'

[ $fail -eq 0 ] && echo "ALL PASSED" || { echo "SOME FAILED"; exit 1; }
