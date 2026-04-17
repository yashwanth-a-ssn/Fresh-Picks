/*
 * order.c - Fresh Picks: Shopping Cart, Payment & Order Management (v4)
 * ======================================================================
 * This is the CORE C backend for the entire shopping flow.
 * Called by Flask (app.py) via subprocess.run() like this:
 *   ./order <command> [arguments...]
 *
 * v4 MIGRATION NOTES:
 *   - All data files are now binary .dat structs accessed via utils.c.
 *   - Direct fopen/fread/fwrite/strtok over .txt files is FORBIDDEN
 *     for all entities except the cart (carts/<uid>_cart.txt).
 *   - Business logic now traverses in-memory SLLs provided by utils.c.
 *   - CLL delivery assignment now uses cll_build_from_sll() +
 *     cll_assign_delivery() from utils.c.
 *   - Order IDs now follow the ORD1001+ scheme (Rule 7).
 *
 * COMMANDS (argv[1]):
 *   list_products                           → Read veg SLL, print all
 *   add_to_cart   <uid> <vid> <grams>       → Add/update item in cart DLL
 *   view_cart     <uid>                     → Print cart items + total
 *   remove_item   <uid> <vid>               → Remove one item from cart
 *   checkout      <uid> <slot>              → Full checkout pipeline
 *   get_orders    <uid>                     → All orders for one user
 *   update_order_status <order_id> <status> → Change status in orders.dat
 *   batch_promote_slot  <slot_name>         → Promote slot orders to OFD
 *   list_all_orders                         → All orders, newest-first
 *
 * CART FILES (only permitted direct I/O — Rule 10):
 *   carts/<user_id>_cart.txt  pipe-delimited, per-session temporary data.
 *
 * Team: CodeCrafters | Project: Fresh Picks | SDP-1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "models.h"   /* All structs, SLL node types, utils.c prototypes */


/* ═════════════════════════════════════════════════════════════
   SECTION 1: HELPER FUNCTIONS
   ═════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: get_slot_priority
 * PURPOSE:  Map a delivery slot name to a numeric priority.
 *           Used by admin_orders (Min-Heap) — lower value = more urgent.
 * PARAMS:   slot — delivery slot string ("Morning", "Afternoon", "Evening")
 * OUTPUT:   (none — returns int)
 * SCHEMA:   (none)
 */
int get_slot_priority(const char* slot) {
    if (strcmp(slot, "Morning")   == 0) return 1;
    if (strcmp(slot, "Afternoon") == 0) return 2;
    return 3;   /* Evening or unrecognised */
}

/*
 * FUNCTION: get_current_timestamp
 * PURPOSE:  Fill out_buf with the current local time as "YYYY-MM-DD HH:MM:SS".
 * PARAMS:   out_buf — caller-provided buffer (at least TIMESTAMP_LEN bytes)
 * OUTPUT:   (none — writes into out_buf)
 * SCHEMA:   (none)
 */
void get_current_timestamp(char* out_buf) {
    time_t     now = time(NULL);
    struct tm* t   = localtime(&now);
    strftime(out_buf, TIMESTAMP_LEN, "%Y-%m-%d %H:%M:%S", t);
}

/*
 * FUNCTION: get_cart_filename
 * PURPOSE:  Build the path to a user's personal cart file.
 *           Format: carts/<user_id>_cart.txt
 * PARAMS:   user_id  — e.g. "U1001"
 *           out_path — buffer to receive the full path
 * OUTPUT:   (none — writes into out_path)
 * SCHEMA:   (none)
 */
void get_cart_filename(const char* user_id, char* out_path) {
    snprintf(out_path, MAX_LINE_LEN, "%s%s_cart.txt", CART_DIR, user_id);
}

/*
 * FUNCTION: find_vegetable_in_sll
 * PURPOSE:  Walk the in-memory VegNode SLL and return a pointer to the
 *           node whose veg_id matches.  Returns NULL if not found.
 * PARAMS:   head   — head of the already-loaded VegNode SLL
 *           veg_id — ID to search for (e.g. "V1001")
 * OUTPUT:   (none — returns VegNode* or NULL)
 * SCHEMA:   (none)
 */
