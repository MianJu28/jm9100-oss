#!/bin/bash
# enable-ssh-rescue.sh —— 建立 SSH 救援通道（最小暴露配置）
#
# 背景：本机只有 openssh-client，没有服务端，所以换内核驱动黑屏后
#       没有任何网络救援通道。这个脚本把服务端装起来并做安全收敛，
#       使你能从另一台机器 ssh 进来回滚。
#
# 用法：
#   sudo ./tools/enable-ssh-rescue.sh              # 默认 22 端口
#   sudo PORT=2222 ./tools/enable-ssh-rescue.sh    # 换端口
#   sudo ./tools/enable-ssh-rescue.sh --check      # 只看现状，不改系统
#   sudo ./tools/enable-ssh-rescue.sh --uninstall  # 卸载并清理
#
# 安全策略（均可在下方 CONFIG 段调整）：
#   - 只允许公钥登录，禁用密码（防爆破）
#   - 禁止 root 直接登录
#   - 只允许指定用户（默认当前 sudo 调用者）
#   - MaxAuthTries 3 / LoginGraceTime 30s
#
# 注意：装好后 authorized_keys 里目前只有本机自己的公钥，
#       要真正从另一台电脑救援，必须先把那台电脑的公钥加进来（脚本末尾会打印做法）。

set -euo pipefail

# ---------------- CONFIG ----------------
PORT="${PORT:-22}"
ALLOW_USER="${ALLOW_USER:-${SUDO_USER:-admin}}"
LISTEN_ADDR="${LISTEN_ADDR:-0.0.0.0}"
DROPIN="/etc/ssh/sshd_config.d/00-mwv207-rescue.conf"
STATE_DIR="/var/lib/mwv207-oss"
# ----------------------------------------

c_red=$'\033[31m'; c_grn=$'\033[32m'; c_yel=$'\033[33m'; c_dim=$'\033[2m'; c_off=$'\033[0m'
info() { echo "${c_grn}[ OK ]${c_off} $*"; }
warn() { echo "${c_yel}[WARN]${c_off} $*"; }
fail() { echo "${c_red}[FAIL]${c_off} $*" >&2; }
step() { echo; echo "${c_dim}== $* ==${c_off}"; }
die()  { fail "$*"; exit 1; }

MODE=install
case "${1:-}" in
    --check)        MODE=check ;;
    --uninstall)    MODE=uninstall ;;
    --print-config) MODE=print ;;
    "" )            ;;
    *)              die "未知参数: $1（可用 --check / --print-config / --uninstall）" ;;
esac

# drop-in 内容集中在一处，--print-config 与实际写盘用同一份
render_config() {
    cat <<EOF
# 由 enable-ssh-rescue.sh 生成 —— 救援通道最小暴露配置
# 生成时间: $(date '+%Y-%m-%d %H:%M:%S')

Port $PORT
ListenAddress $LISTEN_ADDR

# 只认公钥，彻底关掉口令类认证
# 不用 ChallengeResponseAuthentication：它只是 KbdInteractiveAuthentication 的旧别名
# （用 10.0p2 实测：写了会被静默接受并映射为 kbdinteractiveauthentication，无警告），
# 写它纯属冗余，只留下面这条即可。
PubkeyAuthentication yes
PasswordAuthentication no
KbdInteractiveAuthentication no
PermitEmptyPasswords no

# 不给 root 走后门
PermitRootLogin no

# 白名单：只允许这些用户
AllowUsers $ALLOW_USER

# 收敛攻击面
MaxAuthTries 3
MaxSessions 4
LoginGraceTime 30
ClientAliveInterval 30
ClientAliveCountMax 3
X11Forwarding no
AllowAgentForwarding no
PermitTunnel no

UsePAM yes
EOF
}

# ---------------- 现状 ------------------
show_status() {
    step "现状"
    if dpkg -s openssh-server >/dev/null 2>&1; then
        info "openssh-server 已安装：$(dpkg-query -W -f='${Version}' openssh-server)"
    else
        warn "openssh-server 未安装（当前只有客户端）"
    fi
    local a; a=$(systemctl is-active ssh 2>/dev/null || true)
    local e; e=$(systemctl is-enabled ssh 2>/dev/null || true)
    echo "       ssh.service: active=${a:-unknown} enabled=${e:-unknown}"
    local l; l=$(ss -tln 2>/dev/null | awk -v P=":$PORT" '$4 ~ P {print $4}' | paste -sd, -)
    echo "       监听 :$PORT → ${l:-无}"
    if [ -f "$DROPIN" ]; then
        info "已存在加固配置 $DROPIN"
    else
        echo "       加固配置: 尚未写入"
    fi
    if compgen -G "/etc/ssh/ssh_host_*_key" >/dev/null; then
        info "主机密钥已存在（$(ls /etc/ssh/ssh_host_*_key 2>/dev/null | wc -l) 个）"
    else
        echo "       主机密钥: 不存在（首次启动时生成）"
    fi
}

