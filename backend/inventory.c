/*
 * inventory.c - Fresh Picks: Admin Inventory Management
 * ======================================================
 * This binary handles all ADMIN-ONLY stock management commands.
 * Called by Flask via subprocess.run() like this:
 *   ./inventory <command> [arguments...]
 *
 * WHY A SEPARATE BINARY?
 *   Keeping inventory management separate from order.c makes each binary
 *   focused on a single responsibility. If the admin panel needs a new
 *   feature later, we only touch this file — not the entire order pipeline.
 *
 * ─────────────────────────────────────────────────────────────────
 * COMMANDS (argv[1]):
 *
 *   update_stock <veg_id> <new_stock_g> <new_price> <new_validity>
 *     → Update stock, price, and validity for one vegetable in products.txt
 *     → Used by admin to restock or reprice items
 *
 *   update_promo_stock <vf_id> <new_stock_g>
 *     → Update the stock for one promotional freebie in free_inventory.txt
 *     → Used by admin to restock curry leaves, coriander, etc.
 *
 * ─────────────────────────────────────────────────────────────────
 * OUTPUT FORMAT:
 *   Always "SUCCESS|message" or "ERROR|reason"
 *   Flask reads and passes these back to the frontend as JSON.
 *
 * Team: CodeCrafters | Project: Fresh Picks | SDP-1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "models.h"   /* Struct definitions, file path constants, macros */


/* ═════════════════════════════════════════════════════════════
   COMMAND: update_stock
   PURPOSE: Update the stock_g, price_per_1000g, and validity_days
            for one vegetable identified by veg_id.
   
   argv: update_stock <veg_id> <new_stock_g> <new_price> <new_validity>
   
   EXAMPLE:
     ./inventory update_stock V1001 75000 45.50 10
     → Sets Onion (V1001): stock=75000g, price=₹45.50/kg, shelf-life=10 days

   HOW IT WORKS:
     1. Read ALL rows from products.txt into a Vegetable array.
     2. Find the row with the matching veg_id.
     3. Update stock_g, price_per_1000g, validity_days in-memory.
     4. Rewrite the ENTIRE products.txt file.
        (Plain .txt files don't support in-place field edits.)

   VALIDATION:
     - new_stock_g must be >= 0 (0 = out of stock, that's valid)
     - new_price must be > 0
     - new_validity must be >= 1
   ═════════════════════════════════════════════════════════════ */
void cmd_update_stock(const char* veg_id, int new_stock_g,
                      float new_price, int new_validity) {

    /* ── Validate inputs before touching any file ────────────── */
    if (new_stock_g < 0) { PRINT_ERROR("Stock cannot be negative"); return; }
    if (new_price   <= 0) { PRINT_ERROR("Price must be greater than zero"); return; }
    if (new_validity < 1) { PRINT_ERROR("Validity must be at least 1 day"); return; }

    /* ── Step 1: Read all vegetables into an array ───────────── */
    Vegetable vegs[100];
    int count = 0;

    FILE* fp = fopen(PRODUCTS_FILE, "r");
    if (!fp) { PRINT_ERROR("Could not open products.txt"); return; }

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp) && count < 100) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        /* Parse: veg_id|category|name|stock_g|price_per_1000g|tag|validity_days */
        Vegetable v;
        char* tok = strtok(line, "|");
        if (!tok) continue;
        strncpy(v.veg_id,   tok, MAX_ID_LEN  - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(v.category, tok, MAX_STR_LEN - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(v.name,     tok, MAX_STR_LEN - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        v.stock_g = atoi(tok);                        tok = strtok(NULL, "|");
        if (!tok) continue;
        v.price_per_1000g = atof(tok);                tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(v.tag,      tok, MAX_STR_LEN - 1);  tok = strtok(NULL, "|");
        v.validity_days = tok ? atoi(tok) : 0;

        vegs[count++] = v;
    }
    fclose(fp);

    /* ── Step 2: Find matching veg_id and update its fields ─────── */
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(vegs[i].veg_id, veg_id) == 0) {
            vegs[i].stock_g         = new_stock_g;
            vegs[i].price_per_1000g = new_price;
            vegs[i].validity_days   = new_validity;
            found = 1;
            break;
        }
    }

    if (!found) {
        PRINT_ERROR("Vegetable ID not found");
        return;
    }

    /* ── Step 3: Rewrite PRODUCTS.txt with ALL rows (updated) ─── */
    fp = fopen(PRODUCTS_FILE, "w");
    if (!fp) { PRINT_ERROR("Could not write to products.txt"); return; }

    for (int i = 0; i < count; i++) {
        fprintf(fp, "%s|%s|%s|%d|%.2f|%s|%d\n",
            vegs[i].veg_id,
            vegs[i].category,
            vegs[i].name,
            vegs[i].stock_g,
            vegs[i].price_per_1000g,
            vegs[i].tag,
            vegs[i].validity_days
        );
    }
    fclose(fp);

    PRINT_SUCCESS("Stock updated successfully");
}


