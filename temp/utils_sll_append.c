/*
 * ============================================================
 * APPEND THIS BLOCK TO THE BOTTOM OF utils.c
 * (utils.c is the renamed ds_utils.c — keep all existing code above)
 * ============================================================
 *
 * utils.c - Fresh Picks: Data Structure & Binary I/O Utility Library
 * ===================================================================
 * SECTION ADDED IN v4:
 *   DATA STRUCTURE 5: SINGLY LINKED LISTS (Binary .dat Persistence Layer)
 *
 * WHY A SEPARATE SECTION?
 *   The DLL, Queue, CLL, and Min-Heap above operate on in-memory runtime
 *   data (e.g. the cart during a single request). This new section handles
 *   the PERSISTENT layer: loading from and saving to binary .dat files.
 *
 *   Each entity (User, Vegetable, Order, FreeItem, DeliveryBoy, AdminCreds)
 *   gets three functions:
 *     load_<entity>_sll()  — fread structs from .dat into an in-memory SLL
 *     save_<entity>_sll()  — fwrite the entire SLL back to the .dat file
 *     free_<entity>_sll()  — walk and free every heap-allocated node
 *
 * FILE LOCKING STRATEGY:
 *   load uses LOCK_SH (shared read lock)  — multiple readers allowed.
 *   save uses LOCK_EX (exclusive write lock) — one writer at a time.
 *   This prevents data corruption when multiple users place orders
 *   or the admin updates stock simultaneously via Flask's multi-thread mode.
 *
 * HOW TO USE (in any business-logic .c file):
 *   1. Load:   VegNode* vegs = load_veg_sll();
 *   2. Modify: traverse the SLL and update fields in-memory.
 *   3. Save:   save_veg_sll(vegs);
 *   4. Free:   free_veg_sll(vegs);
 *
 * COMPILE:
 *   Always link utils.c alongside the business-logic file:
 *     gcc -Wall -Wextra -o order order.c utils.c -lm
 *
 * Team: CodeCrafters | Project: Fresh Picks | SDP-1
 */

#include <sys/file.h>   /* flock() — file locking for concurrent access safety */


/* ══════════════════════════════════════════════════════════════
   INTERNAL HELPER MACROS (used only inside this file)

   SLL_LOAD_OPEN(filepath, fp)
     Opens the .dat file for reading. Returns NULL on failure.

   SLL_SAVE_OPEN(filepath, fp)
     Opens the .dat file for writing (truncates). Returns on failure.
   ══════════════════════════════════════════════════════════════ */

#define SLL_LOAD_OPEN(filepath, fp)                     \
    do {                                                \
        (fp) = fopen((filepath), "rb");                 \
        if (!(fp)) return NULL;                         \
        flock(fileno(fp), LOCK_SH);                     \
    } while (0)

#define SLL_SAVE_OPEN(filepath, fp)                     \
    do {                                                \
        (fp) = fopen((filepath), "wb");                 \
        if (!(fp)) return;                              \
        flock(fileno(fp), LOCK_EX);                     \
    } while (0)

#define SLL_CLOSE(fp)                                   \
    do {                                                \
        flock(fileno(fp), LOCK_UN);                     \
        fclose(fp);                                     \
    } while (0)


/* ══════════════════════════════════════════════════════════════
   DATA STRUCTURE 5A: USER SLL
   Persistent store: users.dat
   ══════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: load_user_sll
 * PURPOSE:  Read all User structs from users.dat into a Singly Linked List.
 *           Caller must call free_user_sll() when done to prevent leaks.
 * PARAMS:   (none)
 * OUTPUT:   Pointer to the head of the SLL, or NULL if file missing/empty.
 * SCHEMA:   users.dat → flat sequence of User structs (binary, no delimiter)
 */
