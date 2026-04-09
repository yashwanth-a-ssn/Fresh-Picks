/*
 * order.c - Fresh Picks: Shopping Cart, Payment & Order Management (v3)
 * ======================================================================
 * This is the CORE C backend for the entire shopping flow.
 * Called by Flask (app.py) via subprocess.run() like this:
 *   ./order <command> [arguments...]
 *
 * ALL struct definitions and DS implementations have been moved out:
 *   - Structs       → models.h      (shared source of truth)
 *   - Data Structures → ds_utils.c  (DLL, Queue, CLL, Heap)
 *
 * ─────────────────────────────────────────────────────────────────
 * COMMANDS (argv[1]):
 *   list_products                          → Read vegetables.txt, print all
 *   add_to_cart   <uid> <vid> <grams>      → Add/update item in cart DLL
 *   view_cart     <uid>                    → Print cart items + total
 *   remove_item   <uid> <vid>              → Remove one item from cart
 *   checkout      <uid> <slot>             → Full checkout pipeline
 *   get_orders    <uid>                    → All orders for one user
 *   admin_orders                           → All ACTIVE orders, priority sorted
 *   update_order_status <order_id> <status> → Change status in orders.txt
 *
 * ─────────────────────────────────────────────────────────────────
 * CHANGES IN v3:
 *   1. Timestamp: checkout grabs system time → stored in Order.timestamp
 *   2. Price Snapshot: items_string now includes price at order time
 *      e.g. "V1001:500:40.00,VF101:50:0.00"
 *   3. Freebie Multiplier: free qty = (total/500) * item.free_qty_g
 *   4. Priority Filtering: admin_orders only loads "Order Placed" or
 *      "Out for Delivery" status orders into the heap
 *   5. New command: update_order_status <order_id> <new_status>
 *
 * Team: CodeCrafters | Project: Fresh Picks | SDP-1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>     /* NEW v3: for time(), localtime(), strftime() */
#include "models.h"   /* All structs + DS function prototypes */


/* ═════════════════════════════════════════════════════════════
   SECTION 1: HELPER FUNCTIONS
   ═════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: get_slot_priority
 * PURPOSE:  Map a delivery slot name to a numeric priority.
 *           Used by the Min-Heap to sort orders (lower = more urgent).
 *
 *   "Morning"   → 1  (dispatch first)
 *   "Afternoon" → 2
 *   "Evening"   → 3  (dispatch last)
 */
int get_slot_priority(const char* slot) {
    if (strcmp(slot, "Morning")   == 0) return 1;
    if (strcmp(slot, "Afternoon") == 0) return 2;
    return 3;  /* Evening or any unrecognised value */
}

/*
 * FUNCTION: get_current_timestamp
 * PURPOSE:  Fill out_buf with the current date and time as a string.
 *           Format: "YYYY-MM-DD HH:MM:SS"
 *           Example: "2025-04-08 14:30:00"
 *
 * HOW IT WORKS:
 *   time(NULL)   → returns the current Unix time (seconds since 1970)
 *   localtime()  → converts that number into a struct tm
 *                  (struct with year, month, day, hour, min, sec)
 *   strftime()   → formats struct tm into the string we want
 *
 * PARAMS:
 *   out_buf — char array to fill (at least TIMESTAMP_LEN bytes)
 */
void get_current_timestamp(char* out_buf) {
    time_t     now = time(NULL);       /* Get current time as Unix epoch   */
    struct tm* t   = localtime(&now);  /* Convert to local calendar fields */
    strftime(out_buf, TIMESTAMP_LEN, "%Y-%m-%d %H:%M:%S", t);
}

/*
 * FUNCTION: generate_order_id
 * PURPOSE:  Generate a new unique Order ID by finding the current
 *           highest ORD number in orders.txt and adding 1.
 *
 * SCHEME: ORD101, ORD102, ORD103, ...
 * First order will be ORD101 (starts counting from 100).
 */
