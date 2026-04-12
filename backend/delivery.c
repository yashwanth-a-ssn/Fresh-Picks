/*
 * delivery.c - Fresh Picks: Post-Order Delivery Management
 * =========================================================
 * REFACTORED OUT OF order.c (Session 6)
 * This binary handles everything that happens AFTER an order is placed:
 *   - Updating order status (dispatch, deliver, cancel)
 *   - Assigning / re-assigning delivery agents
 *   - Fetching active orders for the delivery dashboard
 *   - Batch slot promotion (JIT auto-dispatch)
 *   - Cancellation with fee deduction logic
 *
 * Called by Flask (app.py) via subprocess.run() like this:
 *   ./delivery <command> [arguments...]
 *
 * WHY A SEPARATE BINARY?
 *   order.c handles the shopping pipeline (add_to_cart → checkout).
 *   Once a receipt row exists in orders.txt, it is THIS binary's domain.
 *   Single Responsibility Principle: each binary owns one lifecycle phase.
 *
 * ─────────────────────────────────────────────────────────────────
 * COMMANDS (argv[1]):
 *
 *   update_status <order_id> <new_status>
 *     → Change the status field of one order in orders.txt.
 *       Valid values: "Order Placed", "Out for Delivery",
 *                     "Delivered", "Cancelled"
 *
 *   cancel_order <order_id>
 *     → Set status to "Cancelled" (wrapper with business-rule check).
 *       Only "Order Placed" orders may be cancelled here.
 *       The ₹50 fee deduction note is handled at Flask/UI level.
 *
 *   get_active_orders
 *     → Dump ALL orders.txt rows whose status is "Order Placed"
 *       or "Out for Delivery". Used by the delivery dashboard.
 *       Output: SUCCESS|<count> then one row per line.
 *
 *   assign_agent <order_id> <boy_id>
 *     → Override the delivery_boy_id on one specific order.
 *       Used when an admin manually re-assigns a delivery agent.
 *
 *   batch_promote_slot <slot_name>
 *     → Flip all "Order Placed" orders for a given slot to
 *       "Out for Delivery". Returns the count promoted.
 *       (Extracted verbatim from order.c's cmd_batch_promote_slot)
 *
 *   list_all_orders
 *     → Dump EVERY order, newest-first, with boy_name + boy_phone
 *       joined from delivery_boys.txt. Used by admin full-view.
 *
 * ─────────────────────────────────────────────────────────────────
 * OUTPUT FORMAT:
 *   Always "SUCCESS|message/count" or "ERROR|reason"
 *   Flask reads stdout and passes it back to the frontend as JSON.
 *
 * Team: CodeCrafters | Project: Fresh Picks | SDP-1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "models.h"   /* Struct definitions, file path constants, macros */


/* ══════════════════════════════════════════════════════════════════════
   INTERNAL HELPER: load_all_orders
   Reads every row from orders.txt into a heap-allocated Order array.
   Returns the count; sets *out_orders (caller must free).
   Returns 0 and sets *out_orders = NULL on failure.
   ══════════════════════════════════════════════════════════════════════ */
static int load_all_orders(Order** out_orders) {
    Order* orders = (Order*)malloc(sizeof(Order) * MAX_ORDERS);
    if (!orders) { *out_orders = NULL; return 0; }

    int count = 0;
    FILE* fp = fopen(ORDERS_FILE, "r");
    if (!fp) { free(orders); *out_orders = NULL; return 0; }

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp) && count < MAX_ORDERS) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

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

        /* Defensive null-termination */
        o.order_id[MAX_ID_LEN - 1]        = '\0';
        o.user_id[MAX_ID_LEN - 1]         = '\0';
        o.delivery_slot[MAX_STR_LEN - 1]  = '\0';
        o.delivery_boy_id[MAX_ID_LEN - 1] = '\0';
        o.status[MAX_STR_LEN - 1]         = '\0';
        o.timestamp[TIMESTAMP_LEN - 1]    = '\0';
        o.items_string[MAX_LINE_LEN - 1]  = '\0';

        orders[count++] = o;
    }
    fclose(fp);
    *out_orders = orders;
    return count;
}

