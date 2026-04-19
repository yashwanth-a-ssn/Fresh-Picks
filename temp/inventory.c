/*
 * inventory.c - Fresh Picks: Admin Inventory Management (v4 — Binary Storage)
 * =============================================================================
 * This binary handles all ADMIN-ONLY stock management commands.
 * Called by Flask via subprocess.run() like this:
 *   ./inventory <command> [arguments...]
 *
 * ALL persistent I/O is delegated to utils.c (load/save/free SLL functions).
 * Direct fopen / fgets / strtok / fprintf on data files is STRICTLY FORBIDDEN.
 *
 * OUTPUT CONTRACT (unchanged from v3):
 *   Always "SUCCESS|message"  or  "ERROR|reason"
 *   Flask reads and passes these back to the frontend as JSON.
 *
 * COMMANDS (argv[1]):
 *   update_stock       <veg_id> <new_stock_g> <new_price> <new_validity>
 *   update_promo_stock <vf_id>  <new_stock_g>
 *   list_promo
 *
 * Team: CodeCrafters | Project: Fresh Picks | SDP-1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "models.h"   /* Struct definitions, SLL node types, macros, utils.c API */


/* ══════════════════════════════════════════════════════════════════════
 * FUNCTION: cmd_update_stock
 * PURPOSE:  Update the stock_g, price_per_1000g, and validity_days for
 *           one vegetable identified by veg_id.
 * PARAMS:   veg_id — ID of the vegetable to update,
 *           new_stock_g — new stock in grams (>= 0),
 *           new_price   — new price per 1000 g (> 0),
 *           new_validity — new shelf life in days (>= 1)
 * OUTPUT:   SUCCESS|Stock updated successfully   OR   ERROR|reason
 * SCHEMA:   SUCCESS|message
 * ══════════════════════════════════════════════════════════════════════ */
void cmd_update_stock(const char *veg_id, int new_stock_g,
                      float new_price, int new_validity) {
    if (!veg_id || strlen(veg_id) == 0) { PRINT_ERROR("Vegetable ID required");           return; }
    if (new_stock_g  < 0) { PRINT_ERROR("Stock cannot be negative");                      return; }
    if (new_price   <= 0) { PRINT_ERROR("Price must be greater than zero");               return; }
    if (new_validity < 1) { PRINT_ERROR("Validity must be at least 1 day");               return; }

    VegNode *head = load_veg_sll();
    if (!head) { PRINT_ERROR("No vegetables found"); return; }

    VegNode *curr = head;
    int found = 0;
    while (curr != NULL) {
        if (strcmp(curr->data.veg_id, veg_id) == 0) {
            curr->data.stock_g         = new_stock_g;
            curr->data.price_per_1000g = new_price;
            curr->data.validity_days   = new_validity;
            found = 1;
            break;
        }
        curr = curr->next;
    }

    if (!found) {
        free_veg_sll(head);
        PRINT_ERROR("Vegetable ID not found");
        return;
    }

    save_veg_sll(head);
    free_veg_sll(head);
    PRINT_SUCCESS("Stock updated successfully");
}


/* ══════════════════════════════════════════════════════════════════════
 * FUNCTION: cmd_update_promo_stock
 * PURPOSE:  Update the stock_g for one promotional freebie identified
 *           by vf_id. Only stock is changed; min_trigger_amt and
 *           free_qty_g are configuration values and remain untouched.
 * PARAMS:   vf_id — ID of the promo item to update,
 *           new_stock_g — new stock in grams (>= 0)
 * OUTPUT:   SUCCESS|Promo stock updated successfully   OR   ERROR|reason
 * SCHEMA:   SUCCESS|message
 * ══════════════════════════════════════════════════════════════════════ */
void cmd_update_promo_stock(const char *vf_id, int new_stock_g) {
    if (!vf_id || strlen(vf_id) == 0) { PRINT_ERROR("Promo item ID required");    return; }
    if (new_stock_g < 0)              { PRINT_ERROR("Promo stock cannot be negative"); return; }

    FreeItemNode *head = load_free_item_sll();
    if (!head) { PRINT_ERROR("No promo items found"); return; }

    FreeItemNode *curr = head;
    int found = 0;
    while (curr != NULL) {
        if (strcmp(curr->data.vf_id, vf_id) == 0) {
            curr->data.stock_g = new_stock_g;
            found = 1;
            break;
        }
        curr = curr->next;
    }

    if (!found) {
        free_free_item_sll(head);
        PRINT_ERROR("Promo item ID not found");
        return;
    }

    save_free_item_sll(head);
    free_free_item_sll(head);
    PRINT_SUCCESS("Promo stock updated successfully");
}


/* ══════════════════════════════════════════════════════════════════════
 * FUNCTION: cmd_list_promo
 * PURPOSE:  Print all promotional freebie records so Flask can render
 *           the Promotional Freebies section in admin_inventory.html.
 * PARAMS:   (none)
 * OUTPUT:   SUCCESS|\n followed by one record per line, OR ERROR|reason
 * SCHEMA:   vf_id|name|stock_g|min_trigger_amt|free_qty_g
 * ══════════════════════════════════════════════════════════════════════ */
void cmd_list_promo(void) {
    FreeItemNode *head = load_free_item_sll();
    if (!head) { PRINT_ERROR("No promo items found"); return; }

    /* Print SUCCESS header first — bridge.py reads line 0 for status */
    printf("SUCCESS|\n");

    FreeItemNode *curr = head;
    int count = 0;
    while (curr != NULL) {
        printf("%s|%s|%d|%.2f|%d\n",
               curr->data.vf_id,
               curr->data.name,
               curr->data.stock_g,
               curr->data.min_trigger_amt,
               curr->data.free_qty_g);
        count++;
        curr = curr->next;
    }

    free_free_item_sll(head);

    if (count == 0) {
        PRINT_ERROR("No promo items found");
    }
}


/* ══════════════════════════════════════════════════════════════════════
 * MAIN — Command Dispatcher
 * PURPOSE:  Parse argv[1] and route to the appropriate cmd_* function.
 *           Guard clause on argc before any dispatch.
 * ══════════════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        PRINT_ERROR("No command. Usage: ./inventory <command> [args]");
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "update_stock") == 0) {
        /* argv: inventory update_stock <veg_id> <stock_g> <price> <validity>
         * idx:  [0]       [1]           [2]      [3]       [4]     [5]
         * argc >= 6 */
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
        /* argv: inventory update_promo_stock <vf_id> <new_stock_g>
         * idx:  [0]       [1]                [2]     [3]
         * argc >= 4 */
        if (argc < 4) {
            PRINT_ERROR("Usage: update_promo_stock <vf_id> <new_stock_g>");
            return 1;
        }
        cmd_update_promo_stock(argv[2], atoi(argv[3]));

    } else if (strcmp(cmd, "list_promo") == 0) {
        /* argv: inventory list_promo
         * idx:  [0]       [1]
         * argc >= 2 (no additional args needed) */
        cmd_list_promo();

    } else {
        char err[MAX_STR_LEN];
        snprintf(err, sizeof(err), "Unknown command: %s", cmd);
        PRINT_ERROR(err);
        return 1;
    }

    return 0;
}