UserNode* load_user_sll(void) {
    FILE* fp;
    SLL_LOAD_OPEN(USERS_FILE, fp);

    UserNode* head = NULL;
    UserNode* tail = NULL;
    User      buf;

    while (fread(&buf, sizeof(User), 1, fp) == 1) {
        UserNode* node = (UserNode*)malloc(sizeof(UserNode));
        if (!node) break;   /* Guard: malloc failure — stop loading */

        node->data = buf;
        node->next = NULL;

        if (!head) {
            head = node;
            tail = node;
        } else {
            tail->next = node;
            tail       = node;
        }
    }

    SLL_CLOSE(fp);
    return head;
}

/*
 * FUNCTION: save_user_sll
 * PURPOSE:  Write every User in the SLL back to users.dat as raw binary structs.
 *           This OVERWRITES the file — the SLL is the authoritative in-memory state.
 * PARAMS:   head — pointer to the first UserNode in the SLL
 * OUTPUT:   (none) — writes to users.dat
 * SCHEMA:   users.dat ← flat sequence of User structs (binary, no delimiter)
 */
void save_user_sll(UserNode* head) {
    FILE* fp;
    SLL_SAVE_OPEN(USERS_FILE, fp);

    UserNode* curr = head;
    while (curr != NULL) {
        fwrite(&curr->data, sizeof(User), 1, fp);
        curr = curr->next;
    }

    SLL_CLOSE(fp);
}

/*
 * FUNCTION: free_user_sll
 * PURPOSE:  Walk the SLL and free every heap-allocated node.
 *           MUST be called after load_user_sll() to prevent memory leaks.
 * PARAMS:   head — pointer to the first UserNode
 * OUTPUT:   (none)
 */
void free_user_sll(UserNode* head) {
    UserNode* curr = head;
    while (curr != NULL) {
        UserNode* next = curr->next;
        free(curr);
        curr = next;
    }
}


/* ══════════════════════════════════════════════════════════════
   DATA STRUCTURE 5B: VEGETABLE SLL
   Persistent store: products.dat
   ══════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: load_veg_sll
 * PURPOSE:  Read all Vegetable structs from products.dat into a SLL.
 * PARAMS:   (none)
 * OUTPUT:   Pointer to head of SLL, or NULL if file missing/empty.
 * SCHEMA:   products.dat → flat sequence of Vegetable structs (binary)
 */
VegNode* load_veg_sll(void) {
    FILE* fp;
    SLL_LOAD_OPEN(PRODUCTS_FILE, fp);

    VegNode*  head = NULL;
    VegNode*  tail = NULL;
    Vegetable buf;

    while (fread(&buf, sizeof(Vegetable), 1, fp) == 1) {
        VegNode* node = (VegNode*)malloc(sizeof(VegNode));
        if (!node) break;

        node->data = buf;
        node->next = NULL;

        if (!head) {
            head = node;
            tail = node;
        } else {
            tail->next = node;
            tail       = node;
        }
    }

    SLL_CLOSE(fp);
    return head;
}

/*
 * FUNCTION: save_veg_sll
 * PURPOSE:  Write every Vegetable in the SLL back to products.dat.
 * PARAMS:   head — pointer to the first VegNode
 * OUTPUT:   (none) — writes to products.dat
 * SCHEMA:   products.dat ← flat sequence of Vegetable structs (binary)
 */
void save_veg_sll(VegNode* head) {
    FILE* fp;
    SLL_SAVE_OPEN(PRODUCTS_FILE, fp);

    VegNode* curr = head;
    while (curr != NULL) {
        fwrite(&curr->data, sizeof(Vegetable), 1, fp);
        curr = curr->next;
    }

    SLL_CLOSE(fp);
}

/*
 * FUNCTION: free_veg_sll
 * PURPOSE:  Free every heap-allocated VegNode in the SLL.
 * PARAMS:   head — pointer to the first VegNode
 * OUTPUT:   (none)
 */
void free_veg_sll(VegNode* head) {
    VegNode* curr = head;
    while (curr != NULL) {
        VegNode* next = curr->next;
        free(curr);
        curr = next;
    }
}


