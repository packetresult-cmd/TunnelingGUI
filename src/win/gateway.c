/* win/gateway.c
 * Windows 게이트웨이 구성: 사전 점검, IP 포워딩, WinNAT(SNAT + 포트 DNAT), Windows 방화벽.
 *
 * NAT/방화벽 cmdlet 은 WMI 를 쓴다. WinNAT 이 없는 Windows 에서는 Get-NetNat 이
 * "잘못된 클래스입니다 (Invalid class, 0x80041010)" 로 실패하므로, 설치를 시작하기 전에
 * gateway_preflight() 에서 미리 확인하고 원인과 대안을 알려준다.
 */
#include "tgui.h"

/* ------------------------------------------------------------------ 사전 점검 */

/* 각 WMI 기반 cmdlet 이 동작하는지 하나씩 확인. 오류 문구(로캘)에 의존하지 않고 HRESULT/CimException 으로 판별한다.
 * "없음(NotFound)" 오류는 클래스가 정상이라는 뜻이므로 OK 로 센다. */
static const char PREFLIGHT_PS[] =
    "function T($n,[scriptblock]$b){try{& $b | Out-Null;\"$n=OK\"}catch{"
    "$m=\"$($_.FullyQualifiedErrorId) $($_.Exception.Message)\";"
    "if($m -match '80041010|InvalidClass|Invalid class'){\"$n=INVALID_CLASS\"}"
    "elseif($m -match 'NotFound|ObjectNotFound|No MSFT_'){\"$n=OK\"}"
    "else{\"$n=ERROR\"};"
    "if($m -notmatch 'NotFound|ObjectNotFound|No MSFT_'){\"Msg_$n=$($_.Exception.Message) [$($_.FullyQualifiedErrorId)]\"}}};"
    "\"Diag=PS $($PSVersionTable.PSVersion) / 64bit-process=$([Environment]::Is64BitProcess) / 64bit-OS=$([Environment]::Is64BitOperatingSystem) / "
    "arch=$env:PROCESSOR_ARCHITECTURE,$env:PROCESSOR_ARCHITEW6432\";"
    "T 'NetNat' {Get-NetNat};"
    "T 'NetFirewall' {Get-NetFirewallRule -DisplayName 'tgui-preflight-none'};"
    "T 'NetRoute' {Get-NetRoute -AddressFamily IPv4 -DestinationPrefix '0.0.0.0/0'};"
    "T 'NetIPInterface' {Get-NetIPInterface -AddressFamily IPv4};"
    "try{$os=Get-CimInstance Win32_OperatingSystem;$ed=(Get-ItemProperty 'HKLM:\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion').EditionID;"
    "\"OS=$($os.Caption) / EditionID=$ed / NetNat.dll=$(Test-Path (Join-Path $env:windir 'System32\\wbem\\NetNat.dll'))\"}catch{'OS=unknown'};"
    "try{$w=Get-CimInstance Win32_SystemDriver -Filter \"Name='winnat'\" -ErrorAction Stop;"
    "if($w){\"WinNat=$($w.State)/$($w.StartMode)\"}else{'WinNat=missing'}}catch{'WinNat=unknown'};"
    "$o=@(Get-NetNat -ErrorAction SilentlyContinue | Where-Object Name -ne '" NAT_NAME "');"
    "if($o.Count){\"OtherNat=\"+($o.Name -join ',')}";

/* 점검 출력에서 "Diag=" / "Msg_" 줄을 그대로 보여준다 (사용자가 붙여넣어 문의할 수 있게) */
static void print_diag(const char *out)
{
    char line[512];
    const char *p = out;
    pf("  진단 정보:\n");
    pf("    tgui: %d비트 빌드\n", (int)(sizeof(void *) * 8));
    while (*p) {
        size_t n = strcspn(p, "\r\n");
        if (n >= sizeof line) n = sizeof line - 1;
        memcpy(line, p, n);
        line[n] = 0;
        if (!strncmp(line, "Diag=", 5)) pf("    %s\n", line + 5);
        else if (!strncmp(line, "WinNat=", 7)) pf("    WinNAT 드라이버(winnat): %s\n", line + 7);
        else if (!strncmp(line, "OS=", 3)) pf("    %s\n", line + 3);
        else if (!strncmp(line, "Msg_", 4)) pf("    %s\n", line);
        p += n;
        while (*p == '\r' || *p == '\n') p++;
    }
}

