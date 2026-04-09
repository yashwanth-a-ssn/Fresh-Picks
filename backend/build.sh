#!/bin/bash
# =============================================================
# build.sh - Fresh Picks: C Backend Compilation Script (v2)
# =============================================================
# This script compiles ALL C source files into executable
# binaries that Flask will call using subprocess.
#
# SPRINT 2 ADDITIONS:
#   - Compiles order.c -> order binary
#   - Creates carts/ directory (needed by order.c for cart files)
#
# HOW TO RUN:
#   Linux / macOS:   chmod +x build.sh && ./build.sh
#   Windows (Bash):  bash build.sh
#
# Team: CodeCrafters | Project: Fresh Picks | SDP-1
# =============================================================

echo "================================================"
echo "  Fresh Picks - Building C Backend Binaries"
echo "================================================"

# Navigate to the backend directory
cd "$(dirname "$0")"

# ── 1. order binary ─────────────────────────────────────────────
echo "[1/3] Compiling order..."
gcc -Wall -Wextra -o order order.c ds_utils.c -lm
echo "      ✓ order executed successfully"

# ── 2. auth binary ──────────────────────────────────────────────
echo "[2/3] Compiling auth..."
gcc -Wall -Wextra -o auth auth.c ds_utils.c -lm
echo "      ✓ auth executed successfully"

# ── 3. inventory binary ─────────────────────────────────────────
echo "[3/3] Compiling inventory..."
gcc -Wall -Wextra -o inventory inventory.c ds_utils.c -lm
echo "      ✓ inventory executed successfully"

# ── Create the carts/ directory if it doesn't exist ────────────
#
# WHY: order.c stores each user's cart as a separate .txt file
# inside the carts/ folder: carts/U1001_cart.txt
# The -p flag means "create parent dirs too, no error if exists"
echo ""
echo "[Setup] Creating carts/ directory..."
mkdir -p carts
echo "        ✓ carts/ directory ready"

echo ""
echo "================================================"
echo "  All binaries compiled! Ready to run Flask."
echo ""
echo "  Next step: Run app.py"
echo "  Git Bash:    cd ../app && python app.py"
echo "  PowerShell:  cd ../app; python app.py"
echo "  macOS/Linux: cd ../app && python3 app.py"
echo ""
echo "================================================"