void generate_order_id(char* out_id) {
    FILE* fp = fopen(ORDERS_FILE, "r");
    int max_num = 100;  /* IDs start at ORD101 */

    if (fp) {
        char line[MAX_LINE_LEN];
        while (fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\n")] = '\0';
            if (strlen(line) == 0) continue;

            /* The order_id is the first field — copy up to the first '|' */
            char oid[MAX_ID_LEN];
            strncpy(oid, line, MAX_ID_LEN - 1);
            oid[MAX_ID_LEN - 1] = '\0';
            char* pipe = strchr(oid, '|');
            if (pipe) *pipe = '\0';  /* Terminate at the pipe delimiter */

            /* Extract the number from "ORD101" → 101 */
            if (strncmp(oid, "ORD", 3) == 0) {
                int num = atoi(oid + 3);  /* atoi("101") = 101 */
                if (num > max_num) max_num = num;
            }
        }
        fclose(fp);
    }
    /* The new ID is one higher than the current maximum */
    snprintf(out_id, MAX_ID_LEN, "ORD%d", max_num + 1);
}

/*
 * FUNCTION: get_cart_filename
 * PURPOSE:  Build the path to a user's personal cart file.
 *           Format: carts/<user_id>_cart.txt
 *           Example: carts/U1001_cart.txt
 */
void get_cart_filename(const char* user_id, char* out_path) {
    snprintf(out_path, MAX_LINE_LEN, "%s%s_cart.txt", CART_DIR, user_id);
}

/*
 * FUNCTION: find_vegetable
 * PURPOSE:  Search vegetables.txt for a row matching the given veg_id.
 *           Fills the Vegetable struct and returns 1 if found, 0 if not.
 */
int find_vegetable(const char* veg_id, Vegetable* out_veg) {
    FILE* fp = fopen(VEGETABLES_FILE, "r");
    if (!fp) return 0;

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        Vegetable v;
        char* tok = strtok(line, "|");
        if (!tok) continue;
        strncpy(v.veg_id,     tok, MAX_ID_LEN  - 1); tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(v.category,   tok, MAX_STR_LEN - 1); tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(v.name,       tok, MAX_STR_LEN - 1); tok = strtok(NULL, "|");
        if (!tok) continue;
        v.stock_g = atoi(tok);                         tok = strtok(NULL, "|");
        if (!tok) continue;
        v.price_per_1000g = atof(tok);                 tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(v.tag,        tok, MAX_STR_LEN - 1); tok = strtok(NULL, "|");
        v.validity_days = tok ? atoi(tok) : 0;

        if (strcmp(v.veg_id, veg_id) == 0) {
            *out_veg = v;
            fclose(fp);
            return 1;  /* Found! */
        }
    }
    fclose(fp);
    return 0;  /* Not found */
}

/*
 * FUNCTION: deduct_vegetable_stock
 * PURPOSE:  Subtract qty_g grams from a vegetable's stock in vegetables.txt.
 *           This is the CRITICAL "commit" step right before payment.
 *           Returns 1 on success, 0 if stock is insufficient.
 *
 * HOW: Read all rows into an array, update the matching one, rewrite the file.
 * WHY rewrite the whole file? Plain .txt files don't support in-place edits.
 */
int deduct_vegetable_stock(const char* veg_id, int qty_g) {
    Vegetable vegs[100];
    int count = 0;

    FILE* fp = fopen(VEGETABLES_FILE, "r");
    if (!fp) return 0;

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp) && count < 100) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        Vegetable v;
        char* tok = strtok(line, "|");
        if (!tok) continue;
        strncpy(v.veg_id,     tok, MAX_ID_LEN  - 1); tok = strtok(NULL, "|");
        strncpy(v.category,   tok, MAX_STR_LEN - 1); tok = strtok(NULL, "|");
        strncpy(v.name,       tok, MAX_STR_LEN - 1); tok = strtok(NULL, "|");
        v.stock_g = atoi(tok);                         tok = strtok(NULL, "|");
        v.price_per_1000g = atof(tok);                 tok = strtok(NULL, "|");
        strncpy(v.tag,        tok, MAX_STR_LEN - 1); tok = strtok(NULL, "|");
        v.validity_days = tok ? atoi(tok) : 0;
        vegs[count++] = v;
    }
    fclose(fp);

    /* Find and deduct stock */
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(vegs[i].veg_id, veg_id) == 0) {
            if (vegs[i].stock_g < qty_g) return 0;  /* Not enough stock! */
            vegs[i].stock_g -= qty_g;
            found = 1;
            break;
        }
    }
    if (!found) return 0;

    /* Rewrite the ENTIRE file with the updated stock */
    fp = fopen(VEGETABLES_FILE, "w");
    if (!fp) return 0;
    for (int i = 0; i < count; i++) {
        fprintf(fp, "%s|%s|%s|%d|%.2f|%s|%d\n",
            vegs[i].veg_id, vegs[i].category, vegs[i].name,
            vegs[i].stock_g, vegs[i].price_per_1000g,
            vegs[i].tag, vegs[i].validity_days
        );
    }
    fclose(fp);
    return 1;
}