static BOOL g_netnat_query_broken;   /* Get-NetNat(조회)만 실패하는 PC: New-NetNat 단계에서 판정 */

static void print_wmi_help(const char *which, const char *out)
{
    say(C_YELLOW,
        "\nWMI 클래스(%s)를 쓸 수 없습니다. (오류 0x80041010 \"잘못된 클래스입니다\")\n"
        "  NAT 생성(New-NetNat)까지 실패하면, 이 Windows 에는 WinNAT 관리 기능이 아직 켜져 있지 않은 것입니다.\n"
        "  (원본 PowerShell 스크립트도 같은 New-NetNat 단계에서 실패합니다)\n"
        "  해결 방법:\n"
        "   1) 기능 켜기 → 재부팅 → 다시 vps-setup:   tgui.exe winnat-enable\n"
        "      (Windows 11/10 Pro·Enterprise: Hyper-V, Home: HypervisorPlatform, Server: Hyper-V 역할)\n"
        "   2) 또는 Linux VPS 에서 같은 프로그램으로 vps-setup (iptables 기반이라 이 기능이 필요 없음)\n"
        "   3) 1) 이후에도 안 되면 WMI 저장소 복구(마지막 수단): net stop winmgmt /y && winmgmt /salvagerepository && winmgmt /resetrepository → 재부팅\n",
        which);
    print_diag(out);
}

/* WinNAT 관리 기능(MSFT_NetNat)을 제공하는 Windows 기능을 켠다. 재부팅이 필요하다.
 * Microsoft Q&A: New-NetNat 의 Invalid class 는 Hyper-V 가 켜져 있지 않을 때 난다.
 * Home 에디션은 Hyper-V 가 없어 HypervisorPlatform 을 켠다 (사용자 보고 기준, Microsoft 공식 안내는 아님). */
int gateway_enable_nat_feature(void)
{
    char out[2048];
    if (!ps("$os=Get-CimInstance Win32_OperatingSystem;"
            "$ed=(Get-ItemProperty 'HKLM:\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion').EditionID;"
            "if($os.ProductType -ne 1){"
            "  $r=Install-WindowsFeature -Name Hyper-V -IncludeManagementTools; 'KIND=server'; 'RESTART='+$r.RestartNeeded"
            "}elseif($ed -match '^Core'){"
            "  $r=Enable-WindowsOptionalFeature -Online -FeatureName HypervisorPlatform -All -NoRestart; 'KIND=home'; 'RESTART='+$r.RestartNeeded"
            "}else{"
            "  $r=Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V -All -NoRestart; 'KIND=pro'; 'RESTART='+$r.RestartNeeded"
            "}", out, sizeof out)) {
        trim(out);
        say(C_RED, "기능을 켜지 못했습니다: %s\n", *out ? out : "(메시지 없음)");
        pf("  CPU 가상화(BIOS 의 VT-x/AMD-V)가 꺼져 있거나, VPS 가 중첩 가상화를 지원하지 않으면 실패할 수 있습니다.\n");
        pf("  이 경우 Linux VPS 에서 vps-setup 을 쓰세요.\n");
        return 1;
    }
    pf("  %s 를 켰습니다.\n", strstr(out, "KIND=home") ? "HypervisorPlatform" : strstr(out, "KIND=server") ? "Hyper-V 역할" : "Hyper-V");
    say(C_GREEN, "완료. 재부팅한 뒤 vps-check → vps-setup 을 다시 실행하세요.\n");
    return 0;
}

