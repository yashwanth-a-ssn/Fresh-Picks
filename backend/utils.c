/*
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


 /* ══════════════════════════════════════════════════════════════
   WINDOWS-NATIVE LOCKING REPLACEMENT
   Replaces Linux flock() with Windows _locking()
   ══════════════════════════════════════════════════════════════ */
#include <io.h>           /* For _fileno */
#include <sys/locking.h>  /* For _LK_LOCK, _LK_UNLCK */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "models.h"   /* All struct definitions + function prototypes live here */

 /* ══════════════════════════════════════════════════════════════
   WINDOWS-NATIVE LOCKING REPLACEMENT
   Replaces Linux flock() with Windows _locking()
   ══════════════════════════════════════════════════════════════ */
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
        _locking(_fileno(fp), _LK_LOCK, 1);             \
    } while (0)

#define SLL_SAVE_OPEN(filepath, fp)                     \
    do {                                                \
        (fp) = fopen((filepath), "wb");                 \
        if (!(fp)) return;                              \
        _locking(_fileno(fp), _LK_LOCK, 1);             \
    } while (0)

#define SLL_CLOSE(fp)                                   \
    do {                                                \
        _locking(_fileno(fp), _LK_UNLCK, 1);            \
        fclose(fp);                                     \
    } while (0)




// ==============================================================
// DEPRECATED, works only in Linux.

// #include <sys/file.h>   /* flock() — file locking for concurrent access safety */

/* ══════════════════════════════════════════════════════════════
   INTERNAL HELPER MACROS (used only inside this file)

   SLL_LOAD_OPEN(filepath, fp)
     Opens the .dat file for reading. Returns NULL on failure.

   SLL_SAVE_OPEN(filepath, fp)
     Opens the .dat file for writing (truncates). Returns on failure.
   ══════════════════════════════════════════════════════════════ */

/*
// #define SLL_LOAD_OPEN(filepath, fp)                     \
//     do {                                                \
//         (fp) = fopen((filepath), "rb");                 \
//         if (!(fp)) return NULL;                         \
//         flock(fileno(fp), LOCK_SH);                     \
//     } while (0)

// #define SLL_SAVE_OPEN(filepath, fp)                     \
//     do {                                                \
//         (fp) = fopen((filepath), "wb");                 \
//         if (!(fp)) return;                              \
//         flock(fileno(fp), LOCK_EX);                     \
//     } while (0)

// #define SLL_CLOSE(fp)                                   \
//     do {                                                \
//         flock(fileno(fp), LOCK_UN);                     \
//         fclose(fp);                                     \
//     } while (0)
// ==============================================================
*/


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

/* ══════════════════════════════════════════════════════════════
   DATA STRUCTURE 1: DOUBLY LINKED LIST (Cart)
   
   WHAT IS A DLL?
   A chain of nodes where each node has:
     - Cargo: the item data (veg_id, name, qty, price)
     - A FORWARD pointer (next) → next item in cart
     - A BACKWARD pointer (prev) ← previous item in cart

   WHY DOUBLY LINKED (not singly)?
   When we remove an item, we need to re-link its neighbors:
     Before:  [A] <-> [B] <-> [C]
     Remove B: [A] <---------> [C]
   With a DLL, node B itself has a pointer to both A and C.
   We don't need to re-scan from the head to find A.
   A singly linked list would require scanning from the head
   to find the node BEFORE B — that's O(n) extra work.
   ══════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: dll_create_node
 * PURPOSE:  Allocate memory for a new cart item node and fill its fields.
 *
 * WHY malloc()?
 *   malloc("memory allocate") reserves memory from the HEAP at runtime.
 *   Normal variables (int x = 5) live on the STACK and disappear when
 *   the function returns. Heap memory persists until we call free().
 *   Since our cart DLL outlives individual function calls, we use the heap.
 *
 * PARAMS:
 *   veg_id        — "V1001"
 *   name          — "Onion"
 *   qty_g         — e.g. 500
 *   price_per_1000g — e.g. 40.0
 *   is_free       — 1 if this is a freebie, 0 if paid
 *
 * RETURNS: Pointer to the new node, or NULL if allocation failed.
 */
