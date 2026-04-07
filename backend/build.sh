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

# ── [1/2] Compile auth.c -> auth ──────────────────────────────
echo ""
echo "[1/2] Compiling auth.c -> auth"
gcc -o auth auth.c -Wall

if [ $? -eq 0 ]; then
    echo "      ✓ auth compiled successfully!"
else
    echo "      ✗ auth compilation failed. Check errors above."
    exit 1
fi

# ── [2/2] Compile order.c -> order ────────────────────────────
#
# order.c contains:
#   - Doubly Linked List  (Cart management)
#   - Standard Queue      (Order processing FIFO)
#   - Circular Linked List(Delivery boy round-robin)
#   - Min-Heap            (Admin priority queue)
#
# -Wall = show ALL warnings (important for pointer bugs in DLL!)
echo ""
echo "[2/2] Compiling order.c -> order"
gcc -o order order.c -Wall

if [ $? -eq 0 ]; then
    echo "      ✓ order compiled successfully!"
else
    echo "      ✗ order compilation failed. Check errors above."
    exit 1
fi

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