BOOL gateway_preflight(BOOL install_missing)
{
    char out[2048];
    BOOL ok = TRUE;
    (void)install_missing;

    if (!ps(PREFLIGHT_PS, out, sizeof out) && !*out) {
        say(C_RED, "PowerShell 점검을 실행하지 못했습니다.\n");
        return FALSE;
    }
    if (!strstr(out, "NetNat=")) {
        say(C_RED, "네트워크 cmdlet 점검 결과를 받지 못했습니다. PowerShell 5.1 이상과 NetNat/NetSecurity 모듈이 필요합니다.\n");
        return FALSE;
    }
    g_netnat_query_broken = FALSE;
    if (strstr(out, "NetNat=INVALID_CLASS")) {
        /* 원본 스크립트도 Get-NetNat 오류는 무시하고(-ErrorAction SilentlyContinue) New-NetNat 으로 진행한다.
         * 조회(Get)만 실패하고 생성(New)은 되는 PC 가 있으므로 여기서 멈추지 않는다.
         * WinNAT 드라이버가 멈춰 있으면 먼저 시작해 본다 (vps-setup 에서만, vps-check 는 아무것도 바꾸지 않는다) */
        if (install_missing && !strstr(out, "WinNat=Running")) {
            char again[512];
            pf("  WinNAT 드라이버(winnat)가 실행 중이 아니라 시작합니다.\n");
            run_ok("sc.exe start winnat");
            if (ps("try{Get-NetNat | Out-Null;'NetNat=OK'}catch{'NetNat=FAIL'}", again, sizeof again) && strstr(again, "NetNat=OK")) {
                pf("  WinNAT 사용 가능해졌습니다.\n");
                return TRUE;
            }
        }
        g_netnat_query_broken = TRUE;
        say(C_YELLOW,
            "  주의: Get-NetNat(NAT 조회)이 \"잘못된 클래스입니다(0x80041010)\" 로 실패합니다.\n"
            "        원본 스크립트처럼 NAT 생성(New-NetNat)은 될 수 있으므로 계속 진행합니다.\n"
            "        생성까지 실패하면 그 자리에서 멈추고 설치한 터널을 자동으로 되돌립니다.\n"
            "        (생성도 실패하는 PC 라면: tgui.exe winnat-enable → 재부팅)\n");
        print_diag(out);
    }
    if (strstr(out, "NetFirewall=INVALID_CLASS")) { print_wmi_help("MSFT_NetFirewallRule", out); return FALSE; }
    if (strstr(out, "NetRoute=INVALID_CLASS")) { print_wmi_help("MSFT_NetRoute", out); return FALSE; }
    if (strstr(out, "NetIPInterface=INVALID_CLASS")) { print_wmi_help("MSFT_NetIPInterface", out); return FALSE; }
    if (strstr(out, "NetFirewall=ERROR") || strstr(out, "NetRoute=ERROR") || strstr(out, "NetIPInterface=ERROR")) {
        say(C_RED, "네트워크 cmdlet 점검이 실패했습니다.\n");
        print_diag(out);
        ok = FALSE;
    }
    {
        const char *p = strstr(out, "OtherNat=");
        if (p) {
            char names[256];
            snprintf(names, sizeof names, "%s", p + 9);
            names[strcspn(names, "\r\n")] = 0;
            say(C_RED, "다른 NAT 네트워크가 이미 있습니다(%s). WinNAT 은 하나만 지원하므로 먼저 정리하세요.\n", names);
            ok = FALSE;
        }
    }
    if (ok) {
        const char *d = strstr(out, "Diag=");
        if (d) { char line[300]; size_t n = strcspn(d + 5, "\r\n"); if (n >= sizeof line) n = sizeof line - 1; memcpy(line, d + 5, n); line[n] = 0; pf("  PowerShell 환경: %s\n", line); }
    }
    return ok;
}

/* ------------------------------------------------------------------ 포워딩 / NAT */

void gateway_conf_extra(char *out, size_t sz)
{
    (void)sz;
    out[0] = 0;   /* Windows 는 터널 설정 파일에 NAT 규칙을 넣지 않는다 */
}