/*
 * FUNCTION: load_cart_from_file
 * PURPOSE:  Read the user's cart .txt file and build a DLL from it.
 *
 * Cart file format (one item per line):
 *   veg_id|name|qty_g|price_per_1000g|is_free
 *
 * RETURNS: Head of the DLL, or NULL if cart is empty or doesn't exist yet.
 */
CartNode* load_cart_from_file(const char* user_id) {
    char path[MAX_LINE_LEN];
    get_cart_filename(user_id, path);

    FILE* fp = fopen(path, "r");
    if (!fp) return NULL;  /* No cart file = empty cart */

    CartNode* head = NULL;
    char line[MAX_LINE_LEN];
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

        CartNode* node = dll_create_node(veg_id, name, qty_g, price, is_free);
        dll_append(&head, node);
    }
    fclose(fp);
    return head;
}

/*
 * FUNCTION: save_cart_to_file
 * PURPOSE:  Walk the DLL and write every node back to the user's cart file.
 *           Overwrites the entire file each time (simple and safe).
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
 * FUNCTION: check_and_apply_freebies  (v3 — Freebie Multiplier)
 * PURPOSE:  If cart total >= ₹500, calculate a MULTIPLIER and give
 *           proportionally more free items for larger orders.
 *
 * NEW MULTIPLIER LOGIC (v3):
 *   multiplier        = (int)(cart_total / 500)
 *   final_free_qty_g  = multiplier * item.free_qty_g
 *
 * EXAMPLES:
 *   Cart total ₹500  → multiplier = 1 → 1 × free_qty_g (e.g. 50g)
 *   Cart total ₹1000 → multiplier = 2 → 2 × free_qty_g (e.g. 100g)
 *   Cart total ₹1499 → multiplier = 2 → 2 × free_qty_g (still 100g)
 *   Cart total ₹1500 → multiplier = 3 → 3 × free_qty_g (e.g. 150g)
 *
 * WHY THIS WAY? It rewards customers who spend MORE without complex
 * tiered pricing — simple math that's easy to explain.
 *
 * STILL CHECKS:
 *   1. cart_total >= item.min_trigger_amt (each item has its own threshold)
 *   2. free inventory has enough stock to give (multiplier * free_qty_g)
 */
void check_and_apply_freebies(CartNode** head, float cart_total) {
    if (cart_total < 500.0f) return;  /* Below the base freebie threshold */

    /* ── Calculate how many "₹500 units" are in this order ── */
    int multiplier = (int)(cart_total / 500.0f);
    /* Example: ₹1200 → 1200/500 = 2.4 → (int) = 2 */

    FILE* fp = fopen(FREE_INV_FILE, "r");
    if (!fp) return;

    FreeItem items[20];
    int count = 0;

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp) && count < 20) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

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

    /* For each freebie item, check trigger amount AND sufficient stock */
    for (int i = 0; i < count; i++) {
        /* This item's personal trigger must also be met */
        if (cart_total < items[i].min_trigger_amt) continue;

        /* Calculate how much to give based on the multiplier */
        int qty_to_give = multiplier * items[i].free_qty_g;
        /* Example: multiplier=2, free_qty_g=50 → qty_to_give=100g */

        /* Only give freebies if we have enough stock */
        if (items[i].stock_g < qty_to_give) continue;

        /* Add the free item to the cart DLL (price=0, is_free=1) */
        dll_update_or_append(head,
            items[i].vf_id,
            items[i].name,
            qty_to_give,   /* <- multiplied quantity */
            0.0f,          /* FREE — zero price */
            1              /* is_free = true */
        );

        /* Deduct from free inventory stock */
        items[i].stock_g -= qty_to_give;
    }

    /* Rewrite free_inventory.txt with the updated stock levels */
    fp = fopen(FREE_INV_FILE, "w");
    if (!fp) return;
    for (int i = 0; i < count; i++) {
        fprintf(fp, "%s|%s|%d|%.2f|%d\n",
            items[i].vf_id, items[i].name,
            items[i].stock_g, items[i].min_trigger_amt,
            items[i].free_qty_g
        );
    }
    fclose(fp);
}