CartNode* dll_create_node(const char* veg_id, const char* name,
                          int qty_g, float price_per_1000g, int is_free) {
    /* Allocate memory for ONE CartNode struct on the heap */
    CartNode* node = (CartNode*)malloc(sizeof(CartNode));
    if (!node) return NULL;  /* malloc returns NULL if system is out of memory */

    /* Copy string data safely — strncpy prevents buffer overflow */
    strncpy(node->veg_id, veg_id, MAX_ID_LEN  - 1);
    strncpy(node->name,   name,   MAX_STR_LEN - 1);

    /* Null-terminate manually to be safe */
    node->veg_id[MAX_ID_LEN  - 1] = '\0';
    node->name  [MAX_STR_LEN - 1] = '\0';

    /* Set numeric fields */
    node->qty_g           = qty_g;
    node->price_per_1000g = price_per_1000g;
    node->item_total      = (qty_g / 1000.0f) * price_per_1000g;
    node->is_free         = is_free;

    /* New node has no neighbors yet */
    node->prev = NULL;
    node->next = NULL;

    return node;
}

/*
 * FUNCTION: dll_append
 * PURPOSE:  Add a new CartNode at the END (tail) of the DLL.
 *
 * WHY "**head" (double pointer)?
 *   head is a pointer to the first node. If the list is empty,
 *   we need to make the new node become the head. To change where
 *   head POINTS (not just what it points to), we need a pointer
 *   to the pointer — that's "**head".
 *
 * STEPS:
 *   1. If list is empty → new node IS the head.
 *   2. Otherwise, walk to the last node (curr->next == NULL).
 *   3. Link: old tail's next → new node, new node's prev → old tail.
 */
void dll_append(CartNode** head, CartNode* new_node) {
    if (*head == NULL) {
        /* Empty list — the new node is the only node */
        *head = new_node;
        return;
    }

    /* Walk to the tail (the last node) */
    CartNode* curr = *head;
    while (curr->next != NULL) {
        curr = curr->next;
    }

    /* Link the new node as the new tail */
    curr->next     = new_node;  /* Old tail now points forward to new node */
    new_node->prev = curr;      /* New node points backward to old tail     */
    /* new_node->next remains NULL — it is the new tail */
}

/*
 * FUNCTION: dll_update_or_append
 * PURPOSE:  If the item (by veg_id) already exists in the cart, UPDATE its
 *           quantity. If it doesn't exist, ADD it as a new node.
 *
 * WHY "update" instead of always adding?
 *   UX reason: If a user clicks "Add 500g" for Onion twice, they expect
 *   ONE row showing 500g in their cart — not two rows each showing 500g.
 *   This matches "set quantity" behavior, not "accumulate" behavior.
 *
 * NOTE: qty_g OVERWRITES the old quantity (it doesn't add to it).
 *       The frontend always sends the DESIRED final quantity.
 */
void dll_update_or_append(CartNode** head, const char* veg_id, const char* name,
                          int qty_g, float price_per_1000g, int is_free) {
    /* Scan the DLL for an existing node with the same veg_id */
    CartNode* curr = *head;
    while (curr != NULL) {
        if (strcmp(curr->veg_id, veg_id) == 0) {
            /* Found! Update quantity and recalculate subtotal */
            curr->qty_g      = qty_g;
            curr->item_total = (qty_g / 1000.0f) * price_per_1000g;
            return;  /* Done — no need to create a new node */
        }
        curr = curr->next;
    }

    /* Not found in the DLL — create and append a brand new node */
    CartNode* node = dll_create_node(veg_id, name, qty_g, price_per_1000g, is_free);
    if (node) dll_append(head, node);
}

/*
 * FUNCTION: dll_remove
 * PURPOSE:  Delete a specific node from the DLL by veg_id.
 *
 * THIS IS THE KEY ADVANTAGE OF A DLL (explain in VIVA):
 *   Before: [prev] <-> [target] <-> [next]
 *   Step 1: prev->next = target->next   (skip target going forward)
 *   Step 2: next->prev = target->prev   (skip target going backward)
 *   Step 3: free(target)                (release the memory)
 *   After:  [prev] <-----------> [next]
 *
 * Edge cases handled:
 *   - Removing the HEAD node (no prev) → update *head pointer
 *   - Removing the TAIL node (no next) → no next->prev to update
 *   - Removing the ONLY node → both head and tail become NULL
 */