/* ═════════════════════════════════════════════════════════════
   COMMAND: update_promo_stock
   PURPOSE: Update the stock_g for one promotional freebie in
            free_inventory.txt. Admin runs this when they have
            physically restocked curry leaves, etc.

   argv: update_promo_stock <vf_id> <new_stock_g>

   EXAMPLE:
     ./inventory update_promo_stock VF101 5000
     → Sets Curry Leaves (VF101) promo stock to 5000g

   HOW IT WORKS:
     1. Read ALL rows from free_inventory.txt into a FreeItem array.
     2. Find the row with the matching vf_id.
     3. Update stock_g in-memory.
     4. Rewrite the ENTIRE free_inventory.txt file.

   NOTE: Only stock_g is updated here.
         min_trigger_amt and free_qty_g are not changed by this command.
         Those are configuration values set by the business team.
   ═════════════════════════════════════════════════════════════ */
void cmd_update_promo_stock(const char* vf_id, int new_stock_g) {

    /* ── Validate: stock cannot be negative ──────────────────── */
    if (new_stock_g < 0) { PRINT_ERROR("Promo stock cannot be negative"); return; }

    /* ── Step 1: Read all free inventory items ───────────────── */
    FreeItem items[20];
    int count = 0;

    FILE* fp = fopen(FREE_INV_FILE, "r");
    if (!fp) { PRINT_ERROR("Could not open free_inventory.txt"); return; }

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp) && count < 20) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        /* Parse: vf_id|name|stock_g|min_trigger_amt|free_qty_g */
        FreeItem fi;
        char* tok = strtok(line, "|");
        if (!tok) continue;
        strncpy(fi.vf_id, tok, MAX_ID_LEN  - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(fi.name,  tok, MAX_STR_LEN - 1);   tok = strtok(NULL, "|");
        if (!tok) continue;
        fi.stock_g         = atoi(tok);             tok = strtok(NULL, "|");
        if (!tok) continue;
        fi.min_trigger_amt = atof(tok);             tok = strtok(NULL, "|");
        fi.free_qty_g      = tok ? atoi(tok) : 0;

        items[count++] = fi;
    }
    fclose(fp);

    /* ── Step 2: Find matching vf_id and update its stock ─────── */
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(items[i].vf_id, vf_id) == 0) {
            items[i].stock_g = new_stock_g;
            found = 1;
            break;
        }
    }

    if (!found) {
        PRINT_ERROR("Promo item ID not found");
        return;
    }

    /* ── Step 3: Rewrite free_inventory.txt with all rows ────── */
    fp = fopen(FREE_INV_FILE, "w");
    if (!fp) { PRINT_ERROR("Could not write to free_inventory.txt"); return; }

    for (int i = 0; i < count; i++) {
        fprintf(fp, "%s|%s|%d|%.2f|%d\n",
            items[i].vf_id,
            items[i].name,
            items[i].stock_g,
            items[i].min_trigger_amt,
            items[i].free_qty_g
        );
    }
    fclose(fp);

    PRINT_SUCCESS("Promo stock updated successfully");
}

