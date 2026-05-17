#!/bin/bash
set -e

# Resolve the script's directory using the absolute path of this script file
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"

# Dynamically get the current interactive user (works on any machine)
CURRENT_USER="${USER:-$(whoami)}"

# ⚠️ CRITICAL FIX:
# 1. Use 'sudo -D' to change directory BEFORE switching users.
# 2. Use a multi-line 'bash -c' string to safely export variables without syntax errors.
# 3. Dynamically pass the current user to VBOX_DELEGATE_USER for VM delegation.
# 4. Set HOME to SCRIPT_DIR for correct relative path resolution.
# 5. Pass arguments correctly via "_ $@"
exec sudo -u agentos bash -c "
    cd '$SCRIPT_DIR'
    HOME='$SCRIPT_DIR'
    export VBOX_DELEGATE_USER='$CURRENT_USER'
    export VBOX_VM_NAME='${VBOX_VM_NAME:-agentos}'
    export VBOX_ACL_USER='agentos'
    exec '$SCRIPT_DIR/venvpython3/bin/python3' '$SCRIPT_DIR/builderagent.py' \"\$@\"
" _ "$@"