static BOOL set_ip_forwarding(DWORD v)
{
    HKEY k;
    LONG r = RegCreateKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters", 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL);
    if (r != ERROR_SUCCESS) return FALSE;
    r = RegSetValueExA(k, "IPEnableRouter", 0, REG_DWORD, (const BYTE *)&v, sizeof v);
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

BOOL gateway_apply(void)
{
    char s[8192], out[1024], udps[PORTS_STR_MAX], tcps[PORTS_STR_MAX];
    int i;

    step("IP 포워딩 활성화");
    if (!set_ip_forwarding(1)) { say(C_RED, "레지스트리(IPEnableRouter) 설정 실패\n"); return FALSE; }
    if (!ps("$t='" TUN_VPS "';"
            "$e=(Get-NetRoute -DestinationPrefix '0.0.0.0/0' -AddressFamily IPv4 | Where-Object InterfaceAlias -ne $t | "
            "Sort-Object { $_.RouteMetric + (Get-NetIPInterface -InterfaceIndex $_.InterfaceIndex -AddressFamily IPv4).InterfaceMetric } | "
            "Select-Object -First 1).InterfaceAlias;"
            "if(-not $e){throw 'No external interface with a default route was found.'};"
            "foreach($a in @($t,$e)){Set-NetIPInterface -InterfaceAlias $a -AddressFamily IPv4 -Forwarding Enabled};"
            "[Console]::Out.Write($e)", out, sizeof out)) {
        trim(out);
        say(C_RED, "인터페이스 포워딩 설정 실패: %s\n", out);
        return FALSE;
    }
    pf("  포워딩: " TUN_VPS ", %s\n", out);

    step("WinNAT 구성");
    /* foreach 에 넣으므로 범위가 아니라 하나씩 나열한다 ("7777-7790" 은 PowerShell 에서 뺄셈이 된다) */
    ports_plain(O.udp, O.nudp, udps, sizeof udps);
    ports_plain(O.tcp, O.ntcp, tcps, sizeof tcps);
    snprintf(s, sizeof s,
             "Get-NetNatStaticMapping -NatName '" NAT_NAME "' -ErrorAction SilentlyContinue | Remove-NetNatStaticMapping -Confirm:$false;"
             "Get-NetNat -Name '" NAT_NAME "' -ErrorAction SilentlyContinue | Remove-NetNat -Confirm:$false;"
             "New-NetNat -Name '" NAT_NAME "' -InternalIPInterfaceAddressPrefix '%s' | Out-Null;", O.subnet);
    if (O.nudp)
        snprintf(s + strlen(s), sizeof s - strlen(s),
                 "foreach($p in %s){Add-NetNatStaticMapping -NatName '" NAT_NAME "' -Protocol UDP -ExternalIPAddress '0.0.0.0/24' "
                 "-ExternalPort $p -InternalIPAddress '%s' -InternalPort $p | Out-Null};", udps, O.clientip);
    if (O.ntcp)
        snprintf(s + strlen(s), sizeof s - strlen(s),
                 "foreach($p in %s){Add-NetNatStaticMapping -NatName '" NAT_NAME "' -Protocol TCP -ExternalIPAddress '0.0.0.0/24' "
                 "-ExternalPort $p -InternalIPAddress '%s' -InternalPort $p | Out-Null};", tcps, O.clientip);
    if (!ps(s, out, sizeof out)) {
        trim(out);
        say(C_RED, "NAT 구성 실패 (New-NetNat): %s\n", *out ? out : "(메시지 없음)");
        if (g_netnat_query_broken || strstr(out, "0x80041010") || strstr(out, "Invalid class") || strstr(out, "잘못된 클래스")) {
            char diag[2048];
            print_wmi_help("MSFT_NetNat / WinNAT", ps(PREFLIGHT_PS, diag, sizeof diag) ? diag : "");
        }
        step("되돌리기: 방금 설치한 터널 서비스 제거");
        pf("  %s\n", tunnel_remove(TUN_VPS) ? "완료" : "실패 - vps-uninstall -force 로 정리하세요");
        pf("  (IP 포워딩 설정은 그대로 둡니다. 끄려면 vps-uninstall -force -disable-routing)\n");
        return FALSE;
    }
    ports_str(O.udp, O.nudp, udps, sizeof udps);
    ports_str(O.tcp, O.ntcp, tcps, sizeof tcps);
    if (O.nudp) pf("  UDP %s → %s (같은 포트)\n", udps, O.clientip);
    if (O.ntcp) pf("  TCP %s → %s (같은 포트)\n", tcps, O.clientip);
    (void)i;
    return TRUE;
}

BOOL gateway_present(void)
{
    char out[64];
    /* 클래스가 없는 Windows 에서도 오류 없이 "없음" 으로 처리되도록 try/catch */
    if (!ps("try{if(Get-NetNat -Name '" NAT_NAME "' -ErrorAction Stop){'1'}}catch{}", out, sizeof out)) return FALSE;
    return out[0] == '1';
}

BOOL gateway_remove(void)
{
    return ps_step("try{Get-NetNatStaticMapping -NatName '" NAT_NAME "' -ErrorAction Stop | Remove-NetNatStaticMapping -Confirm:$false}catch{};"
                   "Remove-NetNat -Name '" NAT_NAME "' -Confirm:$false", "NAT 제거");
}

BOOL gateway_disable_routing(void)
{
    set_ip_forwarding(0);
    return ps_step("Get-NetIPInterface -AddressFamily IPv4 | Where-Object Forwarding -eq 'Enabled' | "
                   "ForEach-Object { Set-NetIPInterface -InterfaceIndex $_.InterfaceIndex -AddressFamily IPv4 -Forwarding Disabled }",
                   "포워딩 해제");
}

void gateway_status(void)
{
    pf("NAT " NAT_NAME "  : %s\n", gateway_present() ? "있음" : "없음");
}

void gateway_list_ports(void)
{
    char out[2048];
    if (!ps("try{$m=@(Get-NetNatStaticMapping -NatName '" NAT_NAME "' -ErrorAction Stop)}catch{$m=@()};"
            "$m | Sort-Object Protocol,ExternalPort | ForEach-Object { '  '+$_.Protocol+' '+$_.ExternalPort+' -> '+$_.InternalIPAddress+':'+$_.InternalPort }",
            out, sizeof out)) out[0] = 0;
    trim(out);
    if (*out) pf("  %s\n", out); else pf("  (없음)\n");
}

/* ------------------------------------------------------------------ Windows 방화벽 */

const char *firewall_label(void) { return "Windows 방화벽"; }

BOOL firewall_remove(void)
{
    return ps_step("Get-NetFirewallRule -Group '" FW_GROUP "' -ErrorAction SilentlyContinue | Remove-NetFirewallRule", "방화벽 규칙 제거");
}

BOOL firewall_rule(const char *name, const char *proto, const char *ports)
{
    char s[PORTS_STR_MAX * 2 + 512], quoted[PORTS_STR_MAX * 2];
    const char *p;
    size_t k = 0;
    /* "7777-7790,27015" → '7777-7790','27015' (따옴표 없으면 PowerShell 이 7777-7790 을 뺄셈으로 계산한다) */
    quoted[k++] = '\'';
    for (p = ports; *p && k + 4 < sizeof quoted; p++) {
        if (*p == ',') { quoted[k++] = '\''; quoted[k++] = ','; quoted[k++] = '\''; }
        else if ((*p >= '0' && *p <= '9') || *p == '-') quoted[k++] = *p;
    }
    quoted[k++] = '\'';
    quoted[k] = 0;
    snprintf(s, sizeof s,
             "New-NetFirewallRule -DisplayName '%s' -Group '" FW_GROUP "' -Direction Inbound -Protocol %s "
             "-LocalPort %s -Action Allow -Profile Any | Out-Null", name, proto, quoted);
    return ps_step(s, "방화벽 규칙 추가");
}

int fw_count(void)
{
    char out[256];
    if (!ps("(Get-NetFirewallRule -Group '" FW_GROUP "' -ErrorAction SilentlyContinue | Measure-Object).Count", out, sizeof out)) return 0;
    return atoi(out);
}

void firewall_list(void)
{
    char out[2048];
    if (!ps("Get-NetFirewallRule -Group '" FW_GROUP "' -ErrorAction SilentlyContinue | ForEach-Object {"
            " $f = $_ | Get-NetFirewallPortFilter; '  '+$_.DisplayName+': '+$f.Protocol+' '+(($f.LocalPort) -join ',') }",
            out, sizeof out)) out[0] = 0;
    trim(out);
    if (*out) pf("  %s\n", out); else pf("  (없음)\n");
}