void dll_remove(CartNode** head, const char* veg_id) {
    CartNode* curr = *head;

    while (curr != NULL) {
        if (strcmp(curr->veg_id, veg_id) == 0) {
            /* Re-link: make the nodes before and after "skip" curr */
            if (curr->prev) curr->prev->next = curr->next;  /* Forward skip  */
            if (curr->next) curr->next->prev = curr->prev;  /* Backward skip */

            /* Special case: if we're removing the head, update *head */
            if (curr == *head) *head = curr->next;

            free(curr);   /* Return memory to the OS — NEVER forget this! */
            return;
        }
        curr = curr->next;
    }
    /* If veg_id wasn't found, do nothing (not an error) */
}

/*
 * FUNCTION: dll_get_total
 * PURPOSE:  Sum up item_total for every node in the DLL.
 *           Returns the cart grand total in Rupees.
 *           Used for: minimum order check (₹100) and freebie check (₹500).
 */
float dll_get_total(CartNode* head) {
    float total = 0.0f;
    CartNode* curr = head;
    while (curr != NULL) {
        total += curr->item_total;
        curr   = curr->next;
    }
    return total;
}

/*
 * FUNCTION: dll_free_all
 * PURPOSE:  Release the memory for EVERY node in the DLL.
 *
 * WHY IS THIS IMPORTANT?
 *   Every malloc() must be matched with a free(). If we don't free
 *   memory when we're done with a DLL, that memory stays "reserved"
 *   until the process ends — this is called a MEMORY LEAK.
 *   Always call dll_free_all() at the end of any function that
 *   loads or builds a cart DLL.
 */
void dll_free_all(CartNode* head) {
    CartNode* curr = head;
    while (curr != NULL) {
        CartNode* next = curr->next;  /* Save next BEFORE freeing curr! */
        free(curr);
        curr = next;
    }
}


/* ══════════════════════════════════════════════════════════════
   DATA STRUCTURE 2: STANDARD QUEUE (FIFO Order Processing)
   
   WHAT IS A QUEUE?
   Think of a queue at a grocery billing counter:
     - New customers join at the BACK (enqueue / rear).
     - The cashier serves the customer at the FRONT (dequeue).
     - First In, First Out (FIFO) — the first person to join
       is the first person to be served.

   WHY FOR ORDER PROCESSING?
   After a user pays, their order "joins the queue."
   Orders are dispatched in the same sequence they were placed —
   no customer jumps the line. This simulates a real-world
   order management system.
   ══════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: queue_init
 * PURPOSE:  Set up an empty queue. MUST call this before using any queue.
 *           An uninitialised queue has garbage pointer values → crashes!
 */
void queue_init(OrderQueue* q) {
    q->front = NULL;   /* No one in the queue yet */
    q->rear  = NULL;
    q->size  = 0;
}

/*
 * FUNCTION: queue_enqueue
 * PURPOSE:  Add a new Order to the BACK (rear) of the queue.
 *           Like a customer joining the end of the billing line.
 *
 * TIME COMPLEXITY: O(1) — we track the rear pointer, so we never
 * need to walk the whole queue to find the end.
 */
void queue_enqueue(OrderQueue* q, Order o) {
    /* Allocate a new node to hold this order */
    QueueNode* node = (QueueNode*)malloc(sizeof(QueueNode));
    if (!node) return;  /* Safety: if malloc fails, skip silently */

    node->order = o;    /* Copy the entire Order struct into the node */
    node->next  = NULL; /* This new node is the last one — nothing after it */

    if (q->rear == NULL) {
        /* Queue was empty — this is BOTH the front and the rear */
        q->front = node;
        q->rear  = node;
    } else {
        /* Link old rear → new node, then update rear pointer */
        q->rear->next = node;
        q->rear       = node;
    }
    q->size++;
}

/*
 * FUNCTION: queue_dequeue
 * PURPOSE:  Remove and return the FRONT (oldest) order from the queue.
 *           Like the cashier calling the next customer.
 *
 * RETURNS: 1 on success, 0 if the queue is empty.
 * OUTPUT:  Fills *out with the dequeued order.
 *
 * TIME COMPLEXITY: O(1) — we always remove from the front.
 */
int queue_dequeue(OrderQueue* q, Order* out) {
    if (q->front == NULL) return 0;  /* Nothing to dequeue */

    QueueNode* temp = q->front;      /* Remember front so we can free it */
    *out = temp->order;              /* Copy order data to the caller     */
    q->front = temp->next;           /* Move front pointer to the next node */

    /* If queue is now empty, reset rear too */
    if (q->front == NULL) q->rear = NULL;

    free(temp);  /* Release the old front node's memory */
    q->size--;
    return 1;
}