VegNode* find_vegetable_in_sll(VegNode* head, const char* veg_id) {
    VegNode* curr = head;
    while (curr != NULL) {
        if (strcmp(curr->data.veg_id, veg_id) == 0)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/*
 * FUNCTION: load_cart_from_file
 * PURPOSE:  Read the user's cart .txt file (Rule 10 exception) and
 *           build a DLL from it.  Returns NULL if cart is empty/absent.
 *
 * Cart file format (one item per line):
 *   veg_id|name|qty_g|price_per_1000g|is_free
 *
 * PARAMS:   user_id — e.g. "U1001"
 * OUTPUT:   (none — returns CartNode* head or NULL)
 * SCHEMA:   (none)
 */
CartNode* load_cart_from_file(const char* user_id) {
    char path[MAX_LINE_LEN];
    get_cart_filename(user_id, path);

    FILE* fp = fopen(path, "r");
    if (!fp) return NULL;

    CartNode* head = NULL;
    char      line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        char  veg_id[MAX_ID_LEN], name[MAX_STR_LEN];
        int   qty_g, is_free;
        float price;

        char* tok = strtok(line, "|");
        if (!tok) continue;
        strncpy(veg_id, tok, MAX_ID_LEN  - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(name,   tok, MAX_STR_LEN - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        qty_g   = atoi(tok);                      tok = strtok(NULL, "|");
        if (!tok) continue;
        price   = atof(tok);                      tok = strtok(NULL, "|");
        is_free = tok ? atoi(tok) : 0;

        veg_id[MAX_ID_LEN  - 1] = '\0';
        name  [MAX_STR_LEN - 1] = '\0';

        CartNode* node = dll_create_node(veg_id, name, qty_g, price, is_free);
        dll_append(&head, node);
    }
    fclose(fp);
    return head;
}

/*
 * FUNCTION: save_cart_to_file
 * PURPOSE:  Walk the DLL and overwrite the user's cart .txt file.
 *           (Rule 10 exception — cart files remain pipe-delimited text.)
 * PARAMS:   user_id — e.g. "U1001"
 *           head    — head of the in-memory cart DLL
 * OUTPUT:   (none — writes carts/<user_id>_cart.txt)
 * SCHEMA:   (none)
 */
void save_cart_to_file(const char* user_id, CartNode* head) {
    char path[MAX_LINE_LEN];
    get_cart_filename(user_id, path);

    FILE* fp = fopen(path, "w");
    if (!fp) return;

    CartNode* curr = head;
    while (curr != NULL) {
        fprintf(fp, "%s|%s|%d|%.2f|%d\n",
            curr->veg_id,
            curr->name,
            curr->qty_g,
            curr->price_per_1000g,
            curr->is_free
        );
        curr = curr->next;
    }
    fclose(fp);
}

/*
 * FUNCTION: check_and_apply_freebies
 * PURPOSE:  If cart total >= ₹500, apply freebie multiplier logic and
 *           append free items to the cart DLL.  Deducts stock from the
 *           free_items SLL via utils.c.
 *
 * MULTIPLIER LOGIC:
 *   multiplier       = (int)(cart_total / 500)
 *   final_free_qty_g = multiplier * item.free_qty_g
 *
 * PARAMS:   head       — pointer to cart DLL head (may be modified)
 *           cart_total — current paid total (freebies are ₹0)
 * OUTPUT:   (none — modifies SLL in memory and saves via utils.c)
 * SCHEMA:   (none)
 */
void check_and_apply_freebies(CartNode** head, float cart_total) {
    if (cart_total < 500.0f) return;

    int multiplier = (int)(cart_total / 500.0f);

    FreeItemNode* fi_head = load_free_item_sll();
    if (!fi_head) return;

    int modified = 0;
    FreeItemNode* curr = fi_head;
    while (curr != NULL) {
        if (cart_total < curr->data.min_trigger_amt) {
            curr = curr->next;
            continue;
        }

        int qty_to_give = multiplier * curr->data.free_qty_g;
        if (curr->data.stock_g < qty_to_give) {
            curr = curr->next;
            continue;
        }

        dll_update_or_append(head,
            curr->data.vf_id,
            curr->data.name,
            qty_to_give,
            0.0f,
            1
        );

        curr->data.stock_g -= qty_to_give;
        modified = 1;
        curr = curr->next;
    }

    if (modified)
        save_free_item_sll(fi_head);

    free_free_item_sll(fi_head);
}

/*
 * FUNCTION: find_boy_in_sll
 * PURPOSE:  Linear search through a DeliveryBoyNode SLL for a matching
 *           boy_id; fills out_name and out_phone.
 * PARAMS:   sll_head  — head of loaded DeliveryBoyNode SLL
 *           boy_id    — ID to search for (e.g. "D1001")
 *           out_name  — filled with name if found, else "Unknown"
 *           out_phone — filled with phone if found, else "N/A"
 * OUTPUT:   (none — writes into out_name / out_phone)
 * SCHEMA:   (none)
 */
void find_boy_in_sll(DeliveryBoyNode* sll_head, const char* boy_id,
                     char* out_name, char* out_phone) {
    strncpy(out_name,  "Unknown", MAX_STR_LEN - 1);
    strncpy(out_phone, "N/A",     MAX_STR_LEN - 1);
    out_name [MAX_STR_LEN - 1] = '\0';
    out_phone[MAX_STR_LEN - 1] = '\0';

    DeliveryBoyNode* curr = sll_head;
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
   SECTION 2: COMMAND HANDLER FUNCTIONS
   ═════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: cmd_list_products
 * PURPOSE:  Load the vegetable SLL and print every product.
 * PARAMS:   (none)
 * OUTPUT:   SUCCESS|
 *           veg_id|category|name|stock_g|price_per_1000g|tag|validity_days
 *           ...
 * SCHEMA:   veg_id|category|name|stock_g|price_per_1000g|tag|validity_days
 */
void cmd_list_products(void) {
    VegNode* head = load_veg_sll();
    if (!head) {
        printf("SUCCESS|\n");
        return;
    }

    printf("SUCCESS|\n");

    VegNode* curr = head;
    while (curr != NULL) {
        printf("%s|%s|%s|%d|%.2f|%s|%d\n",
            curr->data.veg_id,
            curr->data.category,
            curr->data.name,
            curr->data.stock_g,
            curr->data.price_per_1000g,
            curr->data.tag,
            curr->data.validity_days
        );
        curr = curr->next;
    }

    free_veg_sll(head);
}

/*
 * FUNCTION: cmd_add_to_cart
 * PURPOSE:  Add or update one vegetable in the user's cart DLL.
 *           Validates quantity, checks stock via veg SLL, then writes
 *           the updated DLL back to the cart .txt file.
 * PARAMS:   user_id — e.g. "U1001"
 *           veg_id  — e.g. "V1001"
 *           qty_g   — grams requested (must be > 0 and multiple of 50)
 * OUTPUT:   SUCCESS|Item added to cart
 *           ERROR|reason
 * SCHEMA:   SUCCESS|Item added to cart
 */
void cmd_add_to_cart(const char* user_id, const char* veg_id, int qty_g) {
    if (qty_g <= 0) {
        PRINT_ERROR("Quantity must be positive");
        return;
    }
    if (qty_g % 50 != 0) {
        PRINT_ERROR("Quantity must be a multiple of 50g");
        return;
    }

    VegNode* veg_head = load_veg_sll();
    if (!veg_head) {
        PRINT_ERROR("No products available");
        return;
    }

    VegNode* match = find_vegetable_in_sll(veg_head, veg_id);
    if (!match) {
        free_veg_sll(veg_head);
        PRINT_ERROR("Vegetable not found");
        return;
    }

    if (qty_g > match->data.stock_g) {
        free_veg_sll(veg_head);
        PRINT_ERROR("Insufficient stock");
        return;
    }

    /* Capture the fields we need before freeing */
    char  v_name[MAX_STR_LEN];
    float v_price = match->data.price_per_1000g;
    strncpy(v_name, match->data.name, MAX_STR_LEN - 1);
    v_name[MAX_STR_LEN - 1] = '\0';
    free_veg_sll(veg_head);

    CartNode* head = load_cart_from_file(user_id);
    dll_update_or_append(&head, veg_id, v_name, qty_g, v_price, 0);
    save_cart_to_file(user_id, head);
    dll_free_all(head);

    PRINT_SUCCESS("Item added to cart");
}

/*
 * FUNCTION: cmd_view_cart
 * PURPOSE:  Load the user's cart DLL and print all items plus grand total.
 * PARAMS:   user_id — e.g. "U1001"
 * OUTPUT:   SUCCESS|<grand_total>
 *           veg_id|name|qty_g|price_per_1000g|item_total|is_free
 *           ...
 * SCHEMA:   veg_id|name|qty_g|price_per_1000g|item_total|is_free
 */
void cmd_view_cart(const char* user_id) {
    CartNode* head  = load_cart_from_file(user_id);
    float     total = dll_get_total(head);

    printf("SUCCESS|%.2f\n", total);

    CartNode* curr = head;
    while (curr != NULL) {
        printf("%s|%s|%d|%.2f|%.2f|%d\n",
            curr->veg_id,
            curr->name,
            curr->qty_g,
            curr->price_per_1000g,
            curr->item_total,
            curr->is_free
        );
        curr = curr->next;
    }
    dll_free_all(head);
}

/*
 * FUNCTION: cmd_remove_item
 * PURPOSE:  Remove one item from the cart DLL by veg_id, then persist.
 * PARAMS:   user_id — e.g. "U1001"
 *           veg_id  — e.g. "V1001"
 * OUTPUT:   SUCCESS|Item removed from cart
 * SCHEMA:   SUCCESS|Item removed from cart
 */
void cmd_remove_item(const char* user_id, const char* veg_id) {
    CartNode* head = load_cart_from_file(user_id);
    dll_remove(&head, veg_id);
    save_cart_to_file(user_id, head);
    dll_free_all(head);
    PRINT_SUCCESS("Item removed from cart");
}

/*
 * FUNCTION: cmd_checkout
 * PURPOSE:  Full payment pipeline — validates cart, applies freebies,
 *           deducts stock, assigns a delivery boy, records the order.
 * PARAMS:   user_id — e.g. "U1001"
 *           slot    — "Morning" | "Afternoon" | "Evening"
 * OUTPUT:   SUCCESS|order_id|total|slot|boy_name|boy_phone|items_string
 *           ERROR|reason
 * SCHEMA:   order_id|total|slot|boy_name|boy_phone|items_string
 */
void cmd_checkout(const char* user_id, const char* slot) {

    /* ── Step 1: Load cart ─────────────────────────────────────── */
    CartNode* head = load_cart_from_file(user_id);
    if (!head) {
        PRINT_ERROR("Cart is empty");
        return;
    }

    /* ── Step 2: Minimum order check ──────────────────────────── */
    float total = dll_get_total(head);
    if (total < 100.0f) {
        dll_free_all(head);
        PRINT_ERROR("Minimum order is Rs.100");
        return;
    }

    /* ── Step 3: Stock recheck via veg SLL ─────────────────────── */
    VegNode* veg_head = load_veg_sll();
    if (!veg_head) {
        dll_free_all(head);
        PRINT_ERROR("Product data unavailable");
        return;
    }

    CartNode* curr = head;
    while (curr != NULL) {
        if (curr->is_free) { curr = curr->next; continue; }

        VegNode* vmatch = find_vegetable_in_sll(veg_head, curr->veg_id);
        if (!vmatch) {
            free_veg_sll(veg_head);
            dll_free_all(head);
            PRINT_ERROR("Product no longer available");
            return;
        }
        if (vmatch->data.stock_g < curr->qty_g) {
            char err[MAX_STR_LEN + 50];
            snprintf(err, sizeof(err),
                "Insufficient stock for %s (available: %dg)",
                vmatch->data.name, vmatch->data.stock_g);
            free_veg_sll(veg_head);
            dll_free_all(head);
            PRINT_ERROR(err);
            return;
        }
        curr = curr->next;
    }

    /* ── Step 4: Apply freebies (multiplier logic) ─────────────── */
    check_and_apply_freebies(&head, total);
    total = dll_get_total(head);

    /* ── Step 5: Deduct stock for all paid items ───────────────── */
    curr = head;
    while (curr != NULL) {
        if (!curr->is_free) {
            VegNode* vmatch = find_vegetable_in_sll(veg_head, curr->veg_id);
            if (vmatch)
                vmatch->data.stock_g -= curr->qty_g;
        }
        curr = curr->next;
    }
    save_veg_sll(veg_head);
    free_veg_sll(veg_head);

    /* ── Step 6: Delivery boy assignment (CLL round-robin) ─────── */
    DeliveryBoyNode* boy_sll = load_delivery_boy_sll();
    DeliveryNode*    cll     = cll_build_from_sll(boy_sll);

    DeliveryBoy assigned_boy;
    char boy_id   [MAX_ID_LEN]  = "NONE";
    char boy_name [MAX_STR_LEN] = "Unassigned";
    char boy_phone[MAX_STR_LEN] = "N/A";

    if (cll && cll_assign_delivery(cll, &assigned_boy, boy_sll)) {
        strncpy(boy_id,    assigned_boy.boy_id, MAX_ID_LEN  - 1);
        strncpy(boy_name,  assigned_boy.name,   MAX_STR_LEN - 1);
        strncpy(boy_phone, assigned_boy.phone,  MAX_STR_LEN - 1);
        boy_id   [MAX_ID_LEN  - 1] = '\0';
        boy_name [MAX_STR_LEN - 1] = '\0';
        boy_phone[MAX_STR_LEN - 1] = '\0';
        save_delivery_boy_sll(boy_sll);
    }
    cll_free(cll);
    free_delivery_boy_sll(boy_sll);

    /* ── Step 7a: Capture timestamp ────────────────────────────── */
    char timestamp[TIMESTAMP_LEN];
    get_current_timestamp(timestamp);

    /* ── Step 7b: Generate order ID ────────────────────────────── */
    OrderNode* ord_head = load_order_sll();
    int        ord_count = sll_count_orders(ord_head);
    char       order_id[MAX_ID_LEN];
    snprintf(order_id, MAX_ID_LEN, "ORD%d", 1001 + ord_count);

    /* ── Step 7c: Build items_string with price snapshot ────────── */
    char items_string[MAX_LINE_LEN] = "";
    curr = head;
    while (curr != NULL) {
        char safe_name[MAX_STR_LEN];
        strncpy(safe_name, curr->name, MAX_STR_LEN - 1);
        safe_name[MAX_STR_LEN - 1] = '\0';
        for (char* p = safe_name; *p; p++) {
            if (*p == ':' || *p == ',') *p = ' ';
        }

        char part[128];
        snprintf(part, sizeof(part), "%s:%s:%d:%.2f",
            curr->veg_id,
            safe_name,
            curr->qty_g,
            curr->price_per_1000g
        );
        if (strlen(items_string) > 0)
            strncat(items_string, ",", MAX_LINE_LEN - strlen(items_string) - 1);
        strncat(items_string, part, MAX_LINE_LEN - strlen(items_string) - 1);
        curr = curr->next;
    }

    /* ── Step 7d: Build Order struct ───────────────────────────── */
    Order o;
    memset(&o, 0, sizeof(Order));
    strncpy(o.order_id,        order_id,       MAX_ID_LEN   - 1);
    strncpy(o.user_id,         user_id,        MAX_ID_LEN   - 1);
    o.total_amount = total;
    strncpy(o.delivery_slot,   slot,           MAX_STR_LEN  - 1);
    strncpy(o.delivery_boy_id, boy_id,         MAX_ID_LEN   - 1);
    strncpy(o.status,          "Order Placed", MAX_STR_LEN  - 1);
    strncpy(o.timestamp,       timestamp,      TIMESTAMP_LEN - 1);
    strncpy(o.items_string,    items_string,   MAX_LINE_LEN  - 1);
    o.slot_priority = get_slot_priority(slot);

    /* ── Step 8: Append order to order SLL and save ─────────────── */
    OrderNode* new_node = (OrderNode*)malloc(sizeof(OrderNode));
    if (!new_node) {
        free_order_sll(ord_head);
        dll_free_all(head);
        PRINT_ERROR("Memory allocation failed");
        return;
    }
    new_node->data = o;
    new_node->next = NULL;

    if (!ord_head) {
        ord_head = new_node;
    } else {
        OrderNode* tail = ord_head;
        while (tail->next) tail = tail->next;
        tail->next = new_node;
    }
    save_order_sll(ord_head);
    free_order_sll(ord_head);

    /* ── Step 9: Delete the cart file ──────────────────────────── */
    char cart_path[MAX_LINE_LEN];
    get_cart_filename(user_id, cart_path);
    remove(cart_path);

    /* ── Step 10: Print confirmation for Flask ─────────────────── */
    printf("SUCCESS|%s|%.2f|%s|%s|%s|%s\n",
        order_id,
        total,
        slot,
        boy_name,
        boy_phone,
        items_string
    );

    dll_free_all(head);
}

/*
 * FUNCTION: cmd_get_orders
 * PURPOSE:  Print all orders for a given user, enriched with the
 *           assigned delivery boy's name and phone.
 * PARAMS:   user_id — e.g. "U1001"
 * OUTPUT:   SUCCESS|
 *           order_id|user_id|total|slot|boy_id|status|timestamp|items_string|boy_name|boy_phone
 *           ...
 * SCHEMA:   order_id|user_id|total|slot|boy_id|status|timestamp|items_string|boy_name|boy_phone
 */
void cmd_get_orders(const char* user_id) {
    DeliveryBoyNode* boy_head = load_delivery_boy_sll();
    OrderNode*       ord_head = load_order_sll();

    printf("SUCCESS|\n");

    OrderNode* curr = ord_head;
    while (curr != NULL) {
        if (strcmp(curr->data.user_id, user_id) == 0) {
            char boy_name [MAX_STR_LEN] = "Unknown";
            char boy_phone[MAX_STR_LEN] = "N/A";
            find_boy_in_sll(boy_head, curr->data.delivery_boy_id,
                            boy_name, boy_phone);

            printf("%s|%s|%.2f|%s|%s|%s|%s|%s|%s|%s\n",
                curr->data.order_id,
                curr->data.user_id,
                curr->data.total_amount,
                curr->data.delivery_slot,
                curr->data.delivery_boy_id,
                curr->data.status,
                curr->data.timestamp,
                curr->data.items_string,
                boy_name,
                boy_phone
            );
        }
        curr = curr->next;
    }

    free_order_sll(ord_head);
    free_delivery_boy_sll(boy_head);
}

/*
 * FUNCTION: cmd_update_order_status
 * PURPOSE:  Change the status field of one specific order in the SLL
 *           and persist via utils.c.
 * PARAMS:   order_id   — e.g. "ORD1001"
 *           new_status — e.g. "Out for Delivery"
 * OUTPUT:   SUCCESS|Status updated
 *           ERROR|Order not found
 * SCHEMA:   SUCCESS|Status updated
 */
void cmd_update_order_status(const char* order_id, const char* new_status) {
    OrderNode* head = load_order_sll();
    if (!head) {
        PRINT_ERROR("No orders found");
        return;
    }

    OrderNode* curr  = head;
    int        found = 0;
    while (curr != NULL) {
        if (strcmp(curr->data.order_id, order_id) == 0) {
            strncpy(curr->data.status, new_status, MAX_STR_LEN - 1);
            curr->data.status[MAX_STR_LEN - 1] = '\0';
            found = 1;
            break;
        }
        curr = curr->next;
    }

    if (!found) {
        free_order_sll(head);
        PRINT_ERROR("Order not found");
        return;
    }

    save_order_sll(head);
    free_order_sll(head);
    PRINT_SUCCESS("Status updated");
}

/*
 * FUNCTION: cmd_batch_promote_slot
 * PURPOSE:  Promote all "Order Placed" orders in a given slot to
 *           "Out for Delivery" and report the count changed.
 * PARAMS:   slot_name — "Morning" | "Afternoon" | "Evening"
 * OUTPUT:   SUCCESS|<promoted_count>
 *           ERROR|Invalid slot name
 * SCHEMA:   SUCCESS|<count>
 */
void cmd_batch_promote_slot(const char* slot_name) {
    if (strcmp(slot_name, "Morning")   != 0 &&
        strcmp(slot_name, "Afternoon") != 0 &&
        strcmp(slot_name, "Evening")   != 0) {
        PRINT_ERROR("Invalid slot name");
        return;
    }

    OrderNode* head = load_order_sll();
    if (!head) {
        PRINT_SUCCESS("0");
        return;
    }

    int        promoted = 0;
    OrderNode* curr     = head;
    while (curr != NULL) {
        if (strcmp(curr->data.delivery_slot, slot_name) == 0 &&
            strcmp(curr->data.status, "Order Placed")   == 0) {
            strncpy(curr->data.status, "Out for Delivery", MAX_STR_LEN - 1);
            curr->data.status[MAX_STR_LEN - 1] = '\0';
            promoted++;
        }
        curr = curr->next;
    }

    save_order_sll(head);
    free_order_sll(head);

    char result[32];
    snprintf(result, sizeof(result), "%d", promoted);
    PRINT_SUCCESS(result);
}

/*
 * FUNCTION: cmd_list_all_orders
 * PURPOSE:  Dump every order from the SLL, newest-first, enriched with
 *           the delivery boy name and phone.  Used by the admin full-view.
 * PARAMS:   (none)
 * OUTPUT:   SUCCESS|<total_count>
 *           order_id|user_id|total|slot|boy_id|status|timestamp|items_string|boy_name|boy_phone
 *           ...
 * SCHEMA:   order_id|user_id|total|slot|boy_id|status|timestamp|items_string|boy_name|boy_phone
 */
void cmd_list_all_orders(void) {
    DeliveryBoyNode* boy_head = load_delivery_boy_sll();
    OrderNode*       ord_head = load_order_sll();

    int total_count = sll_count_orders(ord_head);
    printf("SUCCESS|%d\n", total_count);

    if (total_count == 0) {
        free_order_sll(ord_head);
        free_delivery_boy_sll(boy_head);
        return;
    }

    /* Collect pointers in an array so we can iterate in reverse */
    OrderNode** ptrs = (OrderNode**)malloc(sizeof(OrderNode*) * total_count);
    if (!ptrs) {
        free_order_sll(ord_head);
        free_delivery_boy_sll(boy_head);
        return;
    }

    int        idx  = 0;
    OrderNode* curr = ord_head;
    while (curr != NULL) {
        ptrs[idx++] = curr;
        curr = curr->next;
    }

    for (int i = idx - 1; i >= 0; i--) {
        char boy_name [MAX_STR_LEN] = "Unknown";
        char boy_phone[MAX_STR_LEN] = "N/A";
        find_boy_in_sll(boy_head, ptrs[i]->data.delivery_boy_id,
                        boy_name, boy_phone);

        printf("%s|%s|%.2f|%s|%s|%s|%s|%s|%s|%s\n",
            ptrs[i]->data.order_id,
            ptrs[i]->data.user_id,
            ptrs[i]->data.total_amount,
            ptrs[i]->data.delivery_slot,
            ptrs[i]->data.delivery_boy_id,
            ptrs[i]->data.status,
            ptrs[i]->data.timestamp,
            ptrs[i]->data.items_string,
            boy_name,
            boy_phone
        );
    }

    free(ptrs);
    free_order_sll(ord_head);
    free_delivery_boy_sll(boy_head);
}


/* ═════════════════════════════════════════════════════════════
   SECTION 3: MAIN — Command Dispatcher
   ═════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: main
 * PURPOSE:  Entry point.  Reads argv[1] (the command name) and dispatches
 *           to the matching handler.  Flask calls this binary via:
 *             subprocess.run(["./order", "checkout", "U1001", "Morning"], ...)
 * PARAMS:   argc — argument count, argv — argument vector
 * OUTPUT:   Delegates entirely to handler functions.
 * SCHEMA:   (none — dispatcher only)
 */
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

    } else if (strcmp(cmd, "update_order_status") == 0) {
        if (argc < 4) { PRINT_ERROR("Usage: update_order_status <order_id> <status>"); return 1; }
        cmd_update_order_status(argv[2], argv[3]);

    } else if (strcmp(cmd, "batch_promote_slot") == 0) {
        if (argc < 3) { PRINT_ERROR("Usage: batch_promote_slot <slot_name>"); return 1; }
        cmd_batch_promote_slot(argv[2]);

    } else if (strcmp(cmd, "list_all_orders") == 0) {
        cmd_list_all_orders();

    } else {
        char err[MAX_STR_LEN];
        snprintf(err, sizeof(err), "Unknown command: %s", cmd);
        PRINT_ERROR(err);
        return 1;
    }
    return 0;
}
