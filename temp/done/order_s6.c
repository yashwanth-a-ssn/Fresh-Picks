/*
 * order.c - Fresh Picks: Shopping Cart, Payment & Order Management (v4)
 * ======================================================================
 * CHANGELOG (v4 — Session 6):
 *   1. NEW cmd_batch_promote_slot():
 *      - Reads orders.txt ONCE, updates ALL "Order Placed" orders for
 *        a given slot to "Out for Delivery", rewrites the file ONCE.
 *      - Idempotent: orders already "Out for Delivery" are left alone.
 *      - Fixes the N+1 subprocess anti-pattern from Session 5.
 *   2. FIXED cmd_admin_orders():
 *      - Now opens delivery_boys.txt INTERNALLY (C-side JOIN).
 *      - Appends boy_name and boy_phone to each output line.
 *      - Flask no longer needs to read delivery_boys.txt at all.
 *   3. Output format of admin_orders changed:
 *      OLD: order_id|user_id|total|slot|boy_id|status|timestamp|items
 *      NEW: order_id|user_id|total|slot|boy_id|status|timestamp|items|boy_name|boy_phone
 *
 * Team: CodeCrafters | Project: Fresh Picks | SDP-1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "models.h"


/* ═════════════════════════════════════════════════════════════
   SECTION 1: HELPER FUNCTIONS  (unchanged from v3)
   ═════════════════════════════════════════════════════════════ */

/* ... (keep all v3 helpers exactly as-is) ... */


/* ══════════════════════════════════════════════════════════════
   NEW HELPER: load_delivery_boys
   PURPOSE: Read delivery_boys.txt into a flat array.
            Used by cmd_admin_orders for the C-side JOIN so Flask
            never has to touch delivery_boys.txt directly.

   PARAMS:
     out_boys  — caller-provided array to fill
     max       — max size of out_boys[]
   RETURNS: number of boys loaded
   ══════════════════════════════════════════════════════════════ */