/*
 * FUNCTION: queue_free
 * PURPOSE:  Free all remaining nodes in the queue (prevent memory leaks).
 *           We just keep dequeuing until the queue is empty.
 */
void queue_free(OrderQueue* q) {
    Order dummy;  /* Temporary Order to receive dequeued data (discarded) */
    while (queue_dequeue(q, &dummy));  /* Dequeue until 0 is returned */
}


/* ══════════════════════════════════════════════════════════════
   DATA STRUCTURE 3: CIRCULAR LINKED LIST (Delivery Allocation)
   
   WHAT IS A CLL?
   Like a regular singly linked list, EXCEPT:
     The LAST node's next pointer does NOT point to NULL.
     Instead, it points BACK to the FIRST node.
     This creates a perfect circle.

   WHY PERFECT FOR ROUND-ROBIN DELIVERY?
   Round-robin = "take turns, then repeat."
     Order 1 → Ramesh
     Order 2 → Suresh
     Order 3 → Ganesh
     Order 4 → Ramesh ← wraps back automatically!
   The circular structure handles the wrap-around naturally —
   no special "reset to beginning" code is needed.
   ══════════════════════════════════════════════════════════════ */


// DEPRECATED
/*
 * FUNCTION: cll_build
 * PURPOSE:  Read delivery_boys.txt and build a Circular Linked List.
 *           Each active delivery boy becomes one node in the circle.
 *
 * RETURNS: Pointer to the HEAD of the CLL, or NULL if file is empty/missing.
 *
 * HOW THE CIRCLE IS FORMED:
 *   While reading, we keep a 'tail' pointer.
 *   After each new node is added: tail->next = head
 *   This ensures the last node always points back to the first.
 */
// DeliveryNode* cll_build(void) {
//     FILE* fp = fopen(DELIVERY_FILE, "r");
//     if (!fp) return NULL;

//     DeliveryNode* head = NULL;  /* First node — the "top" of the circle */
//     DeliveryNode* tail = NULL;  /* Last node — its next will point to head */

//     char line[MAX_LINE_LEN];
//     while (fgets(line, sizeof(line), fp)) {
//         line[strcspn(line, "\n")] = '\0';
//         if (strlen(line) == 0) continue;

//         /* Parse: boy_id|name|phone|vehicle_no|is_active|last_assigned */
//         DeliveryBoy boy;
//         char* tok = strtok(line, "|");
//         if (!tok) continue;
//         strncpy(boy.boy_id,     tok, MAX_ID_LEN  - 1);  tok = strtok(NULL, "|");
//         if (!tok) continue;
//         strncpy(boy.name,       tok, MAX_STR_LEN - 1);  tok = strtok(NULL, "|");
//         if (!tok) continue;
//         strncpy(boy.phone,      tok, MAX_STR_LEN - 1);  tok = strtok(NULL, "|");
//         if (!tok) continue;
//         strncpy(boy.vehicle_no, tok, MAX_STR_LEN - 1);  tok = strtok(NULL, "|");
//         if (!tok) continue;
//         boy.is_active     = atoi(tok);                   tok = strtok(NULL, "|");
//         boy.last_assigned = tok ? atoi(tok) : 0;

//         /* Only add ACTIVE delivery boys to the CLL */
//         if (!boy.is_active) continue;

//         /* Create a new node */
//         DeliveryNode* node = (DeliveryNode*)malloc(sizeof(DeliveryNode));
//         if (!node) continue;
//         node->boy  = boy;
//         node->next = NULL;

//         if (head == NULL) {
//             /* First node — it is both head and tail */
//             head       = node;
//             tail       = node;
//             node->next = head;  /* Circle of one: points to itself */
//         } else {
//             /* Append to tail, then close the circle */
//             tail->next = node;  /* Old tail → new node */
//             tail       = node;  /* New tail is now this node */
//             tail->next = head;  /* New tail → head (closes the circle) */
//         }
//     }
//     fclose(fp);
//     return head;
// }


// FUNCTION OVERLOADING
/*
 * FUNCTION: cll_assign_delivery
 * PURPOSE:  Use Round-Robin to pick the NEXT delivery boy for an order.
 *
 * ALGORITHM (EXPLAIN THIS IN YOUR VIVA!):
 *   1. Count how many nodes are in the CLL (to avoid infinite loop).
 *   2. Find the boy where last_assigned == 1 (he got the PREVIOUS order).
 *   3. Move to that boy's ->next. THAT boy gets THIS order.
 *   4. Update flags: previous boy's last_assigned = 0, new boy's = 1.
 *   5. Rewrite delivery_boys.txt with updated flags.
 *
 * FIRST ORDER EDGE CASE:
 *   No one has last_assigned=1 yet → assign to the head (first boy).
 *
 * RETURNS: 1 on success (and fills *out_boy), 0 if no boys available.
 */
