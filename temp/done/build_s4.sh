#!/bin/bash
# ============================================================
# build.sh - Fresh Picks: Compile All C Binaries
# ============================================================
# Run this script once before starting app.py to compile all
# C binaries that Flask calls via subprocess.run().
#
# USAGE:
#   chmod +x build.sh    (make it executable — one-time only)
#   ./build.sh           (compile all binaries)
#
# OUTPUT BINARIES:
#   ./inventory  ← called by /api/update_stock and /api/update_promo_stock
#   ./order      ← called by checkout and order processing routes
#   ./auth       ← called by login/registration routes (if using C auth)
#
# COMPILER FLAGS EXPLAINED:
#   -Wall        : Enable all common warnings (catches bugs early)
#   -Wextra      : Enable extra warnings
#   -o <name>    : Name the output binary
#
# Team: CodeCrafters | Project: Fresh Picks | SDP-1
# ============================================================

set -e   # Stop immediately if any command fails

echo ""
echo "╔══════════════════════════════════════════╗"
echo "║  🔨  Fresh Picks — Build Script          ║"
echo "╚══════════════════════════════════════════╝"
echo ""

# ── 1. Compile inventory binary ───────────────────────────
# Files: inventory.c + ds_utils.c + (models.h is auto-included)
# Output: ./inventory
echo "  [1/3] Compiling inventory binary..."
gcc -Wall -Wextra \
    inventory.c \
    ds_utils.c \
    -o inventory \
    -lm
echo "        ✓ ./inventory compiled"


# ── 2. Compile order binary ───────────────────────────────
# Files: order.c + ds_utils.c
# Output: ./order
echo "  [2/3] Compiling order binary..."
gcc -Wall -Wextra \
    order.c \
    ds_utils.c \
    -o order \
    -lm
echo "        ✓ ./order compiled"


# ── 3. Compile auth binary ────────────────────────────────
# Files: auth.c + ds_utils.c
# Output: ./auth
echo "  [3/3] Compiling auth binary..."
gcc -Wall -Wextra \
    auth.c \
    ds_utils.c \
    -o auth \
    -lm
echo "        ✓ ./auth compiled"


echo ""
echo "  ✅  All binaries compiled successfully!"
echo "  👉  Run: python app.py"
echo ""