/* ═════════════════════════════════════════════════════════════
   COMMAND: list_promo
   PURPOSE: Print all rows from free_inventory.txt so Flask can
            render the Promotional Freebies section in
            admin_inventory.html.

   OUTPUT FORMAT (one row per item, NO header line):
     vf_id|name|stock_g|min_trigger_amt|free_qty_g
     VF101|Curry Leaves|4450|500.00|50
     VF102|Coriander Leaves|3450|500.00|50

   Called by app.py:
     run_c_binary("inventory", ["list_promo"])
   ═════════════════════════════════════════════════════════════ */
void cmd_list_promo(void) {

    FILE* fp = fopen(FREE_INV_FILE, "r");
    if (!fp) { PRINT_ERROR("Could not open free_inventory.txt"); return; }

    /* Print SUCCESS header FIRST on stdout — bridge.py reads line 0 for status */
    printf("SUCCESS|\n");

    char line[MAX_LINE_LEN];
    int  count = 0;

    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        FreeItem fi;
        char* tok = strtok(line, "|");
        if (!tok) continue;
        strncpy(fi.vf_id, tok, MAX_ID_LEN  - 1); fi.vf_id[MAX_ID_LEN  - 1] = '\0';
        tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(fi.name,  tok, MAX_STR_LEN - 1); fi.name[MAX_STR_LEN  - 1] = '\0';
        tok = strtok(NULL, "|");
        if (!tok) continue;
        fi.stock_g         = atoi(tok);
        tok = strtok(NULL, "|");
        if (!tok) continue;
        fi.min_trigger_amt = atof(tok);
        tok = strtok(NULL, "|");
        fi.free_qty_g      = tok ? atoi(tok) : 0;

        printf("%s|%s|%d|%.2f|%d\n",
               fi.vf_id, fi.name, fi.stock_g,
               fi.min_trigger_amt, fi.free_qty_g);
        count++;
    }
    fclose(fp);

    if (count == 0) {
        PRINT_ERROR("No promo items found");
    }
}

/* ═════════════════════════════════════════════════════════════
   MAIN — Command Dispatcher
   ═════════════════════════════════════════════════════════════ */

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PRINT_ERROR("No command. Usage: ./inventory <command> [args]");
        return 1;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "update_stock") == 0) {
        /*
         * Requires: veg_id, new_stock_g, new_price, new_validity
         * argv:     [0]     [1]           [2]         [3]          [4]
         *         ./inventory update_stock V1001       75000        45.50  10
         * argc = 6
         */
        if (argc < 6) {
            PRINT_ERROR("Usage: update_stock <veg_id> <stock_g> <price> <validity>");
            return 1;
        }
        cmd_update_stock(
            argv[2],          /* veg_id       */
            atoi(argv[3]),    /* new_stock_g  */
            atof(argv[4]),    /* new_price    */
            atoi(argv[5])     /* new_validity */
        );

    } else if (strcmp(cmd, "update_promo_stock") == 0) {
        /*
         * Requires: vf_id, new_stock_g
         * argv:     [0]      [1]              [2]     [3]
         *         ./inventory update_promo_stock VF101  5000
         * argc = 4
         */
        if (argc < 4) {
            PRINT_ERROR("Usage: update_promo_stock <vf_id> <new_stock_g>");
            return 1;
        }
        cmd_update_promo_stock(argv[2], atoi(argv[3]));

    } else if (strcmp(cmd, "list_promo") == 0) {
        /*
         * No arguments needed — reads entire free_inventory.txt.
         * argv: [0]            [1]
         *     ./inventory   list_promo
         * argc = 2
         */
        cmd_list_promo();

    } else {
        char err[MAX_STR_LEN];
        snprintf(err, sizeof(err), "Unknown command: %s", cmd);
        PRINT_ERROR(err);
        return 1;
    }

    return 0;
}