/* ══════════════════════════════════════════════════════════════
   DATA STRUCTURE 5C: ORDER SLL
   Persistent store: orders.dat
   ══════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: load_order_sll
 * PURPOSE:  Read all Order structs from orders.dat into a SLL.
 * PARAMS:   (none)
 * OUTPUT:   Pointer to head of SLL, or NULL if file missing/empty.
 * SCHEMA:   orders.dat → flat sequence of Order structs (binary)
 */
OrderNode* load_order_sll(void) {
    FILE* fp;
    SLL_LOAD_OPEN(ORDERS_FILE, fp);

    OrderNode* head = NULL;
    OrderNode* tail = NULL;
    Order      buf;

    while (fread(&buf, sizeof(Order), 1, fp) == 1) {
        OrderNode* node = (OrderNode*)malloc(sizeof(OrderNode));
        if (!node) break;

        node->data = buf;
        node->next = NULL;

        if (!head) {
            head = node;
            tail = node;
        } else {
            tail->next = node;
            tail       = node;
        }
    }

    SLL_CLOSE(fp);
    return head;
}

/*
 * FUNCTION: save_order_sll
 * PURPOSE:  Write every Order in the SLL back to orders.dat.
 * PARAMS:   head — pointer to the first OrderNode
 * OUTPUT:   (none) — writes to orders.dat
 * SCHEMA:   orders.dat ← flat sequence of Order structs (binary)
 */
void save_order_sll(OrderNode* head) {
    FILE* fp;
    SLL_SAVE_OPEN(ORDERS_FILE, fp);

    OrderNode* curr = head;
    while (curr != NULL) {
        fwrite(&curr->data, sizeof(Order), 1, fp);
        curr = curr->next;
    }

    SLL_CLOSE(fp);
}

/*
 * FUNCTION: free_order_sll
 * PURPOSE:  Free every heap-allocated OrderNode in the SLL.
 * PARAMS:   head — pointer to the first OrderNode
 * OUTPUT:   (none)
 */
void free_order_sll(OrderNode* head) {
    OrderNode* curr = head;
    while (curr != NULL) {
        OrderNode* next = curr->next;
        free(curr);
        curr = next;
    }
}


/* ══════════════════════════════════════════════════════════════
   DATA STRUCTURE 5D: FREE ITEM SLL
   Persistent store: free_inventory.dat
   ══════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: load_free_item_sll
 * PURPOSE:  Read all FreeItem structs from free_inventory.dat into a SLL.
 * PARAMS:   (none)
 * OUTPUT:   Pointer to head of SLL, or NULL if file missing/empty.
 * SCHEMA:   free_inventory.dat → flat sequence of FreeItem structs (binary)
 */
FreeItemNode* load_free_item_sll(void) {
    FILE* fp;
    SLL_LOAD_OPEN(FREE_INV_FILE, fp);

    FreeItemNode* head = NULL;
    FreeItemNode* tail = NULL;
    FreeItem      buf;

    while (fread(&buf, sizeof(FreeItem), 1, fp) == 1) {
        FreeItemNode* node = (FreeItemNode*)malloc(sizeof(FreeItemNode));
        if (!node) break;

        node->data = buf;
        node->next = NULL;

        if (!head) {
            head = node;
            tail = node;
        } else {
            tail->next = node;
            tail       = node;
        }
    }

    SLL_CLOSE(fp);
    return head;
}

/*
 * FUNCTION: save_free_item_sll
 * PURPOSE:  Write every FreeItem in the SLL back to free_inventory.dat.
 * PARAMS:   head — pointer to the first FreeItemNode
 * OUTPUT:   (none) — writes to free_inventory.dat
 * SCHEMA:   free_inventory.dat ← flat sequence of FreeItem structs (binary)
 */