int load_delivery_boys(DeliveryBoy* out_boys, int max) {
    /* Step 1: Open the file */
    FILE* fp = fopen(DELIVERY_FILE, "r");
    if (!fp) return 0;  /* If file missing, return 0 (no boys) */

    int count = 0;
    char line[MAX_LINE_LEN];

    /* Step 2: Read one row per line */
    while (fgets(line, sizeof(line), fp) && count < max) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        /* Step 3: Parse pipe-delimited fields:
           boy_id|name|phone|vehicle_no|is_active|last_assigned */
        DeliveryBoy b;
        char* tok = strtok(line, "|");
        if (!tok) continue;
        strncpy(b.boy_id,     tok, MAX_ID_LEN  - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(b.name,       tok, MAX_STR_LEN - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(b.phone,      tok, MAX_STR_LEN - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(b.vehicle_no, tok, MAX_STR_LEN - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        b.is_active     = atoi(tok);                    tok = strtok(NULL, "|");
        b.last_assigned = tok ? atoi(tok) : 0;

        /* Null-terminate all strings defensively */
        b.boy_id[MAX_ID_LEN - 1]     = '\0';
        b.name[MAX_STR_LEN - 1]      = '\0';
        b.phone[MAX_STR_LEN - 1]     = '\0';
        b.vehicle_no[MAX_STR_LEN -1] = '\0';

        out_boys[count++] = b;
    }
    fclose(fp);
    return count;  /* Total boys loaded */
}

/*
 * HELPER: find_boy_by_id
 * PURPOSE: Linear search through the boys[] array for a matching boy_id.
 *          Used by cmd_admin_orders to JOIN the order row with the boy's
 *          name and phone — entirely in C, no Python file reads needed.
 *
 * PARAMS:
 *   boys      — array of DeliveryBoy structs
 *   count     — number of entries in that array
 *   boy_id    — the ID to search for (e.g. "D001")
 *   out_name  — filled with the boy's name if found
 *   out_phone — filled with the boy's phone if found
 * RETURNS: 1 if found, 0 if not
 */
int find_boy_by_id(DeliveryBoy* boys, int count,
                   const char* boy_id,
                   char* out_name, char* out_phone) {
    for (int i = 0; i < count; i++) {
        if (strcmp(boys[i].boy_id, boy_id) == 0) {
            strncpy(out_name,  boys[i].name,  MAX_STR_LEN - 1);
            strncpy(out_phone, boys[i].phone, MAX_STR_LEN - 1);
            out_name[MAX_STR_LEN - 1]  = '\0';
            out_phone[MAX_STR_LEN - 1] = '\0';
            return 1;
        }
    }
    /* Not found — use safe defaults */
    strncpy(out_name,  "Unknown",    MAX_STR_LEN - 1);
    strncpy(out_phone, "N/A",        MAX_STR_LEN - 1);
    return 0;
}


/* ═════════════════════════════════════════════════════════════
   SECTION 2: COMMAND HANDLER FUNCTIONS
   ═════════════════════════════════════════════════════════════ */

/* ... keep cmd_list_products, cmd_add_to_cart, cmd_view_cart,
        cmd_remove_item, cmd_checkout, cmd_get_orders unchanged ... */


/*
 * COMMAND: admin_orders  (v4 — C-Side JOIN with delivery_boys.txt)
 * PURPOSE: Load ACTIVE orders into a Min-Heap, print them priority-sorted.
 *          NEW: enriches each output line with boy_name and boy_phone
 *          by doing a C-side JOIN with delivery_boys.txt.
 *          Flask no longer needs to open delivery_boys.txt.
 *
 * CHANGES FROM v3:
 *   - Loads delivery boys ONCE before the loop (O(d) read, not O(n*d))
 *   - Each output line now has 2 extra pipe-delimited fields:
 *     ...items|boy_name|boy_phone
 *
 * OUTPUT:
 *   SUCCESS|<count>
 *   order_id|user_id|total|slot|boy_id|status|timestamp|items|boy_name|boy_phone
 *   ...
 */
void cmd_admin_orders(void) {

    /* ── Step 1: Load ALL delivery boys into an array (ONE file read) ─── */
    /* WHY LOAD FIRST? We do a JOIN below: for each order we look up the
       boy's name/phone. Loading boys once and searching in-memory is O(n*d).
       Reopening the file per-order would be O(n) file opens — far worse. */
    DeliveryBoy boys[MAX_DELIVERY_BOYS];
    int boy_count = load_delivery_boys(boys, MAX_DELIVERY_BOYS);

    /* ── Step 2: Open orders.txt and build the Min-Heap ────────────────── */
    FILE* fp = fopen(ORDERS_FILE, "r");
    if (!fp) { PRINT_ERROR("Could not open orders file"); return; }

    MinHeap heap;
    heap.size = 0;  /* Initialise: no elements yet */

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        /* ── Parse the 8 fields of an order row ── */
        Order o;
        char* tok = strtok(line, "|");
        if (!tok) continue;
        strncpy(o.order_id,        tok, MAX_ID_LEN  - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(o.user_id,         tok, MAX_ID_LEN  - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        o.total_amount = atof(tok);                          tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(o.delivery_slot,   tok, MAX_STR_LEN - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(o.delivery_boy_id, tok, MAX_ID_LEN  - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(o.status,          tok, MAX_STR_LEN - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(o.timestamp,       tok, TIMESTAMP_LEN - 1); tok = strtok(NULL, "|");
        if (tok) strncpy(o.items_string, tok, MAX_LINE_LEN - 1);
        else     o.items_string[0] = '\0';

        /* Null-terminate all strings defensively */
        o.order_id[MAX_ID_LEN - 1]        = '\0';
        o.user_id[MAX_ID_LEN - 1]         = '\0';
        o.delivery_slot[MAX_STR_LEN - 1]  = '\0';
        o.delivery_boy_id[MAX_ID_LEN - 1] = '\0';
        o.status[MAX_STR_LEN - 1]         = '\0';
        o.timestamp[TIMESTAMP_LEN - 1]    = '\0';
        o.items_string[MAX_LINE_LEN - 1]  = '\0';

        /* ── Priority Filter: only ACTIVE orders ── */
        int is_active =
            (strcmp(o.status, "Order Placed")     == 0) ||
            (strcmp(o.status, "Out for Delivery") == 0);
        if (!is_active) continue;

        o.slot_priority = get_slot_priority(o.delivery_slot);
        heap_insert(&heap, o);
    }
    fclose(fp);

    /* ── Step 3: Print header then extract orders in priority order ─── */
    printf("SUCCESS|%d\n", heap.size);

    Order out;
    while (heap_extract_min(&heap, &out)) {
        /* ── C-Side JOIN: look up boy's name and phone ─────────────────
           WHY HERE (not in Flask)?
             The constraint says "Flask must NOT read delivery_boys.txt."
             Doing the JOIN in C means one binary produces the complete
             row — Flask just forwards JSON to the browser. */
        char boy_name[MAX_STR_LEN]  = "Unknown";
        char boy_phone[MAX_STR_LEN] = "N/A";
        find_boy_by_id(boys, boy_count, out.delivery_boy_id,
                       boy_name, boy_phone);

        /* Output format (10 fields):
           order_id|user_id|total|slot|boy_id|status|timestamp|items|boy_name|boy_phone */
        printf("%s|%s|%.2f|%s|%s|%s|%s|%s|%s|%s\n",
            out.order_id,
            out.user_id,
            out.total_amount,
            out.delivery_slot,
            out.delivery_boy_id,
            out.status,
            out.timestamp,
            out.items_string,
            boy_name,    /* NEW — from delivery_boys.txt JOIN */
            boy_phone    /* NEW — from delivery_boys.txt JOIN */
        );
    }
}


/*
 * COMMAND: batch_promote_slot  (NEW in v4 — Fixes N+1 Subprocess)
 * PURPOSE: Atomically promote ALL "Order Placed" orders for a given
 *          delivery slot to "Out for Delivery" in a SINGLE file rewrite.
 *
 * WHY THIS EXISTS:
 *   The old approach called update_order_status once PER order from Python,
 *   meaning N subprocess spawns for N orders (the "N+1 subprocess problem").
 *   This single command reads once, updates all matching rows in-memory,
 *   and writes once — O(1) subprocess calls regardless of order count.
 *
 * IDEMPOTENCY:
 *   Orders already "Out for Delivery" are skipped silently.
 *   Running this command twice produces the same result as running it once.
 *
 * argv: batch_promote_slot <slot_name>
 *   slot_name: "Morning", "Afternoon", or "Evening"
 *
 * OUTPUT:
 *   SUCCESS|<n>     (n = number of orders actually promoted this run)
 *   ERROR|<reason>  (if file could not be opened)
 */
void cmd_batch_promote_slot(const char* slot_name) {

    /* ── Step 1: Validate the slot name ────────────────────────────────── */
    /* Only accept recognised slot names to prevent garbage data in the DB */
    if (strcmp(slot_name, "Morning")   != 0 &&
        strcmp(slot_name, "Afternoon") != 0 &&
        strcmp(slot_name, "Evening")   != 0) {
        PRINT_ERROR("Invalid slot name. Use: Morning, Afternoon, Evening");
        return;
    }

    /* ── Step 2: Load ALL orders into a heap-allocated array ────────────── */
    /* We use malloc (heap memory) instead of a stack array to avoid
       stack overflow — Order struct is large, MAX_ORDERS=200 of them
       on the stack would be risky on systems with small stack sizes. */
    Order* orders = (Order*)malloc(sizeof(Order) * MAX_ORDERS);
    if (!orders) { PRINT_ERROR("Memory allocation failed"); return; }
    int count = 0;

    FILE* fp = fopen(ORDERS_FILE, "r");
    if (!fp) {
        free(orders);
        PRINT_ERROR("Could not open orders file");
        return;
    }

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp) && count < MAX_ORDERS) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        /* Parse all 8 pipe-delimited fields of one order row */
        Order o;
        char* tok = strtok(line, "|");
        if (!tok) continue;
        strncpy(o.order_id,        tok, MAX_ID_LEN  - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(o.user_id,         tok, MAX_ID_LEN  - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        o.total_amount = atof(tok);                          tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(o.delivery_slot,   tok, MAX_STR_LEN - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(o.delivery_boy_id, tok, MAX_ID_LEN  - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(o.status,          tok, MAX_STR_LEN - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(o.timestamp,       tok, TIMESTAMP_LEN - 1); tok = strtok(NULL, "|");
        if (tok) strncpy(o.items_string, tok, MAX_LINE_LEN - 1);
        else     o.items_string[0] = '\0';

        /* Defensive null-termination for all string fields */
        o.order_id[MAX_ID_LEN - 1]        = '\0';
        o.user_id[MAX_ID_LEN - 1]         = '\0';
        o.delivery_slot[MAX_STR_LEN - 1]  = '\0';
        o.delivery_boy_id[MAX_ID_LEN - 1] = '\0';
        o.status[MAX_STR_LEN - 1]         = '\0';
        o.timestamp[TIMESTAMP_LEN - 1]    = '\0';
        o.items_string[MAX_LINE_LEN - 1]  = '\0';

        o.slot_priority = get_slot_priority(o.delivery_slot);
        orders[count++] = o;
    }
    fclose(fp);

    /* ── Step 3: In-memory update — mark matching orders ────────────────── */
    /* We ONLY change orders that are:
         (a) in the requested slot  AND
         (b) currently "Order Placed"  (idempotency: skip "Out for Delivery") */
    int promoted = 0;  /* Counter of how many were actually changed */

    for (int i = 0; i < count; i++) {
        int slot_matches   = (strcmp(orders[i].delivery_slot, slot_name) == 0);
        int is_order_placed = (strcmp(orders[i].status, "Order Placed") == 0);

        if (slot_matches && is_order_placed) {
            /* Promote this order */
            strncpy(orders[i].status, "Out for Delivery", MAX_STR_LEN - 1);
            orders[i].status[MAX_STR_LEN - 1] = '\0';
            promoted++;  /* Track how many we changed */
        }
        /* If status is already "Out for Delivery" — do nothing (idempotent) */
    }

    /* ── Step 4: Rewrite orders.txt ONCE with all updated rows ──────────── */
    /* WHY REWRITE THE WHOLE FILE?
       Plain .txt files have no concept of "update row N in place" like SQL.
       We must read everything, change what we need, then write everything back.
       The key benefit over the old N+1 approach: we do this EXACTLY ONCE
       regardless of how many orders need promoting. */
    fp = fopen(ORDERS_FILE, "w");
    if (!fp) {
        free(orders);
        PRINT_ERROR("Could not write to orders file");
        return;
    }

    for (int i = 0; i < count; i++) {
        fprintf(fp, "%s|%s|%.2f|%s|%s|%s|%s|%s\n",
            orders[i].order_id,
            orders[i].user_id,
            orders[i].total_amount,
            orders[i].delivery_slot,
            orders[i].delivery_boy_id,
            orders[i].status,
            orders[i].timestamp,
            orders[i].items_string
        );
    }
    fclose(fp);
    free(orders);

    /* ── Step 5: Report how many orders were promoted ────────────────────── */
    /* Flask uses this number to show the JIT banner:
       "System: 3 orders auto-promoted to Out for Delivery" */
    char result[64];
    snprintf(result, sizeof(result), "%d", promoted);
    PRINT_SUCCESS(result);
    /* Output example: SUCCESS|3  (means 3 orders were promoted) */
    /*                 SUCCESS|0  (means nothing to do — idempotent) */
}


/* ═════════════════════════════════════════════════════════════
   SECTION 3: MAIN — Command Dispatcher  (v4)
   ═════════════════════════════════════════════════════════════ */

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PRINT_ERROR("No command provided. Usage: ./order <command> [args]");
        return 1;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "list_products") == 0) {
        cmd_list_products();

    } else if (strcmp(cmd, "add_to_cart") == 0) {
        if (argc < 5) { PRINT_ERROR("Usage: add_to_cart <user_id> <veg_id> <grams>"); return 1; }
        cmd_add_to_cart(argv[2], argv[3], atoi(argv[4]));

    } else if (strcmp(cmd, "view_cart") == 0) {
        if (argc < 3) { PRINT_ERROR("Usage: view_cart <user_id>"); return 1; }
        cmd_view_cart(argv[2]);

    } else if (strcmp(cmd, "remove_item") == 0) {
        if (argc < 4) { PRINT_ERROR("Usage: remove_item <user_id> <veg_id>"); return 1; }
        cmd_remove_item(argv[2], argv[3]);

    } else if (strcmp(cmd, "checkout") == 0) {
        if (argc < 4) { PRINT_ERROR("Usage: checkout <user_id> <slot>"); return 1; }
        cmd_checkout(argv[2], argv[3]);

    } else if (strcmp(cmd, "get_orders") == 0) {
        if (argc < 3) { PRINT_ERROR("Usage: get_orders <user_id>"); return 1; }
        cmd_get_orders(argv[2]);

    } else if (strcmp(cmd, "admin_orders") == 0) {
        cmd_admin_orders();

    } else if (strcmp(cmd, "update_order_status") == 0) {
        if (argc < 4) { PRINT_ERROR("Usage: update_order_status <order_id> <status>"); return 1; }
        cmd_update_order_status(argv[2], argv[3]);

    } else if (strcmp(cmd, "batch_promote_slot") == 0) {
        /* NEW v4: Bulk JIT promotion — one call promotes ALL orders for the slot */
        if (argc < 3) { PRINT_ERROR("Usage: batch_promote_slot <slot_name>"); return 1; }
        cmd_batch_promote_slot(argv[2]);

    } else {
        char err[MAX_STR_LEN];
        snprintf(err, sizeof(err), "Unknown command: %s", cmd);
        PRINT_ERROR(err);
        return 1;
    }

    return 0;
}