/* ══════════════════════════════════════════════════════════════════════
   INTERNAL HELPER: write_all_orders
   Rewrites orders.txt entirely from the in-memory array.
   Plain text files don't support in-place field edits.
   ══════════════════════════════════════════════════════════════════════ */
static int write_all_orders(Order* orders, int count) {
    FILE* fp = fopen(ORDERS_FILE, "w");
    if (!fp) return 0;
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
    return 1;
}

/* ══════════════════════════════════════════════════════════════════════
   INTERNAL HELPER: load_delivery_boys  (mirrors order.c helper)
   Reads delivery_boys.txt into a flat array.
   ══════════════════════════════════════════════════════════════════════ */
static int load_delivery_boys_local(DeliveryBoy* out_boys, int max) {
    FILE* fp = fopen(DELIVERY_FILE, "r");
    if (!fp) return 0;
    int count = 0;
    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp) && count < max) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;
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
        b.boy_id[MAX_ID_LEN - 1]     = '\0';
        b.name[MAX_STR_LEN - 1]      = '\0';
        b.phone[MAX_STR_LEN - 1]     = '\0';
        b.vehicle_no[MAX_STR_LEN -1] = '\0';
        out_boys[count++] = b;
    }
    fclose(fp);
    return count;
}

/* Find a boy by ID — fills out_name + out_phone */
static void find_boy(DeliveryBoy* boys, int count, const char* id,
                     char* out_name, char* out_phone) {
    for (int i = 0; i < count; i++) {
        if (strcmp(boys[i].boy_id, id) == 0) {
            strncpy(out_name,  boys[i].name,  MAX_STR_LEN - 1);
            strncpy(out_phone, boys[i].phone, MAX_STR_LEN - 1);
            return;
        }
    }
    strncpy(out_name,  "Unknown", MAX_STR_LEN - 1);
    strncpy(out_phone, "N/A",     MAX_STR_LEN - 1);
}


/* ═════════════════════════════════════════════════════════════════════
   COMMAND: update_status
   PURPOSE: Change the status field of ONE order in orders.txt.
   argv: update_status <order_id> <new_status>

   VALID STATUSES:
     "Order Placed" | "Out for Delivery" | "Delivered" | "Cancelled"

   OUTPUT:
     SUCCESS|Status updated
     ERROR|Order not found
   ═════════════════════════════════════════════════════════════════════ */
void cmd_update_status(const char* order_id, const char* new_status) {

    /* Validate the status string before touching the file */
    const char* VALID[] = {
        "Order Placed", "Out for Delivery", "Delivered", "Cancelled"
    };
    int valid = 0;
    for (int i = 0; i < 4; i++) {
        if (strcmp(new_status, VALID[i]) == 0) { valid = 1; break; }
    }
    if (!valid) { PRINT_ERROR("Invalid status value"); return; }

    Order* orders = NULL;
    int count = load_all_orders(&orders);
    if (count == 0 && orders == NULL) {
        PRINT_ERROR("Could not open orders file");
        return;
    }

    int found = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(orders[i].order_id, order_id) == 0) {
            strncpy(orders[i].status, new_status, MAX_STR_LEN - 1);
            orders[i].status[MAX_STR_LEN - 1] = '\0';
            found = 1;
            break;
        }
    }

    if (!found) { free(orders); PRINT_ERROR("Order not found"); return; }

    if (!write_all_orders(orders, count)) {
        free(orders);
        PRINT_ERROR("Could not write to orders file");
        return;
    }
    free(orders);
    PRINT_SUCCESS("Status updated");
}


/* ═════════════════════════════════════════════════════════════════════
   COMMAND: cancel_order
   PURPOSE: Business-rule-aware cancellation.
            Only "Order Placed" orders may be cancelled through this path.
            The ₹50 cancellation fee is noted in the frontend disclaimer;
            the actual deduction lives in the payment/refund subsystem
            (outside C scope for this project).

   argv: cancel_order <order_id>

   OUTPUT:
     SUCCESS|Order cancelled
     ERROR|Only "Order Placed" orders can be cancelled
     ERROR|Order not found
   ═════════════════════════════════════════════════════════════════════ */
