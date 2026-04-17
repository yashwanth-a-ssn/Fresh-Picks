/*
 * receipt.c - Fresh Picks: Order Receipt Data Extractor (v4)
 * ===========================================================
 * Standalone C binary.  Takes an order_id as the ONLY argument.
 * Looks up the order in orders.dat (via utils.c), then the user
 * in users.dat, then the delivery boy in delivery_boys.dat —
 * all through in-memory SLL traversal.
 *
 * v4 MIGRATION NOTES:
 *   - All data is now read through load_*_sll() / free_*_sll()
 *     from utils.c.  No direct fopen/strtok on .txt files.
 *   - The three former helper functions (find_order, find_user,
 *     find_delivery_boy) are reimplemented as SLL traversals.
 *
 * OUTPUT (single line to stdout):
 *   SUCCESS|order_id|user_id|full_name|user_phone|user_email|
 *           address|slot|status|timestamp|boy_name|boy_phone|
 *           total|items_string
 *
 * ERROR:
 *   ERROR|reason message
 *
 * COMPILE:
 *   gcc -Wall -Wextra -o receipt receipt.c utils.c -lm
 *
 * USAGE:
 *   ./receipt ORD1001
 *
 * Team: CodeCrafters | Project: Fresh Picks | SDP-1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "models.h"


/* ═════════════════════════════════════════════════════════════
   HELPER FUNCTIONS
   ═════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: sll_find_order
 * PURPOSE:  Walk the already-loaded OrderNode SLL and fill `out` with
 *           the first Order whose order_id matches.
 * PARAMS:   head     — head of the loaded OrderNode SLL
 *           order_id — ID to search for (e.g. "ORD1001")
 *           out      — caller-provided Order struct to fill on match
 * OUTPUT:   (none — returns 1 on success, 0 if not found)
 * SCHEMA:   (none)
 */
int sll_find_order(OrderNode* head, const char* order_id, Order* out) {
    OrderNode* curr = head;
    while (curr != NULL) {
        if (strcmp(curr->data.order_id, order_id) == 0) {
            *out = curr->data;
            return 1;
        }
        curr = curr->next;
    }
    return 0;
}

/*
 * FUNCTION: sll_find_user
 * PURPOSE:  Walk the already-loaded UserNode SLL and fill `out` with
 *           the first User whose user_id matches.
 * PARAMS:   head    — head of the loaded UserNode SLL
 *           user_id — ID to search for (e.g. "U1001")
 *           out     — caller-provided User struct to fill on match
 * OUTPUT:   (none — returns 1 on success, 0 if not found)
 * SCHEMA:   (none)
 */
int sll_find_user(UserNode* head, const char* user_id, User* out) {
    UserNode* curr = head;
    while (curr != NULL) {
        if (strcmp(curr->data.user_id, user_id) == 0) {
            *out = curr->data;
            return 1;
        }
        curr = curr->next;
    }
    return 0;
}

/*
 * FUNCTION: sll_find_delivery_boy
 * PURPOSE:  Walk the already-loaded DeliveryBoyNode SLL and fill
 *           out_name / out_phone for the matching boy_id.
 *           Falls back to "Unknown" / "N/A" if not found.
 * PARAMS:   head      — head of the loaded DeliveryBoyNode SLL
 *           boy_id    — ID to search for (e.g. "D1001")
 *           out_name  — buffer for the boy's name
 *           out_phone — buffer for the boy's phone
 * OUTPUT:   (none — writes into out_name / out_phone)
 * SCHEMA:   (none)
 */
