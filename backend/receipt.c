/*
 * receipt.c - Fresh Picks: Order Receipt Data Extractor
 * ======================================================
 * Standalone C binary. Takes an order_id as the ONLY argument.
 * Looks up the order in orders.txt, then the user in users.txt,
 * then the delivery boy in delivery_boys.txt.
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
 *   gcc -Wall -Wextra -o receipt receipt.c -lm
 *   (No ds_utils.c needed — this binary only reads files.)
 *
 * USAGE:
 *   ./receipt ORD125
 *
 * Team: CodeCrafters | Project: Fresh Picks | SDP-1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "models.h"


/* ═════════════════════════════════════════════════════════════
   HELPER: parse_order_line
   Parse one pipe-delimited line from orders.txt into an Order.
   Format: order_id|user_id|total|slot|boy_id|status|timestamp|items_string
   Returns 1 on success, 0 on parse failure.
   ═════════════════════════════════════════════════════════════ */
int parse_order_line(char* line, Order* out) {
    char* tok;

    tok = strtok(line, "|"); if (!tok) return 0;
    strncpy(out->order_id,       tok, MAX_ID_LEN  - 1);
    out->order_id[MAX_ID_LEN - 1] = '\0';

    tok = strtok(NULL, "|"); if (!tok) return 0;
    strncpy(out->user_id,        tok, MAX_ID_LEN  - 1);
    out->user_id[MAX_ID_LEN - 1] = '\0';

    tok = strtok(NULL, "|"); if (!tok) return 0;
    out->total_amount = atof(tok);

    tok = strtok(NULL, "|"); if (!tok) return 0;
    strncpy(out->delivery_slot,  tok, MAX_STR_LEN - 1);
    out->delivery_slot[MAX_STR_LEN - 1] = '\0';

    tok = strtok(NULL, "|"); if (!tok) return 0;
    strncpy(out->delivery_boy_id, tok, MAX_ID_LEN - 1);
    out->delivery_boy_id[MAX_ID_LEN - 1] = '\0';

    tok = strtok(NULL, "|"); if (!tok) return 0;
    strncpy(out->status,         tok, MAX_STR_LEN - 1);
    out->status[MAX_STR_LEN - 1] = '\0';

    tok = strtok(NULL, "|"); if (!tok) return 0;
    strncpy(out->timestamp,      tok, TIMESTAMP_LEN - 1);
    out->timestamp[TIMESTAMP_LEN - 1] = '\0';

    /* items_string is the rest of the line (may contain colons and commas) */
    tok = strtok(NULL, "\n"); /* take everything to end of line */
    if (tok) {
        strncpy(out->items_string, tok, MAX_LINE_LEN - 1);
        out->items_string[MAX_LINE_LEN - 1] = '\0';
    } else {
        out->items_string[0] = '\0';
    }

    return 1;
}


/* ═════════════════════════════════════════════════════════════
   HELPER: find_order
   Linear search through orders.txt for matching order_id.
   Returns 1 if found and fills `out`, else 0.
   ═════════════════════════════════════════════════════════════ */
int find_order(const char* order_id, Order* out) {
    FILE* fp = fopen(ORDERS_FILE, "r");
    if (!fp) return 0;

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        /* Peek at the first field without consuming the line */
        char peek[MAX_ID_LEN];
        strncpy(peek, line, MAX_ID_LEN - 1);
        peek[MAX_ID_LEN - 1] = '\0';
        char* pipe = strchr(peek, '|');
        if (pipe) *pipe = '\0';

        if (strcmp(peek, order_id) == 0) {
            /* Found — now parse the full line */
            char buf[MAX_LINE_LEN];
            strncpy(buf, line, MAX_LINE_LEN - 1);
            buf[MAX_LINE_LEN - 1] = '\0';
            fclose(fp);
            return parse_order_line(buf, out);
        }
    }
    fclose(fp);
    return 0;
}


/* ═════════════════════════════════════════════════════════════
   HELPER: find_user
   Linear search through users.txt for matching user_id.
   Format: user_id|username|password|full_name|email|phone|address
   Returns 1 if found and fills `out`, else 0.
   ═════════════════════════════════════════════════════════════ */