void save_free_item_sll(FreeItemNode* head) {
    FILE* fp;
    SLL_SAVE_OPEN(FREE_INV_FILE, fp);

    FreeItemNode* curr = head;
    while (curr != NULL) {
        fwrite(&curr->data, sizeof(FreeItem), 1, fp);
        curr = curr->next;
    }

    SLL_CLOSE(fp);
}

/*
 * FUNCTION: free_free_item_sll
 * PURPOSE:  Free every heap-allocated FreeItemNode in the SLL.
 * PARAMS:   head — pointer to the first FreeItemNode
 * OUTPUT:   (none)
 */
void free_free_item_sll(FreeItemNode* head) {
    FreeItemNode* curr = head;
    while (curr != NULL) {
        FreeItemNode* next = curr->next;
        free(curr);
        curr = next;
    }
}


/* ══════════════════════════════════════════════════════════════
   DATA STRUCTURE 5E: DELIVERY BOY SLL
   Persistent store: delivery_boys.dat
   ══════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: load_delivery_boy_sll
 * PURPOSE:  Read all DeliveryBoy structs from delivery_boys.dat into a SLL.
 * PARAMS:   (none)
 * OUTPUT:   Pointer to head of SLL, or NULL if file missing/empty.
 * SCHEMA:   delivery_boys.dat → flat sequence of DeliveryBoy structs (binary)
 */
DeliveryBoyNode* load_delivery_boy_sll(void) {
    FILE* fp;
    SLL_LOAD_OPEN(DELIVERY_FILE, fp);

    DeliveryBoyNode* head = NULL;
    DeliveryBoyNode* tail = NULL;
    DeliveryBoy      buf;

    while (fread(&buf, sizeof(DeliveryBoy), 1, fp) == 1) {
        DeliveryBoyNode* node = (DeliveryBoyNode*)malloc(sizeof(DeliveryBoyNode));
        if (!node) break;

        node->data = buf;
        node->next = NULL;

        if (!head) {
            head = node;
            tail = node;
        } else {
            tail->next = node;
            tail       = node;
        }
    }

    SLL_CLOSE(fp);
    return head;
}

/*
 * FUNCTION: save_delivery_boy_sll
 * PURPOSE:  Write every DeliveryBoy in the SLL back to delivery_boys.dat.
 * PARAMS:   head — pointer to the first DeliveryBoyNode
 * OUTPUT:   (none) — writes to delivery_boys.dat
 * SCHEMA:   delivery_boys.dat ← flat sequence of DeliveryBoy structs (binary)
 */
void save_delivery_boy_sll(DeliveryBoyNode* head) {
    FILE* fp;
    SLL_SAVE_OPEN(DELIVERY_FILE, fp);

    DeliveryBoyNode* curr = head;
    while (curr != NULL) {
        fwrite(&curr->data, sizeof(DeliveryBoy), 1, fp);
        curr = curr->next;
    }

    SLL_CLOSE(fp);
}

/*
 * FUNCTION: free_delivery_boy_sll
 * PURPOSE:  Free every heap-allocated DeliveryBoyNode in the SLL.
 * PARAMS:   head — pointer to the first DeliveryBoyNode
 * OUTPUT:   (none)
 */
void free_delivery_boy_sll(DeliveryBoyNode* head) {
    DeliveryBoyNode* curr = head;
    while (curr != NULL) {
        DeliveryBoyNode* next = curr->next;
        free(curr);
        curr = next;
    }
}


/* ══════════════════════════════════════════════════════════════
   DATA STRUCTURE 5F: ADMIN CREDENTIALS SLL
   Persistent store: admin_creds.dat
   ══════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: load_admin_sll
 * PURPOSE:  Read all AdminCreds structs from admin_creds.dat into a SLL.
 * PARAMS:   (none)
 * OUTPUT:   Pointer to head of SLL, or NULL if file missing/empty.
 * SCHEMA:   admin_creds.dat → flat sequence of AdminCreds structs (binary)
 */