/* ═════════════════════════════════════════════════════════════
   SECTION 2: COMMAND HANDLER FUNCTIONS
   Each function handles one command dispatched by main().
   ═════════════════════════════════════════════════════════════ */

/*
 * COMMAND: list_products
 * PURPOSE: Read vegetables.txt and print every row.
 * Flask splits by "\n" to build the products JSON array.
 *
 * OUTPUT FORMAT:
 *   SUCCESS|
 *   veg_id|category|name|stock_g|price_per_1000g|tag|validity_days
 *   veg_id|category|name|stock_g|price_per_1000g|tag|validity_days
 *   ...
 */
void cmd_list_products(void) {
    FILE* fp = fopen(VEGETABLES_FILE, "r");
    if (!fp) { PRINT_ERROR("Could not open vegetables.txt"); return; }

    printf("SUCCESS|");  /* Header line — Flask uses this to know it succeeded */

    char line[MAX_LINE_LEN];
    int  first = 1;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;
        if (!first) printf("\n");
        printf("%s", line);
        first = 0;
    }
    printf("\n");
    fclose(fp);
}

/*
 * COMMAND: add_to_cart
 * PURPOSE: Add or update one vegetable in the user's cart DLL.
 *          argv: add_to_cart <user_id> <veg_id> <qty_grams>
 *
 * STEPS:
 *   1. Validate quantity (> 0, multiple of 50g)
 *   2. Look up vegetable to get current name + price
 *   3. Check requested qty does not exceed current stock
 *   4. Load user's cart DLL from file
 *   5. Update or append the item in the DLL
 *   6. Save the DLL back to file
 */
void cmd_add_to_cart(const char* user_id, const char* veg_id, int qty_g) {
    /* Step 1: Validate */
    if (qty_g <= 0) { PRINT_ERROR("Quantity must be positive"); return; }
    if (qty_g % 50 != 0) { PRINT_ERROR("Quantity must be a multiple of 50g"); return; }

    /* Step 2: Look up the vegetable in vegetables.txt */
    Vegetable v;
    if (!find_vegetable(veg_id, &v)) { PRINT_ERROR("Vegetable not found"); return; }

    /* Step 3: Stock check (at add-to-cart time — will recheck at checkout) */
    if (qty_g > v.stock_g) { PRINT_ERROR("Insufficient stock"); return; }

    /* Step 4: Load the existing cart DLL */
    CartNode* head = load_cart_from_file(user_id);

    /* Step 5: Update if already in cart, else append as new node */
    dll_update_or_append(&head, veg_id, v.name, qty_g, v.price_per_1000g, 0);

    /* Step 6: Write updated DLL back to the cart file */
    save_cart_to_file(user_id, head);

    dll_free_all(head);  /* Always free the DLL after use! */
    PRINT_SUCCESS("Item added to cart");
}

/*
 * COMMAND: view_cart
 * PURPOSE: Load the user's cart DLL and print all items + grand total.
 *          argv: view_cart <user_id>
 *
 * OUTPUT:
 *   SUCCESS|<grand_total>
 *   veg_id|name|qty_g|price_per_1000g|item_total|is_free
 *   ...
 */
