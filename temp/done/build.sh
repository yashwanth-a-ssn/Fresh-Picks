#!/bin/bash
# =============================================================
# build.sh - Fresh Picks: C Backend Compilation Script
# =============================================================
# Run this script from the /backend directory to compile ALL
# C binaries. Execute once whenever you change any .c or .h file.
#
# HOW TO RUN:
#   cd Fresh-Picks/backend
#   chmod +x build.sh    (only needed the first time — makes it executable)
#   ./build.sh
#
# WHAT EACH FLAG MEANS:
#   gcc         → The GNU C Compiler
#   -Wall       → Show ALL warnings (helps catch bugs early)
#   -Wextra     → Show even more warnings (good habit)
#   -o auth     → Name the output binary "auth" (instead of a.out)
#   -lm         → Link the math library (needed for some math functions)
#
# LINKING EXPLAINED:
#   order.c and inventory.c both #include "models.h"
#   They call functions DEFINED in ds_utils.c.
#   So we must compile BOTH files together in one gcc command:
#     gcc order.c ds_utils.c -o order
#   The linker then connects the function CALLS (in order.c) to the
#   function IMPLEMENTATIONS (in ds_utils.c).
#
# Team: CodeCrafters | Project: Fresh Picks | SDP-1
# =============================================================

# Stop immediately if any command fails.
# Without this, the script might silently continue after a compile error.
set -e

echo "========================================"
echo "  Fresh Picks — C Backend Build Script"
echo "  Team: CodeCrafters | SDP-1"
echo "========================================"
echo ""

# ─────────────────────────────────────────────────────────────
# BINARY 1: auth
# Source file: auth.c (handles login, register, change password)
# Does NOT use ds_utils.c (no data structures needed for auth)
# ─────────────────────────────────────────────────────────────
echo "[1/3] Compiling auth..."
gcc -Wall -Wextra -o auth auth.c
echo "      ✓ auth compiled successfully"
echo ""

# ─────────────────────────────────────────────────────────────
# BINARY 2: order
# Source files: order.c + ds_utils.c
#
# WHY TWO FILES?
#   order.c contains the command handlers (checkout, add_to_cart, etc.)
#   ds_utils.c contains the data structure implementations (DLL, Queue, etc.)
#   order.c CALLS functions from ds_utils.c.
#   The compiler needs BOTH files to build one complete binary.
#
# -lm: Links the standard math library. time() requires linking
#      the time utilities, which are included with -lm on some systems.
# ─────────────────────────────────────────────────────────────
echo "[2/3] Compiling order (with ds_utils)..."
gcc -Wall -Wextra -o order order.c ds_utils.c -lm
echo "      ✓ order compiled successfully"
echo ""

# ─────────────────────────────────────────────────────────────
# BINARY 3: inventory
# Source files: inventory.c + ds_utils.c
#
# WHY ds_utils.c HERE TOO?
#   inventory.c includes models.h, which declares the DS structs.
#   Even if inventory.c doesn't directly use the DLL or Heap, linking
#   ds_utils.c ensures no "undefined reference" linker errors occur
#   if models.h refers to any DS types.
#   It's safer and simpler to always link ds_utils.c for any binary
#   that includes models.h.
# ─────────────────────────────────────────────────────────────
echo "[3/3] Compiling inventory (with ds_utils)..."
gcc -Wall -Wextra -o inventory inventory.c ds_utils.c -lm
echo "      ✓ inventory compiled successfully"
echo ""

echo "========================================"
echo "  All binaries built successfully!"
echo ""
echo "  Binaries created in /backend:"
echo "    ./auth        (login, register, change password)"
echo "    ./order       (cart, checkout, orders)"
echo "    ./inventory   (admin: update stock + promos)"
echo ""
echo "  Next step: cd ../app && python app.py"
echo "========================================"