// int cll_assign_delivery(DeliveryNode* head, DeliveryBoy* out_boy) {
//     if (!head) return 0;  /* No delivery boys in the system */

//     /* ── Step 1: Count nodes to prevent an infinite loop ─────────
//        A CLL has no NULL — we'd loop forever without a counter. */
//     int count = 0;
//     DeliveryNode* walker = head;
//     do {
//         count++;
//         walker = walker->next;
//     } while (walker != head);

//     /* ── Step 2: Find the boy who was last assigned ────────────── */
//     DeliveryNode* curr     = head;
//     DeliveryNode* chosen   = NULL;  /* The boy who will get THIS order */
//     DeliveryNode* prev_boy = NULL;  /* The boy who got the LAST order  */
//     int found = 0;

//     for (int i = 0; i < count; i++) {
//         if (curr->boy.last_assigned == 1) {
//             prev_boy = curr;         /* Remember who had the flag */
//             chosen   = curr->next;   /* Round-robin: next boy in circle */
//             found    = 1;
//             break;
//         }
//         curr = curr->next;
//     }

//     if (!found) {
//         /* No one has the flag yet — this is the very first order.
//            Assign to the head (first active boy in the list). */
//         chosen = head;
//     }

//     /* ── Step 3 & 4: Update the last_assigned flags ─────────────── */
//     if (prev_boy) prev_boy->boy.last_assigned = 0;  /* Clear old flag */
//     chosen->boy.last_assigned = 1;                  /* Set new flag   */
//     *out_boy = chosen->boy;    /* Return the assigned boy's details    */

//     /* ── Step 5: Rewrite delivery_boys.txt with updated flags ────── */
//     FILE* fp = fopen(DELIVERY_FILE, "w");
//     if (!fp) return 0;

//     DeliveryNode* w = head;
//     for (int i = 0; i < count; i++) {
//         fprintf(fp, "%s|%s|%s|%s|%d|%d\n",
//             w->boy.boy_id,
//             w->boy.name,
//             w->boy.phone,
//             w->boy.vehicle_no,
//             w->boy.is_active,
//             w->boy.last_assigned
//         );
//         w = w->next;
//     }
//     fclose(fp);
//     return 1;
// }

/*
 * FUNCTION: cll_free
 * PURPOSE:  Free all nodes in the CLL.
 *
 * WHY CAN'T WE JUST WALK UNTIL NULL?
 *   A CLL has NO null terminator — we'd loop forever!
 *   SOLUTION: Count nodes first, then walk exactly that many times.
 */
void cll_free(DeliveryNode* head) {
    if (!head) return;

    /* Count how many nodes there are */
    int count = 0;
    DeliveryNode* curr = head;
    do {
        count++;
        curr = curr->next;
    } while (curr != head);

    /* Free exactly 'count' nodes */
    curr = head;
    for (int i = 0; i < count; i++) {
        DeliveryNode* next = curr->next;
        free(curr);
        curr = next;
    }
}


/* ══════════════════════════════════════════════════════════════
   DATA STRUCTURE 4: MIN-HEAP (Admin Priority Queue)
   
   WHAT IS A MIN-HEAP?
   A COMPLETE BINARY TREE where every parent is SMALLER than
   (or equal to) its children. "Complete" means every level is
   full except possibly the last, which fills left-to-right.
   
   WHY STORE IT IN AN ARRAY?
   A complete binary tree maps perfectly onto an array:
     Root        → index 0
     Left child  of node i → index (2*i + 1)
     Right child of node i → index (2*i + 2)
     Parent      of node i → index ((i-1) / 2)
   No pointer overhead — just simple arithmetic!

   WHY MIN-HEAP FOR ORDERS?
   Admin needs the MOST URGENT orders first:
     Priority 1 = Morning   (dispatch first!)
     Priority 2 = Afternoon
     Priority 3 = Evening
   The Min-Heap always keeps the Priority-1 order at index 0 (root),
   so the admin always sees Morning deliveries at the top.
   ══════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: heap_swap
 * PURPOSE:  Swap two Order elements inside the heap array.
 *           Used by heapify_up and heapify_down to reorder elements.
 */