void cmd_view_cart(const char* user_id) {
    CartNode* head  = load_cart_from_file(user_id);
    float     total = dll_get_total(head);

    /* First line: status + grand total */
    printf("SUCCESS|%.2f\n", total);

    /* One line per cart item */
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
 * COMMAND: remove_item
 * PURPOSE: Remove one item from the cart DLL by veg_id.
 *          argv: remove_item <user_id> <veg_id>
 */
void cmd_remove_item(const char* user_id, const char* veg_id) {
    CartNode* head = load_cart_from_file(user_id);
    dll_remove(&head, veg_id);   /* DLL re-links neighbors automatically */
    save_cart_to_file(user_id, head);
    dll_free_all(head);
    PRINT_SUCCESS("Item removed from cart");
}

/*
 * COMMAND: checkout  (v3 — with Timestamp + Price Snapshot)
 * PURPOSE: The full payment pipeline.
 *          argv: checkout <user_id> <delivery_slot>
 *
 * FULL PIPELINE (explain each step in the VIVA!):
 *   Step 1:  Load cart DLL from file
 *   Step 2:  Minimum order check (₹100)
 *   Step 3:  STOCK RECHECK — verify current stock for every paid item
 *   Step 4:  Apply freebies (multiplier logic — modifies DLL)
 *   Step 5:  Deduct stock from vegetables.txt
 *   Step 6:  Assign delivery boy (CLL round-robin)
 *   Step 7a: Capture current timestamp
 *   Step 7b: Build items_string WITH PRICE SNAPSHOT
 *            Format: "V1001:500:40.00,VF101:100:0.00"
 *   Step 7c: Build Order struct and enqueue it (FIFO queue)
 *   Step 8:  Append order to orders.txt
 *   Step 9:  Delete cart file
 *   Step 10: Print SUCCESS response for Flask
 */
void cmd_checkout(const char* user_id, const char* slot) {

    /* ── Step 1: Load cart ─────────────────────────────────────── */
    CartNode* head = load_cart_from_file(user_id);
    if (!head) { PRINT_ERROR("Cart is empty"); return; }

    /* ── Step 2: Minimum order check ──────────────────────────── */
    float total = dll_get_total(head);
    if (total < 100.0f) {
        dll_free_all(head);
        PRINT_ERROR("Minimum order is Rs.100");
        return;
    }

    /* ── Step 3: STOCK RECHECK ─────────────────────────────────
       WHY DO WE CHECK AGAIN?
       Between the user adding items to their cart and clicking "Pay",
       another user may have purchased the same item, reducing the stock
       below what we need. We MUST verify stock at payment time.
       We check ALL items BEFORE deducting any — to avoid partial deductions
       that would leave the database in an inconsistent state. */
    CartNode* curr = head;
    while (curr != NULL) {
        if (curr->is_free) { curr = curr->next; continue; } /* Skip freebies */
        Vegetable v;
        if (!find_vegetable(curr->veg_id, &v)) {
            dll_free_all(head);
            PRINT_ERROR("Product no longer available");
            return;
        }
        if (v.stock_g < curr->qty_g) {
            dll_free_all(head);
            char err[MAX_STR_LEN + 50];
            snprintf(err, sizeof(err),
                "Insufficient stock for %s (available: %dg)", v.name, v.stock_g);
            PRINT_ERROR(err);
            return;
        }
        curr = curr->next;
    }

    /* ── Step 4: Apply freebies (Multiplier Logic) ─────────────
       This may ADD new free nodes to the DLL.
       We recalculate total after — though freebies are ₹0. */
    check_and_apply_freebies(&head, total);
    total = dll_get_total(head);  /* Recalculate (freebies don't change total) */

    /* ── Step 5: Deduct stock for all PAID items ───────────────── */
    curr = head;
    while (curr != NULL) {
        if (!curr->is_free) {
            deduct_vegetable_stock(curr->veg_id, curr->qty_g);
        }
        curr = curr->next;
    }

    /* ── Step 6: Delivery Boy Assignment (CLL Round-Robin) ─────── */
    DeliveryNode* cll_head = cll_build();
    DeliveryBoy   assigned_boy;
    char boy_id[MAX_ID_LEN]    = "NONE";
    char boy_name[MAX_STR_LEN] = "Unassigned";
    char boy_phone[MAX_STR_LEN]= "N/A";

    if (cll_assign_delivery(cll_head, &assigned_boy)) {
        strncpy(boy_id,    assigned_boy.boy_id, MAX_ID_LEN  - 1);
        strncpy(boy_name,  assigned_boy.name,   MAX_STR_LEN - 1);
        strncpy(boy_phone, assigned_boy.phone,  MAX_STR_LEN - 1);
    }
    cll_free(cll_head);

    /* ── Step 7a: Capture Timestamp (NEW in v3) ─────────────────── */
    char timestamp[TIMESTAMP_LEN];
    get_current_timestamp(timestamp);
    /* timestamp is now like: "2025-04-08 14:30:00" */

    /* ── Step 7b: Generate Order ID ────────────────────────────── */
    char order_id[MAX_ID_LEN];
    generate_order_id(order_id);

    /* ── Step 7c: Build items_string WITH PRICE SNAPSHOT (NEW v3) ─
       OLD format: "V1001:500,V1003:1000"
       NEW format: "V1001:500:40.00,V1003:1000:60.00,VF101:100:0.00"

       WHY INCLUDE THE PRICE?
       If the admin changes the price of Onion from ₹40 to ₹50 next week,
       an order placed TODAY should still show ₹40 on the invoice.
       Storing the price AT ORDER TIME prevents historical bills from changing.
    */
    char items_string[MAX_LINE_LEN] = "";
    curr = head;
    while (curr != NULL) {
        char part[80];
        /* Format: veg_id:qty_g:price_per_1000g */
        snprintf(part, sizeof(part), "%s:%d:%.2f",
            curr->veg_id,
            curr->qty_g,
            curr->price_per_1000g  /* Price snapshot! */
        );

        /* Append comma separator before each item except the first */
        if (strlen(items_string) > 0) {
            strncat(items_string, ",", MAX_LINE_LEN - strlen(items_string) - 1);
        }
        strncat(items_string, part, MAX_LINE_LEN - strlen(items_string) - 1);
        curr = curr->next;
    }

    /* ── Step 7d: Build the Order struct ───────────────────────── */
    Order o;
    strncpy(o.order_id,        order_id,      MAX_ID_LEN  - 1);
    strncpy(o.user_id,         user_id,       MAX_ID_LEN  - 1);
    o.total_amount = total;
    strncpy(o.delivery_slot,   slot,          MAX_STR_LEN - 1);
    strncpy(o.delivery_boy_id, boy_id,        MAX_ID_LEN  - 1);
    strncpy(o.status,          "Order Placed",MAX_STR_LEN - 1); /* Initial status */
    strncpy(o.timestamp,       timestamp,     TIMESTAMP_LEN - 1);
    strncpy(o.items_string,    items_string,  MAX_LINE_LEN - 1);
    o.slot_priority = get_slot_priority(slot);

    /* ── Step 7e: Enqueue the order in the FIFO queue ───────────
       In a real production system, a background worker would dequeue
       these and trigger warehouse operations. For our demo, we write
       the order to the database immediately after. */
    OrderQueue q;
    queue_init(&q);
    queue_enqueue(&q, o);
    queue_free(&q);

    /* ── Step 8: Append order to orders.txt ─────────────────────
       Schema: order_id|user_id|total|slot|boy_id|status|timestamp|items_string */
    FILE* fp = fopen(ORDERS_FILE, "a");  /* "a" = append, never overwrites */
    if (!fp) {
        dll_free_all(head);
        PRINT_ERROR("Could not save order");
        return;
    }
    fprintf(fp, "%s|%s|%.2f|%s|%s|%s|%s|%s\n",
        o.order_id,
        o.user_id,
        o.total_amount,
        o.delivery_slot,
        o.delivery_boy_id,
        o.status,
        o.timestamp,
        o.items_string
    );
    fclose(fp);

    /* ── Step 9: Delete the cart file (cart is now empty) ─────── */
    char cart_path[MAX_LINE_LEN];
    get_cart_filename(user_id, cart_path);
    remove(cart_path);   /* C standard library: deletes a file */

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
 * COMMAND: get_orders
 * PURPOSE: Return all past orders for a specific user from orders.txt.
 *          argv: get_orders <user_id>
 *
 * OUTPUT:
 *   SUCCESS|
 *   <full order row>
 *   <full order row>
 *   ...
 */
void cmd_get_orders(const char* user_id) {
    FILE* fp = fopen(ORDERS_FILE, "r");
    if (!fp) { PRINT_ERROR("Could not open orders file"); return; }

    printf("SUCCESS|\n");  /* Header line */

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        /* Keep a copy of the original line for printing (strtok modifies) */
        char orig[MAX_LINE_LEN];
        strncpy(orig, line, MAX_LINE_LEN - 1);
        orig[strcspn(orig, "\n")] = '\0';

        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        /* user_id is the SECOND pipe-delimited field */
        char* tok = strtok(line, "|");
        if (!tok) continue;              /* order_id */
        tok = strtok(NULL, "|");         /* user_id  */
        if (!tok) continue;

        if (strcmp(tok, user_id) == 0) {
            printf("%s\n", orig);  /* Print the original, unmodified row */
        }
    }
    fclose(fp);
}

/*
 * COMMAND: admin_orders  (v3 — Status Filtering)
 * PURPOSE: Load orders into a Min-Heap and print them sorted by delivery
 *          slot priority (Morning first). ONLY loads "active" orders.
 *          argv: admin_orders
 *
 * FILTER (NEW in v3):
 *   Only "Order Placed" and "Out for Delivery" orders are relevant
 *   for dispatch. Delivered or Cancelled orders are excluded.
 *   WHY? The admin's dispatch screen should only show ACTIONABLE orders.
 *   Cluttering it with completed orders wastes the admin's time.
 *
 * OUTPUT:
 *   SUCCESS|<count>
 *   order_id|user_id|total|slot|boy_id|status|timestamp|items (sorted)
 *   ...
 */

void cmd_batch_promote_slot(const char* slot_name) {
    if (strcmp(slot_name, "Morning") != 0 && strcmp(slot_name, "Afternoon") != 0 && strcmp(slot_name, "Evening") != 0) {
        PRINT_ERROR("Invalid slot name"); return;
    }
    Order* orders = (Order*)malloc(sizeof(Order) * MAX_ORDERS);
    if (!orders) { PRINT_ERROR("Memory allocation failed"); return; }
    int count = 0; FILE* fp = fopen(ORDERS_FILE, "r");
    if (!fp) { free(orders); PRINT_ERROR("Could not open orders file"); return; }
    char line[MAX_LINE_LEN];

    while (fgets(line, sizeof(line), fp) && count < MAX_ORDERS) {
        line[strcspn(line, "\n")] = '\0'; if (strlen(line) == 0) continue;
        Order o; char* tok = strtok(line, "|");
        if (!tok) continue; strncpy(o.order_id, tok, MAX_ID_LEN - 1); tok = strtok(NULL, "|");
        if (!tok) continue; strncpy(o.user_id, tok, MAX_ID_LEN - 1); tok = strtok(NULL, "|");
        if (!tok) continue; o.total_amount = atof(tok); tok = strtok(NULL, "|");
        if (!tok) continue; strncpy(o.delivery_slot, tok, MAX_STR_LEN - 1); tok = strtok(NULL, "|");
        if (!tok) continue; strncpy(o.delivery_boy_id, tok, MAX_ID_LEN - 1); tok = strtok(NULL, "|");
        if (!tok) continue; strncpy(o.status, tok, MAX_STR_LEN - 1); tok = strtok(NULL, "|");
        if (!tok) continue; strncpy(o.timestamp, tok, TIMESTAMP_LEN - 1); tok = strtok(NULL, "|");
        if (tok) strncpy(o.items_string, tok, MAX_LINE_LEN - 1); else o.items_string[0] = '\0';
        
        o.order_id[MAX_ID_LEN - 1] = '\0'; o.user_id[MAX_ID_LEN - 1] = '\0';
        o.delivery_slot[MAX_STR_LEN - 1] = '\0'; o.delivery_boy_id[MAX_ID_LEN - 1] = '\0';
        o.status[MAX_STR_LEN - 1] = '\0'; o.timestamp[TIMESTAMP_LEN - 1] = '\0'; o.items_string[MAX_LINE_LEN - 1] = '\0';
        o.slot_priority = get_slot_priority(o.delivery_slot);
        orders[count++] = o;
    }
    fclose(fp);

    int promoted = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(orders[i].delivery_slot, slot_name) == 0 && strcmp(orders[i].status, "Order Placed") == 0) {
            strncpy(orders[i].status, "Out for Delivery", MAX_STR_LEN - 1);
            orders[i].status[MAX_STR_LEN - 1] = '\0';
            promoted++;
        }
    }

    fp = fopen(ORDERS_FILE, "w");
    if (!fp) { free(orders); PRINT_ERROR("Could not write"); return; }
    for (int i = 0; i < count; i++) {
        fprintf(fp, "%s|%s|%.2f|%s|%s|%s|%s|%s\n", orders[i].order_id, orders[i].user_id, orders[i].total_amount, 
                orders[i].delivery_slot, orders[i].delivery_boy_id, orders[i].status, orders[i].timestamp, orders[i].items_string);
    }
    fclose(fp); free(orders);
    
    char result[64]; snprintf(result, sizeof(result), "%d", promoted);
    PRINT_SUCCESS(result);
}

 
/*
 * COMMAND: update_order_status  (NEW in v3)
 * PURPOSE: Change the status field of one specific order in orders.txt.
 *          argv: update_order_status <order_id> <new_status>
 *
 * HOW:
 *   1. Read ALL orders from orders.txt into an array.
 *   2. Find the matching order_id.
 *   3. Update its status field in-memory.
 *   4. Rewrite the ENTIRE orders.txt file (plain-text files can't
 *      do in-place edits of an arbitrary-length field).
 *
 * VALID STATUS VALUES (enforce in Flask, document here):
 *   "Order Placed"     → initial state after checkout
 *   "Out for Delivery" → admin dispatched the order
 *   "Delivered"        → delivery boy confirmed delivery
 *   "Cancelled"        → order was cancelled
 *
 * OUTPUT:
 *   SUCCESS|Status updated       (if order_id was found)
 *   ERROR|Order not found        (if order_id doesn't exist)
 */
