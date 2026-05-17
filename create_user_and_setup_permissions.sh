#!/bin/bash
set -euo pipefail

# ─── Configuration ─────────────────────────────────────────────────────────────
PROJECT_DIR="${1:-$(pwd)}"
SCRIPT_PATH="$(realpath "$0")"
CURRENT_USER="${SUDO_USER:-$USER}"
AGENTOS_USER="agentos"
AGENTOS_GROUP="agentos"
VENV_DIR="$PROJECT_DIR/venvpython3"
SUDOERS_FILE="/etc/sudoers.d/agentos"
RUN_AGENT_SCRIPT="$PROJECT_DIR/run-agent.sh"

# ─── Pre-flight Checks ────────────────────────────────────────────────────────
if [[ $EUID -ne 0 ]]; then
    echo "❌ Error: This script must be run as root (use sudo $0)"
    exit 1
fi

if [[ ! -d "$PROJECT_DIR" ]]; then
    echo "❌ Error: Project directory '$PROJECT_DIR' not found."
    exit 1
fi

echo "🔧 Setting up secure 'agentos' environment for: $PROJECT_DIR"
echo "   Current User : $CURRENT_USER"
echo "   Agent User   : $AGENTOS_USER"

# ─── 0. Ensure ACL tools are installed ────────────────────────────────────────
if ! command -v setfacl &> /dev/null; then
    echo "   Installing ACL tools..."
    apt-get update -y && apt-get install -y acl
fi

# ─── 1. Create Group & User ───────────────────────────────────────────────────
echo "📦 Creating group and user '$AGENTOS_USER'..."
if ! getent group "$AGENTOS_GROUP" >/dev/null 2>&1; then
    groupadd "$AGENTOS_GROUP"
fi
if ! id "$AGENTOS_USER" >/dev/null 2>&1; then
    useradd -m -g "$AGENTOS_GROUP" -s /bin/bash "$AGENTOS_USER"
fi
usermod -aG "$AGENTOS_GROUP" "$CURRENT_USER"

# ─── 2. Ensure Python venv/pip system packages are present ────────────────────
echo "📦 Ensuring Python venv & pip system packages are installed..."
if ! command -v python3-venv &> /dev/null || ! command -v python3-pip &> /dev/null; then
    echo "   Installing python3-venv and python3-pip..."
    apt-get update -y && apt-get install -y python3-venv python3-pip
fi

# ─── 3. Create Virtual Environment (Only if missing) ─────────────────────────
echo "🐍 Checking Python virtual environment..."
if [ ! -d "$VENV_DIR" ] || [ ! -x "$VENV_DIR/bin/python3" ]; then
    echo "   Creating Python virtual environment..."
    /usr/bin/python3 -m venv "$VENV_DIR"
    echo "   Upgrading pip and installing requests inside the venv..."
    "$VENV_DIR/bin/pip" install --quiet --upgrade pip >/dev/null 2>&1
    "$VENV_DIR/bin/pip" install --quiet requests >/dev/null 2>&1
else
    echo "   Virtual environment already exists. Skipping creation."
fi

# ─── 4. Set Directory Ownership & Permissions (STRICT) ─────────────────────────
echo "🔒 Configuring directory permissions (STRICT ISOLATION)"
chown -R "${AGENTOS_USER}:${AGENTOS_GROUP}" "$PROJECT_DIR"

# Base Permissions: Strict isolation (660 files, 770 dirs)
find "$PROJECT_DIR" -type f -exec chmod 660 {} \;
find "$PROJECT_DIR" -type d -exec chmod 770 {} \;

# ⚠️ ACL FIX: Apply Explicit ACLs to ALL existing files (fixes current files)
echo "🔓 Applying Explicit ACLs for $CURRENT_USER..."
setfacl -R -m u:"$CURRENT_USER":rwX "$PROJECT_DIR"

# ⚠️ ACL FIX: Apply Default ACLs to ALL directories (fixes future files)
# This ensures any new file created by the agent is also readable by you.
echo "🔓 Applying Default ACLs for $CURRENT_USER..."
find "$PROJECT_DIR" -type d -exec setfacl -d -m u:"$CURRENT_USER":rwx {} \;

# Optional: Lock down references to read-only for agentos
if [[ -d "$PROJECT_DIR/os/references" ]]; then
    echo "🔒 Locking down os/references..."
    chmod 750 "$PROJECT_DIR/os/references"
    find "$PROJECT_DIR/os/references" -type f -exec chmod 440 {} \;
    # Explicitly grant read access to current user (overrides 440)
    setfacl -R -m u:"$CURRENT_USER":r "$PROJECT_DIR/os/references"
    find "$PROJECT_DIR/os/references" -type d -exec setfacl -d -m u:"$CURRENT_USER":r {} \;
fi

# ⚠️ CRITICAL: Allow agentos to traverse parent directories
echo "🔓 Allowing 'agentos' to traverse parent directories..."
chmod o+x /home
chmod o+x "/home/${CURRENT_USER}"
chmod o+x "/home/${CURRENT_USER}/agents" 2>/dev/null || true

# ─── 5. Configure Sudoers Delegation ──────────────────────────────────────────
echo "⚙️  Configuring sudoers..."
cat > "$SUDOERS_FILE" <<EOF
# Allow agentos to run VBoxManage and setfacl as the current USER
agentos ALL=(${CURRENT_USER}) NOPASSWD: /usr/bin/VBoxManage, /usr/bin/setfacl

# Allow the current user to launch bash as agentos (required for run-agent.sh)
${CURRENT_USER} ALL=(agentos) NOPASSWD: /bin/bash
EOF

if visudo -cf "$SUDOERS_FILE" > /dev/null 2>&1; then
    chmod 440 "$SUDOERS_FILE"
    echo "✅ Sudoers configuration validated and installed."
else
    echo "❌ Error: Invalid sudoers syntax. Rolling back..."
    rm -f "$SUDOERS_FILE"
    exit 1
fi

# ─── 6. Restore Ownership for User-Managed Scripts ────────────────────────────
if [[ -f "$RUN_AGENT_SCRIPT" ]]; then
    chown "${CURRENT_USER}:${CURRENT_USER}" "$RUN_AGENT_SCRIPT"
    chmod 750 "$RUN_AGENT_SCRIPT"
    echo "🔐 Restored ownership/permissions for $RUN_AGENT_SCRIPT to ${CURRENT_USER}"
fi

chown "${CURRENT_USER}:${CURRENT_USER}" "$SCRIPT_PATH"
chmod 755 "$SCRIPT_PATH"
echo "🔐 Restored ownership/permissions for setup script to ${CURRENT_USER}"

echo ""
echo "🎉 Setup complete!"
echo "   - Base permissions: 660/770 (Strict Isolation)"
echo "   - Access Control: ACLs grant $CURRENT_USER read access to all files"
echo "📖 Usage: bash run-agent.sh chat interface"