AdminNode* load_admin_sll(void) {
    FILE* fp;
    SLL_LOAD_OPEN(ADMIN_FILE, fp);

    AdminNode*  head = NULL;
    AdminNode*  tail = NULL;
    AdminCreds  buf;

    while (fread(&buf, sizeof(AdminCreds), 1, fp) == 1) {
        AdminNode* node = (AdminNode*)malloc(sizeof(AdminNode));
        if (!node) break;

        node->data = buf;
        node->next = NULL;

        if (!head) {
            head = node;
            tail = node;
        } else {
            tail->next = node;
            tail       = node;
        }
    }

    SLL_CLOSE(fp);
    return head;
}

/*
 * FUNCTION: save_admin_sll
 * PURPOSE:  Write every AdminCreds in the SLL back to admin_creds.dat.
 * PARAMS:   head — pointer to the first AdminNode
 * OUTPUT:   (none) — writes to admin_creds.dat
 * SCHEMA:   admin_creds.dat ← flat sequence of AdminCreds structs (binary)
 */
void save_admin_sll(AdminNode* head) {
    FILE* fp;
    SLL_SAVE_OPEN(ADMIN_FILE, fp);

    AdminNode* curr = head;
    while (curr != NULL) {
        fwrite(&curr->data, sizeof(AdminCreds), 1, fp);
        curr = curr->next;
    }

    SLL_CLOSE(fp);
}

/*
 * FUNCTION: free_admin_sll
 * PURPOSE:  Free every heap-allocated AdminNode in the SLL.
 * PARAMS:   head — pointer to the first AdminNode
 * OUTPUT:   (none)
 */
void free_admin_sll(AdminNode* head) {
    AdminNode* curr = head;
    while (curr != NULL) {
        AdminNode* next = curr->next;
        free(curr);
        curr = next;
    }
}


/* ══════════════════════════════════════════════════════════════
   SLL UTILITY FUNCTIONS
   Generic helpers that operate on SLL heads.
   ══════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: sll_count_orders
 * PURPOSE:  Count the number of nodes in an OrderNode SLL.
 *           Used when generating the next ORD ID.
 * PARAMS:   head — pointer to the first OrderNode
 * OUTPUT:   Integer count of nodes.
 */
int sll_count_orders(OrderNode* head) {
    int count = 0;
    OrderNode* curr = head;
    while (curr != NULL) {
        count++;
        curr = curr->next;
    }
    return count;
}

/*
 * FUNCTION: sll_count_users
 * PURPOSE:  Count the number of nodes in a UserNode SLL.
 *           Used when generating the next U ID during registration.
 * PARAMS:   head — pointer to the first UserNode
 * OUTPUT:   Integer count of nodes.
 */
int sll_count_users(UserNode* head) {
    int count = 0;
    UserNode* curr = head;
    while (curr != NULL) {
        count++;
        curr = curr->next;
    }
    return count;
}


/* ══════════════════════════════════════════════════════════════
   CLL ADAPTER: cll_build_from_sll
   (Replaces the old cll_build() which read from delivery_boys.txt)

   The CLL is no longer built directly from a file. Instead it is
   built from the in-memory DeliveryBoy SLL returned by
   load_delivery_boy_sll(). This enforces the rule that ALL file
   access must go through the utils.c load/save layer.
   ══════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: cll_build_from_sll
 * PURPOSE:  Build a Circular Linked List of ACTIVE delivery boys
 *           from an already-loaded DeliveryBoy SLL (no file I/O here).
 *           The CLL is used by cll_assign_delivery() for round-robin.
 *
 * PARAMS:   sll_head — head of the DeliveryBoy SLL from load_delivery_boy_sll()
 * OUTPUT:   Pointer to the head of the CLL, or NULL if no active boys found.
 *
 * HOW THE CIRCLE IS FORMED:
 *   After each new node is added, tail->next = head, so the last node
 *   always points back to the first — forming a closed ring.
 */