void cmd_cancel_order(const char* order_id) {

    Order* orders = NULL;
    int count = load_all_orders(&orders);
    if (count == 0 && orders == NULL) {
        PRINT_ERROR("Could not open orders file");
        return;
    }

    int found = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(orders[i].order_id, order_id) == 0) {
            /* Business rule: only "Order Placed" can be cancelled */
            if (strcmp(orders[i].status, "Order Placed") != 0) {
                free(orders);
                PRINT_ERROR("Only Order Placed orders can be cancelled");
                return;
            }
            strncpy(orders[i].status, "Cancelled", MAX_STR_LEN - 1);
            orders[i].status[MAX_STR_LEN - 1] = '\0';
            found = 1;
            break;
        }
    }

    if (!found) { free(orders); PRINT_ERROR("Order not found"); return; }

    if (!write_all_orders(orders, count)) {
        free(orders);
        PRINT_ERROR("Could not write to orders file");
        return;
    }
    free(orders);
    PRINT_SUCCESS("Order cancelled");
}


/* ═════════════════════════════════════════════════════════════════════
   COMMAND: get_active_orders
   PURPOSE: Return all orders whose status is "Order Placed" or
            "Out for Delivery". Used by the delivery dashboard.
            Enriched with boy_name + boy_phone via C-side JOIN.

   OUTPUT:
     SUCCESS|<count>
     order_id|user_id|total|slot|boy_id|status|timestamp|items|boy_name|boy_phone
     ...
   ═════════════════════════════════════════════════════════════════════ */
void cmd_get_active_orders(void) {

    DeliveryBoy boys[MAX_DELIVERY_BOYS];
    int boy_count = load_delivery_boys_local(boys, MAX_DELIVERY_BOYS);

    Order* orders = NULL;
    int count = load_all_orders(&orders);

    /* Count active orders */
    int active = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(orders[i].status, "Order Placed")     == 0 ||
            strcmp(orders[i].status, "Out for Delivery") == 0) {
            active++;
        }
    }

    printf("SUCCESS|%d\n", active);

    for (int i = 0; i < count; i++) {
        if (strcmp(orders[i].status, "Order Placed")     != 0 &&
            strcmp(orders[i].status, "Out for Delivery") != 0) continue;

        char boy_name[MAX_STR_LEN]  = "Unknown";
        char boy_phone[MAX_STR_LEN] = "N/A";
        find_boy(boys, boy_count, orders[i].delivery_boy_id, boy_name, boy_phone);

        printf("%s|%s|%.2f|%s|%s|%s|%s|%s|%s|%s\n",
            orders[i].order_id,
            orders[i].user_id,
            orders[i].total_amount,
            orders[i].delivery_slot,
            orders[i].delivery_boy_id,
            orders[i].status,
            orders[i].timestamp,
            orders[i].items_string,
            boy_name,
            boy_phone
        );
    }

    free(orders);
}


/* ═════════════════════════════════════════════════════════════════════
   COMMAND: assign_agent
   PURPOSE: Override the delivery_boy_id on one specific order.
            Used when an admin manually re-assigns a delivery agent
            (e.g. original agent is sick).

   argv: assign_agent <order_id> <boy_id>

   OUTPUT:
     SUCCESS|Agent assigned
     ERROR|Order not found
   ═════════════════════════════════════════════════════════════════════ */
void cmd_assign_agent(const char* order_id, const char* boy_id) {

    Order* orders = NULL;
    int count = load_all_orders(&orders);
    if (count == 0 && orders == NULL) {
        PRINT_ERROR("Could not open orders file");
        return;
    }

    int found = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(orders[i].order_id, order_id) == 0) {
            strncpy(orders[i].delivery_boy_id, boy_id, MAX_ID_LEN - 1);
            orders[i].delivery_boy_id[MAX_ID_LEN - 1] = '\0';
            found = 1;
            break;
        }
    }

    if (!found) { free(orders); PRINT_ERROR("Order not found"); return; }

    if (!write_all_orders(orders, count)) {
        free(orders);
        PRINT_ERROR("Could not write to orders file");
        return;
    }
    free(orders);
    PRINT_SUCCESS("Agent assigned");
}