void heap_swap(MinHeap* h, int i, int j) {
    Order temp = h->data[i];
    h->data[i] = h->data[j];
    h->data[j] = temp;
}

/*
 * FUNCTION: heap_heapify_up  (also called "Bubble Up")
 * PURPOSE:  After inserting a new element at the END of the array,
 *           move it UP the tree until the min-heap property is restored.
 *
 * VISUAL EXAMPLE:
 *   Insert Priority=1 (Morning) at the last position.
 *   It's smaller than its parent → swap.
 *   Keep swapping upward until parent ≤ this node, or we reach the root.
 *
 *   Parent formula: parent = (idx - 1) / 2
 */
void heap_heapify_up(MinHeap* h, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        /* If this node has a LOWER priority number (more urgent) than its parent, swap */
        if (h->data[idx].slot_priority < h->data[parent].slot_priority) {
            heap_swap(h, idx, parent);
            idx = parent;  /* Continue checking upward from the new position */
        } else {
            break;  /* Heap property is satisfied — stop bubbling up */
        }
    }
}

/*
 * FUNCTION: heap_insert
 * PURPOSE:  Add a new Order into the Min-Heap.
 *
 * ALGORITHM:
 *   1. Place the new order at the END of the array (maintaining "complete" shape).
 *   2. Bubble it UP to its correct position (heap_heapify_up).
 *
 * TIME COMPLEXITY: O(log n) — at most, the element bubbles up the full height
 *                              of the tree, which is log₂(n) levels.
 */
void heap_insert(MinHeap* h, Order o) {
    if (h->size >= MAX_ORDERS) return;  /* Heap is full — don't overflow the array */
    h->data[h->size] = o;               /* Place at the last position              */
    heap_heapify_up(h, h->size);        /* Restore heap property by bubbling up    */
    h->size++;                          /* Increase the count                      */
}

/*
 * FUNCTION: heap_heapify_down  (also called "Sift Down")
 * PURPOSE:  After removing the root (minimum element), the last array element
 *           is placed at the root. Then it "sinks down" to its correct place.
 *
 * ALGORITHM:
 *   1. Compare current node with its left child (2*idx+1) and right child (2*idx+2).
 *   2. Find which of the three is smallest.
 *   3. If a child is smaller → swap and continue downward.
 *   4. If current is already smallest → stop.
 */
void heap_heapify_down(MinHeap* h, int idx) {
    while (1) {
        int left     = 2 * idx + 1;  /* Left child index  */
        int right    = 2 * idx + 2;  /* Right child index */
        int smallest = idx;           /* Assume current is smallest */

        /* Is left child smaller? */
        if (left < h->size &&
            h->data[left].slot_priority < h->data[smallest].slot_priority)
            smallest = left;

        /* Is right child even smaller? */
        if (right < h->size &&
            h->data[right].slot_priority < h->data[smallest].slot_priority)
            smallest = right;

        if (smallest != idx) {
            heap_swap(h, idx, smallest);  /* Sink down */
            idx = smallest;              /* Continue from the new position */
        } else {
            break;  /* Heap property satisfied — stop sinking */
        }
    }
}

/*
 * FUNCTION: heap_extract_min
 * PURPOSE:  Remove and return the ORDER with the LOWEST slot_priority.
 *           (i.e., the most urgent Morning order gets dispatched first.)
 *
 * ALGORITHM (THE CLASSIC HEAP EXTRACT — EXPLAIN IN VIVA!):
 *   1. Save root (index 0) — it's the minimum (most urgent).
 *   2. Move the LAST element to the root position.
 *   3. Decrease the heap size.
 *   4. Sift the new root DOWN to restore the heap property.
 *
 * RETURNS: 1 on success (fills *out with the minimum order), 0 if empty.
 * TIME COMPLEXITY: O(log n)
 */
int heap_extract_min(MinHeap* h, Order* out) {
    if (h->size == 0) return 0;       /* Nothing to extract */

    *out = h->data[0];                /* Step 1: Save the root (minimum) */
    h->data[0] = h->data[h->size-1]; /* Step 2: Move last element to root */
    h->size--;                        /* Step 3: Shrink the heap           */

    if (h->size > 0)
        heap_heapify_down(h, 0);     /* Step 4: Restore heap property     */

    return 1;
}