[ "$MODE" = check ] && { show_status; exit 0; }

# 预览：不碰系统，打印将要写入的 drop-in
if [ "$MODE" = print ]; then
    echo "# ↓↓↓ 将写入 $DROPIN （端口 $PORT，用户 $ALLOW_USER）↓↓↓"
    render_config
    exit 0
fi

# ---------------- 前置 ------------------
[ "$(id -u)" -eq 0 ] || die "需要 root：sudo $0 $*"

if [ "$MODE" = uninstall ]; then
    step "卸载"
    systemctl disable --now ssh 2>/dev/null || true
    rm -f "$DROPIN"
    apt-get -y purge openssh-server >/dev/null 2>&1 || warn "purge 失败，可手动 apt-get purge openssh-server"
    rm -rf "$STATE_DIR"
    info "已卸载（/etc/ssh/ssh_host_* 保留，重装会复用；彻底清理请自行删除）"
    exit 0
fi

# ---------------- 1. 安装 ----------------
step "1/6 安装 openssh-server"
if dpkg -s openssh-server >/dev/null 2>&1; then
    info "已安装，跳过"
else
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq || warn "apt-get update 失败，继续尝试安装"
    apt-get install -y -qq openssh-server || die "安装失败"
    info "已安装 $(dpkg-query -W -f='${Version}' openssh-server)"
fi

# ---------------- 2. 生成主机密钥 ----------------
step "2/6 主机密钥"
if compgen -G "/etc/ssh/ssh_host_*_key" >/dev/null; then
    info "已存在，跳过"
else
    ssh-keygen -A || die "ssh-keygen -A 失败"
    info "已生成 $(ls /etc/ssh/ssh_host_*_key | wc -l) 个主机密钥"
fi

# ---------------- 3. 加固配置 ----------------
step "3/6 写入加固配置"

mkdir -p /etc/ssh/sshd_config.d

# 确保主配置 include 了 drop-in 目录（Debian/Deepin 默认有，个别版本没有）
CONF=/etc/ssh/sshd_config
if [ ! -f "$CONF" ]; then
    printf 'Include /etc/ssh/sshd_config.d/*.conf\n' > "$CONF"
    chmod 0644 "$CONF"
    info "sshd_config 不存在，已新建并带上 Include"
elif ! grep -qE '^\s*Include\s+/etc/ssh/sshd_config\.d/\*\.conf' "$CONF"; then
    cp -a "$CONF" "${CONF}.bak.$(date +%Y%m%d-%H%M%S)"
    { echo "Include /etc/ssh/sshd_config.d/*.conf"; cat "$CONF"; } > /tmp/sshd_config.new
    install -m 0644 /tmp/sshd_config.new "$CONF"
    rm -f /tmp/sshd_config.new
    info "已给 sshd_config 补上 Include（原文件已备份）"
else
    info "sshd_config 已 include drop-in 目录"
fi
# 先落盘到临时文件再原子替换：sshd -t 失败时不会留下坏配置
render_config > "$DROPIN.tmp"
mv "$DROPIN.tmp" "$DROPIN"
chmod 0644 "$DROPIN"
info "已写入 $DROPIN"

# ---------------- 4. 语法校验（失败即回滚） ----------------
step "4/6 校验配置"
SSHD_BIN="$(command -v sshd 2>/dev/null || true)"
[ -n "$SSHD_BIN" ] || { rm -f "$DROPIN"; die "找不到 sshd 可执行文件"; }
if "$SSHD_BIN" -t 2>/tmp/sshd-t.err; then
    info "sshd -t 通过"
else
    fail "sshd -t 失败，已移除加固配置以免 sshd 起不来："
    sed 's/^/       /' /tmp/sshd-t.err >&2
    rm -f "$DROPIN"
    rm -f /tmp/sshd-t.err
    die "配置无效，未启用服务"
fi
rm -f /tmp/sshd-t.err

# ---------------- 5. 启用服务 ----------------
step "5/6 启用并启动服务"
systemctl daemon-reload
# Debian 13 / OpenSSH 10 起有 socket 激活模式；两种都兼容
if systemctl list-unit-files ssh.socket >/dev/null 2>&1 && systemctl cat ssh.socket >/dev/null 2>&1; then
    if systemctl is-enabled ssh.socket >/dev/null 2>&1; then
        info "ssh.socket 处于启用状态（socket 激活模式）"
        systemctl restart ssh.socket 2>/dev/null || warn "重启 ssh.socket 失败"
    else
        systemctl enable --now ssh.service >/dev/null 2>&1 || warn "enable ssh.service 失败"
        systemctl restart ssh.service 2>/dev/null || systemctl start ssh.service || warn "启动 ssh.service 失败"
    fi