/* ═════════════════════════════════════════════════════════════════════
   COMMAND: batch_promote_slot
   PURPOSE: Flip ALL "Order Placed" orders for a given slot name to
            "Out for Delivery". Returns the count that were promoted.
            (Extracted verbatim from order.c; now lives here.)

   argv: batch_promote_slot <slot_name>
         slot_name: "Morning" | "Afternoon" | "Evening"

   OUTPUT:
     SUCCESS|<n>   where n = number of orders promoted
   ═════════════════════════════════════════════════════════════════════ */
void cmd_batch_promote_slot(const char* slot_name) {

    if (strcmp(slot_name, "Morning")   != 0 &&
        strcmp(slot_name, "Afternoon") != 0 &&
        strcmp(slot_name, "Evening")   != 0) {
        PRINT_ERROR("Invalid slot name");
        return;
    }

    Order* orders = NULL;
    int count = load_all_orders(&orders);
    if (count == 0 && orders == NULL) {
        PRINT_ERROR("Could not open orders file");
        return;
    }

    int promoted = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(orders[i].delivery_slot, slot_name) == 0 &&
            strcmp(orders[i].status, "Order Placed")   == 0) {
            strncpy(orders[i].status, "Out for Delivery", MAX_STR_LEN - 1);
            orders[i].status[MAX_STR_LEN - 1] = '\0';
            promoted++;
        }
    }

    if (!write_all_orders(orders, count)) {
        free(orders);
        PRINT_ERROR("Could not write to orders file");
        return;
    }
    free(orders);

    char result[32];
    snprintf(result, sizeof(result), "%d", promoted);
    PRINT_SUCCESS(result);
}


/* ═════════════════════════════════════════════════════════════════════
   COMMAND: list_all_orders
   PURPOSE: Dump EVERY order from orders.txt, newest-first.
            Unlike get_active_orders, this outputs ALL statuses.
            Used by Flask's full admin view.
            Enriched with boy_name + boy_phone via C-side JOIN.

   OUTPUT:
     SUCCESS|<count>
     order_id|user_id|total|slot|boy_id|status|timestamp|items|boy_name|boy_phone
     ...
   ═════════════════════════════════════════════════════════════════════ */
void cmd_list_all_orders(void) {

    DeliveryBoy boys[MAX_DELIVERY_BOYS];
    int boy_count = load_delivery_boys_local(boys, MAX_DELIVERY_BOYS);

    Order* orders = NULL;
    int count = load_all_orders(&orders);

    if (orders == NULL) {
        /* orders.txt doesn't exist yet */
        printf("SUCCESS|0\n");
        return;
    }

    printf("SUCCESS|%d\n", count);

    /* Output newest-first (last written = highest ORD number) */
    for (int i = count - 1; i >= 0; i--) {
        char boy_name[MAX_STR_LEN]  = "Unknown";
        char boy_phone[MAX_STR_LEN] = "N/A";
        find_boy(boys, boy_count, orders[i].delivery_boy_id, boy_name, boy_phone);

        printf("%s|%s|%.2f|%s|%s|%s|%s|%s|%s|%s\n",
            orders[i].order_id,
            orders[i].user_id,
            orders[i].total_amount,
            orders[i].delivery_slot,
            orders[i].delivery_boy_id,
            orders[i].status,
            orders[i].timestamp,
            orders[i].items_string,
            boy_name,
            boy_phone
        );
    }
    free(orders);
}

/* ─── Slot priority helper (Morning first) ────────────────────────────── */
static int get_slot_priority(const char* slot) {
    if (strcmp(slot, "Morning")   == 0) return 1;
    if (strcmp(slot, "Afternoon") == 0) return 2;
    return 3; /* Evening or unknown */
}

/* ─── qsort comparator: slot ASC, then timestamp ASC ─────────────────── */
static int compare_orders_priority(const void* a, const void* b) {
    const Order* oa = (const Order*)a;
    const Order* ob = (const Order*)b;
    if (oa->slot_priority != ob->slot_priority)
        return oa->slot_priority - ob->slot_priority;
    return strcmp(oa->timestamp, ob->timestamp);
}