void cmd_update_order_status(const char* order_id, const char* new_status) {

    /* ── Step 1: Read all orders into a temporary array ─────────── */
    /* We use a dynamically allocated array to avoid stack overflow
       with MAX_ORDERS * large Order struct. */
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

        o.slot_priority = get_slot_priority(o.delivery_slot);
        orders[count++] = o;
    }
    fclose(fp);

    /* ── Step 2: Find the matching order and update its status ─── */
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(orders[i].order_id, order_id) == 0) {
            strncpy(orders[i].status, new_status, MAX_STR_LEN - 1);
            orders[i].status[MAX_STR_LEN - 1] = '\0';
            found = 1;
            break;
        }
    }

    if (!found) {
        free(orders);
        PRINT_ERROR("Order not found");
        return;
    }

    /* ── Step 3: Rewrite orders.txt with all orders (updated) ───── */
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
    free(orders);  /* Free the heap-allocated array */

    PRINT_SUCCESS("Status updated");
}


/* ═════════════════════════════════════════════════════════════
   SECTION 3: MAIN — Command Dispatcher
   ═════════════════════════════════════════════════════════════ */

/*
 * main()
 * PURPOSE: Reads argv[1] (the command name) and calls the matching
 *          handler function. This is the entry point Flask calls via:
 *            subprocess.run(["./order", "checkout", "U1001", "Morning"], ...)
 *
 * argc = total argument count (includes the program name itself)
 * argv[0] = "./order"
 * argv[1] = command name
 * argv[2+] = command-specific arguments
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

    } else if (strcmp(cmd, "admin_orders") == 0) {
        cmd_admin_orders();

    } else if (strcmp(cmd, "update_order_status") == 0) {
        if (argc < 4) { PRINT_ERROR("Usage: update_order_status <order_id> <status>"); return 1; }
        cmd_update_order_status(argv[2], argv[3]);

    } else if (strcmp(cmd, "update_order_status") == 0) {
        if (argc < 4) { PRINT_ERROR("Usage: update_order_status <order_id> <status>"); return 1; }
        cmd_update_order_status(argv[2], argv[3]);

    } else if (strcmp(cmd, "batch_promote_slot") == 0) {
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