DeliveryNode* cll_build_from_sll(DeliveryBoyNode* sll_head) {
    DeliveryNode* head = NULL;
    DeliveryNode* tail = NULL;

    DeliveryBoyNode* curr = sll_head;
    while (curr != NULL) {
        /* Guard: only active delivery boys join the round-robin ring */
        if (!curr->data.is_active) {
            curr = curr->next;
            continue;
        }

        DeliveryNode* node = (DeliveryNode*)malloc(sizeof(DeliveryNode));
        if (!node) {
            curr = curr->next;
            continue;
        }

        node->boy  = curr->data;
        node->next = NULL;

        if (!head) {
            head       = node;
            tail       = node;
            node->next = head;   /* Circle of one: points to itself */
        } else {
            tail->next = node;   /* Old tail → new node             */
            tail       = node;   /* Advance tail pointer            */
            tail->next = head;   /* New tail closes the circle      */
        }

        curr = curr->next;
    }

    return head;
}

/*
 * FUNCTION: cll_assign_delivery  (UPDATED for binary storage)
 * PURPOSE:  Use Round-Robin to pick the NEXT delivery boy for an order.
 *           After picking, persists the updated last_assigned flags back
 *           to delivery_boys.dat via the DeliveryBoy SLL.
 *
 * ALGORITHM:
 *   1. Count nodes in the CLL (prevents infinite loop — CLL has no NULL).
 *   2. Find the boy with last_assigned == 1 (he got the PREVIOUS order).
 *   3. Move to his ->next. THAT boy gets THIS order.
 *   4. Update flags: previous boy's last_assigned = 0, new boy's = 1.
 *   5. Mirror the updated flags into the sll_head (for save_delivery_boy_sll).
 *   6. Save the updated SLL back to delivery_boys.dat.
 *
 * PARAMS:
 *   head     — CLL head from cll_build_from_sll()
 *   out_boy  — filled with the chosen DeliveryBoy on success
 *   sll_head — the original SLL (needed to persist updated flags)
 *
 * OUTPUT:   1 on success, 0 if no active boys available.
 */
int cll_assign_delivery(DeliveryNode* head, DeliveryBoy* out_boy,
                        DeliveryBoyNode* sll_head) {
    if (!head) return 0;

    /* ── Step 1: Count CLL nodes to cap the traversal ────────── */
    int count = 0;
    DeliveryNode* walker = head;
    do {
        count++;
        walker = walker->next;
    } while (walker != head);

    /* ── Step 2: Find the boy who was last assigned ────────────── */
    DeliveryNode* curr     = head;
    DeliveryNode* chosen   = NULL;
    DeliveryNode* prev_boy = NULL;
    int found = 0;

    for (int i = 0; i < count; i++) {
        if (curr->boy.last_assigned == 1) {
            prev_boy = curr;
            chosen   = curr->next;   /* Round-robin: NEXT boy gets this order */
            found    = 1;
            break;
        }
        curr = curr->next;
    }

    /* ── First-order edge case: no one has the flag yet ─────────── */
    if (!found) chosen = head;

    /* ── Step 3: Update last_assigned flags in the CLL ──────────── */
    if (prev_boy) prev_boy->boy.last_assigned = 0;
    chosen->boy.last_assigned = 1;
    *out_boy = chosen->boy;

    /* ── Step 4: Mirror updated flags back into the persistent SLL ─ */
    DeliveryBoyNode* s = sll_head;
    while (s != NULL) {
        /* Match by boy_id to sync the CLL flag changes into the SLL */
        if (strcmp(s->data.boy_id, chosen->boy.boy_id) == 0) {
            s->data.last_assigned = 1;
        } else if (prev_boy && strcmp(s->data.boy_id, prev_boy->boy.boy_id) == 0) {
            s->data.last_assigned = 0;
        }
        s = s->next;
    }

    /* ── Step 5: Persist the updated SLL to delivery_boys.dat ────── */
    save_delivery_boy_sll(sll_head);

    return 1;
}