/* ═════════════════════════════════════════════════════════════════════
   COMMAND: list_all_orders_sorted
   PURPOSE: Dump ALL orders sorted by slot priority (Morning first),
            then by timestamp ASC as tiebreaker.
            Includes ALL statuses (Delivered, Cancelled included).
            Enriched with boy_name + boy_phone via C-side JOIN.

   OUTPUT:
     SUCCESS|<count>
     order_id|user_id|total|slot|boy_id|status|timestamp|items|boy_name|boy_phone
     ...
   ═════════════════════════════════════════════════════════════════════ */
void cmd_list_all_orders_sorted(void) {
    DeliveryBoy boys[MAX_DELIVERY_BOYS];
    int boy_count = load_delivery_boys_local(boys, MAX_DELIVERY_BOYS);

    Order* orders = NULL;
    int count = load_all_orders(&orders);

    if (orders == NULL) {
        printf("SUCCESS|0\n");
        return;
    }

    /* Stamp slot_priority on each order before sorting */
    for (int i = 0; i < count; i++)
        orders[i].slot_priority = get_slot_priority(orders[i].delivery_slot);

    /* Sort: primary = slot_priority ASC, secondary = timestamp ASC */
    qsort(orders, count, sizeof(Order), compare_orders_priority);

    printf("SUCCESS|%d\n", count);

    for (int i = 0; i < count; i++) {
        char boy_name[MAX_STR_LEN]  = "Unknown";
        char boy_phone[MAX_STR_LEN] = "N/A";
        find_boy(boys, boy_count, orders[i].delivery_boy_id, boy_name, boy_phone);

        printf("%s|%s|%.2f|%s|%s|%s|%s|%s|%s|%s\n",
            orders[i].order_id,
            orders[i].user_id,
            orders[i].total_amount,
            orders[i].delivery_slot,
            orders[i].delivery_boy_id,
            orders[i].status,
            orders[i].timestamp,
            orders[i].items_string,
            boy_name,
            boy_phone
        );
    }
    free(orders);
}

/* ═════════════════════════════════════════════════════════════════════
   MAIN — Command Dispatcher
   ═════════════════════════════════════════════════════════════════════ */
int main(int argc, char* argv[]) {

    if (argc < 2) {
        PRINT_ERROR("No command. Usage: ./delivery <command> [args]");
        return 1;
    }

    const char* cmd = argv[1];

    /* ── update_status <order_id> <new_status> ── */
    if (strcmp(cmd, "update_status") == 0) {
        if (argc < 4) {
            PRINT_ERROR("Usage: update_status <order_id> <new_status>");
            return 1;
        }
        cmd_update_status(argv[2], argv[3]);

    /* ── cancel_order <order_id> ── */
    } else if (strcmp(cmd, "cancel_order") == 0) {
        if (argc < 3) {
            PRINT_ERROR("Usage: cancel_order <order_id>");
            return 1;
        }
        cmd_cancel_order(argv[2]);

    /* ── get_active_orders ── */
    } else if (strcmp(cmd, "get_active_orders") == 0) {
        cmd_get_active_orders();

    /* ── assign_agent <order_id> <boy_id> ── */
    } else if (strcmp(cmd, "assign_agent") == 0) {
        if (argc < 4) {
            PRINT_ERROR("Usage: assign_agent <order_id> <boy_id>");
            return 1;
        }
        cmd_assign_agent(argv[2], argv[3]);

    /* ── batch_promote_slot <slot_name> ── */
    } else if (strcmp(cmd, "batch_promote_slot") == 0) {
        if (argc < 3) {
            PRINT_ERROR("Usage: batch_promote_slot <slot_name>");
            return 1;
        }
        cmd_batch_promote_slot(argv[2]);

    /* ── list_all_orders ── */
    } else if (strcmp(cmd, "list_all_orders") == 0) {
        cmd_list_all_orders();

    /* ── list_all_orders_sorted ── */
    } else if (strcmp(cmd, "list_all_orders_sorted") == 0) {
        cmd_list_all_orders_sorted();

    } else {
        char err[MAX_STR_LEN];
        snprintf(err, sizeof(err), "Unknown command: %s", cmd);
        PRINT_ERROR(err);
        return 1;
    }

    return 0;
}