else
    systemctl enable --now ssh.service >/dev/null 2>&1 || warn "enable ssh.service 失败"
    systemctl restart ssh.service 2>/dev/null || systemctl start ssh.service || warn "启动 ssh.service 失败"
fi

# 真正的判据是"端口有没有在听"，而不是某个 unit 名是否为 active
LISTEN=""
for _ in 1 2 3 4 5 6; do
    sleep 0.5
    LISTEN=$(ss -tln 2>/dev/null | awk -v P=":$PORT" '$4 ~ P {print $4}' | paste -sd, -)
    [ -n "$LISTEN" ] && break
done
if [ -z "$LISTEN" ]; then
    journalctl -u ssh -u ssh.socket -n 20 --no-pager 2>/dev/null | sed 's/^/       /'
    die "服务已配置但 :$PORT 仍无监听"
fi
info "已在 :$PORT 监听（$LISTEN）"

# ---------------- 6. 验证 ----------------
step "6/6 连通性自检"

# 用 sshd -T 读回"生效值"，确认加固真的落地（而不是只写了文件）
EFF=$("$SSHD_BIN" -T 2>/dev/null || true)
if [ -n "$EFF" ]; then
    for k in port listenaddress passwordauthentication kbdinteractiveauthentication \
             permitrootlogin pubkeyauthentication allowusers maxauthtries; do
        v=$(printf '%s\n' "$EFF" | awk -v K="$k" 'tolower($1)==K {$1=""; sub(/^ /,""); print; exit}')
        printf '       %-32s %s\n' "$k" "${v:-<未设置>}"
    done
    # 硬断言：口令登录必须是 no，否则救援通道等于开了后门
    if printf '%s\n' "$EFF" | awk 'tolower($1)=="passwordauthentication"{print tolower($2)}' | grep -qx no; then
        info "生效值确认：PasswordAuthentication=no"
    else
        fail "生效值里 PasswordAuthentication 不是 no —— 拒绝继续"
        rm -f "$DROPIN"
        die "加固未生效，已撤销 drop-in"
    fi
else
    warn "sshd -T 取不到生效值，跳过（sshd -t 已通过）"
fi

# 本机自发登录测试（用现有 authorized_keys）
if [ -f "/home/$ALLOW_USER/.ssh/authorized_keys" ]; then
    if timeout 8 ssh -o BatchMode=yes -o StrictHostKeyChecking=no \
         -o UserKnownHostsFile=/dev/null -p "$PORT" \
         "$ALLOW_USER@127.0.0.1" 'echo SSH_SELF_TEST_OK' 2>/dev/null | grep -q SSH_SELF_TEST_OK; then
        info "公钥登录自检通过"
    else
        warn "公钥登录自检失败 —— 检查 authorized_keys 权限（应为 600）与 ~/.ssh（700）"
    fi
else
    warn "没有 /home/$ALLOW_USER/.ssh/authorized_keys"
fi

# ---------------- 收尾提示 ----------------
mkdir -p "$STATE_DIR"
echo "$(date '+%F %T') port=$PORT user=$ALLOW_USER" >> "$STATE_DIR/ssh-rescue.log"

IP=$(ip -4 -o addr show scope global 2>/dev/null | awk '!/tun/{print $4}' | cut -d/ -f1 | head -1)
cat <<EOF

$(printf '%s' "${c_grn}")================ 救援通道已建立 ================$(printf '%s' "${c_off}")

  从另一台电脑连入：
      ssh -p $PORT $ALLOW_USER@${IP:-<本机IP>}

$(printf '%s' "${c_yel}")⚠ 还差最后一步：把「另一台电脑」的公钥加进来$(printf '%s' "${c_off}")
  现在 authorized_keys 里只有本机自己的公钥，别的机器连不进来。
  在另一台电脑上执行：
      ssh-keygen -t ed25519 -f ~/.ssh/id_ed25519 -N ''      # 若还没有
      ssh-keygen -y -f ~/.ssh/id_ed25519                     # 得到公钥
  把输出的一行，追加到本机：
      echo '<那台电脑的公钥>' >> /home/$ALLOW_USER/.ssh/authorized_keys
      chmod 600 /home/$ALLOW_USER/.ssh/authorized_keys

  排查命令：
      sudo sshd -t                      # 配置语法
      sudo journalctl -u ssh -f         # 实时日志
      sudo ss -tlnp | grep :$PORT     # 确认监听

  黑屏救援流程（KMD 出问题时）：
      1. 另一台电脑 ssh 进来
      2. sudo /home/${SUDO_USER:-admin}/jm9100-oss/switch-mwv207.sh kmd rollback
      3. sudo systemctl restart lightdm

$(printf '%s' "${c_dim}")配置：$DROPIN   卸载：sudo $0 --uninstall$(printf '%s' "${c_off}")
EOF