void sll_find_delivery_boy(DeliveryBoyNode* head, const char* boy_id,
                           char* out_name, char* out_phone) {
    strncpy(out_name,  "Unknown", MAX_STR_LEN - 1);
    strncpy(out_phone, "N/A",     MAX_STR_LEN - 1);
    out_name [MAX_STR_LEN - 1] = '\0';
    out_phone[MAX_STR_LEN - 1] = '\0';

    DeliveryBoyNode* curr = head;
    while (curr != NULL) {
        if (strcmp(curr->data.boy_id, boy_id) == 0) {
            strncpy(out_name,  curr->data.name,  MAX_STR_LEN - 1);
            strncpy(out_phone, curr->data.phone, MAX_STR_LEN - 1);
            out_name [MAX_STR_LEN - 1] = '\0';
            out_phone[MAX_STR_LEN - 1] = '\0';
            return;
        }
        curr = curr->next;
    }
}


/* ═════════════════════════════════════════════════════════════
   MAIN
   ═════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: main
 * PURPOSE:  Entry point for the receipt binary.  Accepts one argument
 *           (order_id), loads all three SLLs, performs lookups, prints
 *           the unified receipt line to stdout.
 * PARAMS:   argc — must be 2; argv[1] — order_id string
 * OUTPUT:   SUCCESS|order_id|user_id|full_name|user_phone|user_email|
 *                   address|slot|status|timestamp|boy_name|boy_phone|
 *                   total|items_string
 *           ERROR|reason
 * SCHEMA:   order_id|user_id|full_name|user_phone|user_email|address|
 *           slot|status|timestamp|boy_name|boy_phone|total|items_string
 */
int main(int argc, char* argv[]) {

    if (argc < 2) {
        PRINT_ERROR("Usage: ./receipt <order_id>");
        return 1;
    }

    const char* order_id = argv[1];

    /* ── Step 1: Load order SLL and find the order ───────────────── */
    OrderNode* ord_head = load_order_sll();
    if (!ord_head) {
        PRINT_ERROR("No orders found");
        return 1;
    }

    Order order;
    memset(&order, 0, sizeof(Order));
    if (!sll_find_order(ord_head, order_id, &order)) {
        free_order_sll(ord_head);
        char err[MAX_STR_LEN];
        snprintf(err, sizeof(err), "Order not found: %s", order_id);
        PRINT_ERROR(err);
        return 1;
    }
    free_order_sll(ord_head);

    /* ── Step 2: Load user SLL and find the user ─────────────────── */
    UserNode* usr_head = load_user_sll();
    if (!usr_head) {
        PRINT_ERROR("No users found");
        return 1;
    }

    User user;
    memset(&user, 0, sizeof(User));
    if (!sll_find_user(usr_head, order.user_id, &user)) {
        free_user_sll(usr_head);
        char err[MAX_STR_LEN];
        snprintf(err, sizeof(err), "User not found: %s", order.user_id);
        PRINT_ERROR(err);
        return 1;
    }
    free_user_sll(usr_head);

    /* ── Step 3: Load delivery boy SLL and find the assigned boy ─── */
    DeliveryBoyNode* boy_head = load_delivery_boy_sll();

    char boy_name [MAX_STR_LEN] = "Unknown";
    char boy_phone[MAX_STR_LEN] = "N/A";
    sll_find_delivery_boy(boy_head, order.delivery_boy_id,
                          boy_name, boy_phone);

    free_delivery_boy_sll(boy_head);

    /*
     * ── Step 4: Print unified pipe-delimited receipt line ──────────
     *
     * FORMAT (13 fields after SUCCESS):
     *   SUCCESS|order_id|user_id|full_name|user_phone|user_email|
     *           address|slot|status|timestamp|boy_name|boy_phone|
     *           total|items_string
     *
     * NOTE: address is a raw comma-separated string from users.dat
     *       e.g. "No 11 - Flat No 11,Elumalai Street,West Tambaram,600045"
     *       Python splits on commas to render 4 address lines.
     */
    printf("SUCCESS|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%.2f|%s\n",
        order.order_id,
        order.user_id,
        user.full_name,
        user.phone,
        user.email,
        user.address,
        order.delivery_slot,
        order.status,
        order.timestamp,
        boy_name,
        boy_phone,
        order.total_amount,
        order.items_string
    );

    return 0;
}
