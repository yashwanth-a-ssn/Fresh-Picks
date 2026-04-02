#!/bin/bash
# =============================================================
# build.sh - Fresh Picks: C Backend Compilation Script
# =============================================================
# This script compiles all C source files into executable
# binaries that Flask will call using subprocess.
#
# HOW TO RUN:
#   1. Open a terminal in the Fresh-Picks/backend/ folder
#   2. Make this script executable (only needed once):
#        chmod +x build.sh
#   3. Run it:
#        ./build.sh
#
# OR compile manually (if you use VS Code's Code Runner):
#   gcc -o auth auth.c -Wall
#
# Team: CodeCrafters | Project: Fresh Picks | SDP-1
# =============================================================

echo "================================================"
echo "  Fresh Picks - Building C Backend Binaries"
echo "================================================"

# Navigate to the backend directory (in case script is called from elsewhere)
cd "$(dirname "$0")"

# ── Compile auth.c into the 'auth' binary ──────────────────
# Flags explained:
#   -o auth    : name the output binary "auth"
#   auth.c     : the source file to compile
#   -Wall      : show all warnings (good practice)
echo ""
echo "[1/1] Compiling auth.c -> auth"
gcc -o auth auth.c -Wall

# Check if compilation succeeded (exit code 0 = success)
if [ $? -eq 0 ]; then
    echo "      ✓ auth compiled successfully!"
else
    echo "      ✗ Compilation failed. Check errors above."
    exit 1
fi

echo ""
echo "================================================"
echo "  All binaries compiled! Ready to run Flask."
echo "  Next step: cd ../app && python app.py"
echo "================================================"
