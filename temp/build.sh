#!/bin/bash
# =============================================================
# build.sh - Fresh Picks: C Backend Compilation Script (v4)
# =============================================================
# SPRINT 4 CHANGES:
#   - ds_utils.c renamed to utils.c (rename your file before running)
#   - utils.c is now linked to ALL binaries including receipt.c
#     (receipt.c now calls load_order_sll / load_user_sll / load_delivery_boy_sll)
#   - Added txt_to_bin_converter compilation step
#   - All binaries now read/write .dat binary files via utils.c
#
# HOW TO RUN:
#   Linux / macOS:   chmod +x build.sh && ./build.sh
#   Windows (Powershell):  bash build.sh
#
# FIRST-TIME SETUP:
#   After running this script, run the converter once:
#     ./txt_to_bin_converter
#   Then start Flask:
#     cd ../app && python app.py
#
# Team: CodeCrafters | Project: Fresh Picks | SDP-1
# =============================================================

echo "================================================"
echo "  Fresh Picks - Building C Backend Binaries (v4)"
echo "================================================"

# Navigate to the directory containing this script (backend/)
cd "$(dirname "$0")"

# Track overall build success
BUILD_FAILED=0

# ── Helper: compile one binary and report result ─────────────
compile() {
    local STEP="$1"
    local LABEL="$2"
    local CMD="$3"

    echo "[$STEP] Compiling $LABEL..."
    if eval "$CMD"; then
        echo "      ✓ $LABEL compiled successfully"
    else
        echo "      ✗ $LABEL FAILED — check errors above"
        BUILD_FAILED=1
    fi
}

# ── 1. order binary ─────────────────────────────────────────────
# Handles: list_products, add_to_cart, view_cart, remove_item,
#          checkout, get_orders, admin_orders, batch_promote_slot
compile "1/6" "order" \
    "gcc -Wall -Wextra -o order order.c utils.c -lm"

# ── 2. auth binary ──────────────────────────────────────────────
# Handles: login_user, login_admin, register, get_profile,
#          update_profile, change_pass_user, change_pass_admin
compile "2/6" "auth" \
    "gcc -Wall -Wextra -o auth auth.c utils.c -lm"

# ── 3. inventory binary ─────────────────────────────────────────
# Handles: update_stock, update_promo_stock, list_promo
compile "3/6" "inventory" \
    "gcc -Wall -Wextra -o inventory inventory.c utils.c -lm"

# ── 4. delivery binary ──────────────────────────────────────────
# Handles: update_status, batch_promote_slot, cancel_order,
#          get_active_orders, assign_agent, list_all_orders_sorted
compile "4/6" "delivery" \
    "gcc -Wall -Wextra -o delivery delivery.c utils.c -lm"

# ── 5. receipt binary ───────────────────────────────────────────
# Handles: <order_id> → outputs full receipt data for PDF generation
# Now links utils.c because it calls load_order_sll, load_user_sll,
# and load_delivery_boy_sll instead of reading .txt files directly.
compile "5/6" "receipt" \
    "gcc -Wall -Wextra -o receipt receipt.c utils.c -lm"

# ── 6. txt_to_bin_converter ─────────────────────────────────────
# One-time migration tool: converts .txt files to binary .dat files.
# Run this ONCE after building to migrate your existing data.
compile "6/6" "txt_to_bin_converter" \
    "gcc -Wall -Wextra -o txt_to_bin_converter txt_to_bin_converter.c utils.c -lm"

# ── Create the carts/ directory if it doesn't exist ─────────────
echo ""
echo "[Setup] Creating carts/ directory..."
mkdir -p carts
echo "        ✓ carts/ directory ready"

echo ""
echo "================================================"

if [ "$BUILD_FAILED" -eq 1 ]; then
    echo "  BUILD INCOMPLETE — one or more binaries failed."
    echo "  Fix the errors above and re-run build.sh."
    echo "================================================"
    exit 1
fi

echo "  All binaries compiled! Ready to migrate and run."
echo ""
echo "  NEXT STEPS (first time only):"
echo "    1. Run the converter:  ./txt_to_bin_converter"
echo "       This creates .dat files from your .txt data."
echo ""
echo "  NEXT STEPS (every time):"
echo "    2. Start Flask:"
echo "       Linux/macOS:  cd ../app && python3 app.py"
echo "       Windows Powershell: cd ../app && python app.py"
echo ""
echo "================================================"