int find_user(const char* user_id, User* out) {
    FILE* fp = fopen(USERS_FILE, "r");
    if (!fp) return 0;

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        char buf[MAX_LINE_LEN];
        strncpy(buf, line, MAX_LINE_LEN - 1);
        buf[MAX_LINE_LEN - 1] = '\0';

        char* tok = strtok(buf, "|"); if (!tok) continue;
        strncpy(out->user_id,   tok, MAX_ID_LEN  - 1);
        out->user_id[MAX_ID_LEN - 1] = '\0';

        if (strcmp(out->user_id, user_id) != 0) continue;

        /* Matching user — parse remaining fields */
        tok = strtok(NULL, "|"); if (!tok) { fclose(fp); return 0; }
        strncpy(out->username,  tok, MAX_STR_LEN - 1);
        out->username[MAX_STR_LEN - 1] = '\0';

        tok = strtok(NULL, "|"); if (!tok) { fclose(fp); return 0; }
        strncpy(out->password,  tok, MAX_STR_LEN - 1); /* skip */
        out->password[MAX_STR_LEN - 1] = '\0';

        tok = strtok(NULL, "|"); if (!tok) { fclose(fp); return 0; }
        strncpy(out->full_name, tok, MAX_STR_LEN - 1);
        out->full_name[MAX_STR_LEN - 1] = '\0';

        tok = strtok(NULL, "|"); if (!tok) { fclose(fp); return 0; }
        strncpy(out->email,     tok, MAX_STR_LEN - 1);
        out->email[MAX_STR_LEN - 1] = '\0';

        tok = strtok(NULL, "|"); if (!tok) { fclose(fp); return 0; }
        strncpy(out->phone,     tok, MAX_STR_LEN - 1);
        out->phone[MAX_STR_LEN - 1] = '\0';

        /* Address is the rest of the line */
        tok = strtok(NULL, "\n");
        if (tok) {
            strncpy(out->address, tok, MAX_ADD_LEN - 1);
            out->address[MAX_ADD_LEN - 1] = '\0';
        } else {
            out->address[0] = '\0';
        }

        fclose(fp);
        return 1;
    }
    fclose(fp);
    return 0;
}


/* ═════════════════════════════════════════════════════════════
   HELPER: find_delivery_boy
   Finds boy_name and boy_phone from delivery_boys.txt by boy_id.
   Format: boy_id|name|phone|vehicle_no|is_active|last_assigned
   ═════════════════════════════════════════════════════════════ */
int find_delivery_boy(const char* boy_id,
                      char* out_name, char* out_phone) {
    /* Safe defaults */
    strncpy(out_name,  "Unknown", MAX_STR_LEN - 1);
    strncpy(out_phone, "N/A",     MAX_STR_LEN - 1);

    FILE* fp = fopen(DELIVERY_FILE, "r");
    if (!fp) return 0;

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        char buf[MAX_LINE_LEN];
        strncpy(buf, line, MAX_LINE_LEN - 1);
        buf[MAX_LINE_LEN - 1] = '\0';

        char* tok = strtok(buf, "|"); if (!tok) continue;
        char  bid[MAX_ID_LEN];
        strncpy(bid, tok, MAX_ID_LEN - 1);
        bid[MAX_ID_LEN - 1] = '\0';

        if (strcmp(bid, boy_id) != 0) continue;

        tok = strtok(NULL, "|"); if (!tok) { fclose(fp); return 0; }
        strncpy(out_name,  tok, MAX_STR_LEN - 1);
        out_name[MAX_STR_LEN - 1] = '\0';

        tok = strtok(NULL, "|"); if (!tok) { fclose(fp); return 0; }
        strncpy(out_phone, tok, MAX_STR_LEN - 1);
        out_phone[MAX_STR_LEN - 1] = '\0';

        fclose(fp);
        return 1;
    }
    fclose(fp);
    return 0;
}


/* ═════════════════════════════════════════════════════════════
   MAIN
   ═════════════════════════════════════════════════════════════ */
int main(int argc, char* argv[]) {

    if (argc < 2) {
        PRINT_ERROR("Usage: ./receipt <order_id>");
        return 1;
    }

    const char* order_id = argv[1];

    /* ── Step 1: Find the order ── */
    Order order;
    memset(&order, 0, sizeof(Order));
    if (!find_order(order_id, &order)) {
        char err[MAX_STR_LEN];
        snprintf(err, sizeof(err), "Order not found: %s", order_id);
        PRINT_ERROR(err);
        return 1;
    }

    /* ── Step 2: Find the user ── */
    User user;
    memset(&user, 0, sizeof(User));
    if (!find_user(order.user_id, &user)) {
        char err[MAX_STR_LEN];
        snprintf(err, sizeof(err), "User not found: %s", order.user_id);
        PRINT_ERROR(err);
        return 1;
    }

    /* ── Step 3: Find the delivery boy ── */
    char boy_name[MAX_STR_LEN]  = "Unknown";
    char boy_phone[MAX_STR_LEN] = "N/A";
    find_delivery_boy(order.delivery_boy_id, boy_name, boy_phone);

    /*
     * ── Step 4: Print unified pipe-delimited output ──
     *
     * FORMAT (13 fields after SUCCESS):
     *   SUCCESS|order_id|user_id|full_name|user_phone|user_email|
     *           address|slot|status|timestamp|boy_name|boy_phone|
     *           total|items_string
     *
     * NOTE: address is the raw comma-separated string from users.txt
     *       e.g. "No 11 - Flat No 11,Elumalai Street,West Tambaram,600045"
     *       Python will split on commas and render 4 separate lines.
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
