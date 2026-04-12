/*
 * models.h - Fresh Picks: GLOBAL Source of Truth (v3)
 * =====================================================
 * This header is the SINGLE file every .c file must include.
 * It defines:
 *   1. Constants (string lengths, file paths, max sizes)
 *   2. ALL data structs (Vegetable, Order, User, etc.)
 *   3. Function prototypes for ds_utils.c
 *      (so order.c and inventory.c can CALL those functions)
 *
 * WHY A HEADER FILE?
 *   In C, if file A.c wants to call a function from file B.c,
 *   A.c must first "declare" that function exists. A header (.h)
 *   file contains those declarations. The linker then connects
 *   the declaration (in .h) to the actual implementation (in .c).
 *
 * HOW TO USE: Add this line at the top of every .c file:
 *   #include "models.h"
 *
 * CHANGELOG (v3):
 *   - Moved ALL struct definitions here from order.c
 *   - Added timestamp field to Order struct
 *   - Added function prototypes for ds_utils.c
 *   - Added new file path defines (FREE_INV_FILE, DELIVERY_FILE, etc.)
 *   - Updated Order status values to match new workflow
 *
 * Team: CodeCrafters | Project: Fresh Picks | SDP-1
 */

/* ─────────────────────────────────────────────────────────────
   INCLUDE GUARD
   Prevents this file from being included more than once.
   If models.h is included in 3 files, the compiler only processes
   it once — avoiding "duplicate definition" errors.
   ───────────────────────────────────────────────────────────── */
#ifndef MODELS_H
#define MODELS_H

/* Standard headers needed for types used in this file */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* ═════════════════════════════════════════════════════════════
   SECTION 1: SIZE CONSTANTS
   ═════════════════════════════════════════════════════════════ */

#define MAX_STR_LEN     100   /* Max length for names, usernames, tags, etc.  */
#define MAX_LINE_LEN    512   /* Max length for one line in a .txt DB file     */
#define MAX_ID_LEN       20   /* Max length for IDs like "U1001", "ORD101"     */
#define MAX_ADD_LEN     256   /* Max length for a user's address field          */
#define MAX_ORDERS      200   /* Max orders the Min-Heap can hold at once       */
#define MAX_DELIVERY_BOYS 20  /* Max delivery boys in the Circular Linked List  */
#define MAX_CART_ITEMS   50   /* Max items a single user's cart can hold        */
#define TIMESTAMP_LEN    30   /* Length of "YYYY-MM-DD HH:MM:SS" string         */


/* ═════════════════════════════════════════════════════════════
   SECTION 2: FILE PATH CONSTANTS
   Change these if your folder layout ever changes.
   ═════════════════════════════════════════════════════════════ */

/* Auth binary reads/writes these */
#define USERS_FILE        "users.txt"
#define ADMIN_FILE        "admin_creds.txt"

/* Order binary reads/writes these */
#define PRODUCTS_FILE     "products.txt"
#define ORDERS_FILE       "orders.txt"
#define DELIVERY_FILE     "delivery_boys.txt"
#define FREE_INV_FILE     "free_inventory.txt"
#define CART_DIR          "carts/"   /* Each user gets carts/<user_id>_cart.txt */

/* Inventory binary reads/writes these (same as above, aliased for clarity) */
#define SHOP_FILE         "shop_info.txt"

/* Delimiter used in EVERY .txt database file */
#define DELIMITER         "|"


/* ═════════════════════════════════════════════════════════════
   SECTION 3: OUTPUT MACROS
   Always use these so Flask can reliably split on '|'.
   ═════════════════════════════════════════════════════════════ */

/* Usage: PRINT_SUCCESS("U1001")  →  prints: SUCCESS|U1001  */
#define PRINT_SUCCESS(data)   printf("SUCCESS|%s\n", (data))

/* Usage: PRINT_ERROR("Wrong password")  →  prints: ERROR|Wrong password  */
#define PRINT_ERROR(reason)   printf("ERROR|%s\n",   (reason))


/* ═════════════════════════════════════════════════════════════
   SECTION 4: STRUCT DEFINITIONS
   These are the "blueprints" for every type of data in the app.
   ═════════════════════════════════════════════════════════════ */

/* ─────────────────────────────────────────────
   STRUCT: AdminCreds
   One admin account loaded from admin_creds.txt
   DB Format: admin_id|username|password|admin_name|email
   ───────────────────────────────────────────── */
typedef struct {
    char admin_id[MAX_ID_LEN];    /* e.g. "A1001"                      */
    char username[MAX_STR_LEN];   /* Login username, e.g. "Admin"       */
    char password[MAX_STR_LEN];   /* Plain-text password (demo only)    */
    char admin_name[MAX_STR_LEN]; /* Display name, e.g. "CodeCrafters"  */
    char email[MAX_STR_LEN];      /* e.g. "codecrafters@gmail.com"      */
} AdminCreds;

/* ─────────────────────────────────────────────
   STRUCT: User
   One customer account row from users.txt
   DB Format: user_id|username|password|full_name|email|phone|address
   ───────────────────────────────────────────── */
typedef struct {
    char user_id[MAX_ID_LEN];     /* e.g. "U1001"                        */
    char username[MAX_STR_LEN];   /* Login username                      */
    char password[MAX_STR_LEN];   /* Plain-text password (demo only)     */
    char full_name[MAX_STR_LEN];  /* Display name                        */
    char email[MAX_STR_LEN];      /* Email for OTP/comms                 */
    char phone[MAX_STR_LEN];      /* Phone number                        */
    char address[MAX_ADD_LEN];    /* "Door,Street,Area,PIN"              */
} User;

/* ─────────────────────────────────────────────
   STRUCT: Vegetable
   One product row from products.txt
   DB Format: veg_id|category|name|stock_g|price_per_1000g|tag|validity_days
   ───────────────────────────────────────────── */
typedef struct {
    char  veg_id[MAX_ID_LEN];         /* e.g. "V1001"          */
    char  category[MAX_STR_LEN];      /* e.g. "Allium"         */
    char  name[MAX_STR_LEN];          /* e.g. "Onion"          */
    int   stock_g;                    /* In grams, e.g. 50000  */
    float price_per_1000g;            /* Price per 1kg, e.g. 40.0 */
    char  tag[MAX_STR_LEN];           /* e.g. "Farm Fresh"     */
    int   validity_days;              /* Shelf life, e.g. 7    */
} Vegetable;

/* ─────────────────────────────────────────────
   STRUCT: FreeItem
   One promotional freebie from free_inventory.txt
   DB Format: vf_id|name|stock_g|min_trigger_amt|free_qty_g
   ───────────────────────────────────────────── */
typedef struct {
    char  vf_id[MAX_ID_LEN];      /* e.g. "VF101"                         */
    char  name[MAX_STR_LEN];      /* e.g. "Curry Leaves"                  */
    int   stock_g;                /* Current free stock in grams           */
    float min_trigger_amt;        /* Cart total needed to unlock freebie   */
    int   free_qty_g;             /* Base free quantity in grams (per ₹500) */
} FreeItem;

/* ─────────────────────────────────────────────
   STRUCT: DeliveryBoy
   One row from delivery_boys.txt
   DB Format: boy_id|name|phone|vehicle_no|is_active|last_assigned
   ───────────────────────────────────────────── */
typedef struct {
    char boy_id[MAX_ID_LEN];      /* e.g. "D001"                           */
    char name[MAX_STR_LEN];       /* e.g. "Ramesh"                         */
    char phone[MAX_STR_LEN];      /* e.g. "9876543210"                     */
    char vehicle_no[MAX_STR_LEN]; /* e.g. "TN09AB1234"                     */
    int  is_active;               /* 1 = on duty, 0 = off duty             */
    int  last_assigned;           /* 1 = this boy got the LAST order       */
} DeliveryBoy;

/* ─────────────────────────────────────────────
   STRUCT: Order
   One row from orders.txt
   DB Format: order_id|user_id|total|slot|boy_id|status|timestamp|items_string
   
   NOTE: items_string format is now: veg_id:qty_g:price_at_order
     Example: "V1001:500:40.00,V1003:1000:60.00,VF101:50:0.00"
     The price_at_order is a SNAPSHOT of the price when the order was placed,
     so historical bills never change even if the price changes later.
   ───────────────────────────────────────────── */
typedef struct {
    char  order_id[MAX_ID_LEN];           /* e.g. "ORD101"                 */
    char  user_id[MAX_ID_LEN];            /* e.g. "U1001"                  */
    float total_amount;                   /* Grand total in Rupees          */
    char  delivery_slot[MAX_STR_LEN];     /* "Morning", "Afternoon", "Evening" */
    char  delivery_boy_id[MAX_ID_LEN];    /* e.g. "D001"                   */
    char  status[MAX_STR_LEN];            /* "Order Placed", "Out for Delivery",
                                             "Delivered", "Cancelled"       */
    char  timestamp[TIMESTAMP_LEN];       /* "2025-04-08 14:30:00" — NEW v3 */
    char  items_string[MAX_LINE_LEN];     /* "V1001:500:40.00,VF101:50:0.00" */
    int   slot_priority;                  /* 1=Morning, 2=Afternoon, 3=Evening
                                             Used by Min-Heap for sorting   */
} Order;


/* ═════════════════════════════════════════════════════════════
   SECTION 5: DATA STRUCTURE NODE / CONTAINER STRUCTS
   These are used by ds_utils.c. Defined here so every .c file
   that includes models.h can use these types.
   ═════════════════════════════════════════════════════════════ */

/* ─────────────────────────────────────────────
   DOUBLY LINKED LIST NODE: CartNode
   Each node represents ONE item in a user's cart.
   "Doubly" = has BOTH a prev AND a next pointer.
   ───────────────────────────────────────────── */
typedef struct CartNode {
    char  veg_id[MAX_ID_LEN];   /* Which vegetable? e.g. "V1001"             */
    char  name[MAX_STR_LEN];    /* Display name, e.g. "Onion"                */
    int   qty_g;                /* How many grams the user wants             */
    float price_per_1000g;      /* Price at time of adding to cart           */
    float item_total;           /* qty_g / 1000.0 * price_per_1000g          */
    int   is_free;              /* 1 = freebie (₹0), 0 = paid item          */

    struct CartNode *prev;      /* ← Points BACK to the previous node       */
    struct CartNode *next;      /* → Points FORWARD to the next node        */
} CartNode;

/* ─────────────────────────────────────────────
   STANDARD QUEUE NODE: QueueNode
   Each node holds one complete Order.
   Used for FIFO order processing after checkout.
   ───────────────────────────────────────────── */
typedef struct QueueNode {
    Order order;
    struct QueueNode *next;   /* Points to the node BEHIND this one */
} QueueNode;

/* ─────────────────────────────────────────────
   STANDARD QUEUE CONTAINER: OrderQueue
   Tracks both front (oldest = next to process)
   and rear (newest = just joined the queue).
   ───────────────────────────────────────────── */
typedef struct {
    QueueNode *front;   /* Served NEXT (oldest order) */
    QueueNode *rear;    /* Most recently added order  */
    int        size;
} OrderQueue;

/* ─────────────────────────────────────────────
   CIRCULAR LINKED LIST NODE: DeliveryNode
   Each node holds one DeliveryBoy.
   The LAST node's next → FIRST node (forms a circle).
   ───────────────────────────────────────────── */
typedef struct DeliveryNode {
    DeliveryBoy         boy;
    struct DeliveryNode *next;  /* Loops back to head when at the last boy */
} DeliveryNode;

/* ─────────────────────────────────────────────
   MIN-HEAP CONTAINER: MinHeap
   Stores up to MAX_ORDERS orders in an array.
   Always keeps the MOST URGENT order at index 0.
   (Lower slot_priority number = more urgent)
   ───────────────────────────────────────────── */
typedef struct {
    Order data[MAX_ORDERS];  /* The heap stored as a flat array              */
    int   size;              /* Number of orders currently in the heap       */
} MinHeap;


/* ═════════════════════════════════════════════════════════════
   SECTION 6: FUNCTION PROTOTYPES FOR ds_utils.c
   
   WHY ARE PROTOTYPES NEEDED?
   When order.c calls dll_append(), the compiler needs to know
   what arguments dll_append() accepts BEFORE it sees the
   actual implementation in ds_utils.c.
   These prototypes serve as that "advance notice."
   ═════════════════════════════════════════════════════════════ */

/* ── Doubly Linked List (Cart) ── */
CartNode* dll_create_node(const char* veg_id, const char* name,
                          int qty_g, float price_per_1000g, int is_free);
void      dll_append(CartNode** head, CartNode* new_node);
void      dll_update_or_append(CartNode** head, const char* veg_id,
                               const char* name, int qty_g,
                               float price_per_1000g, int is_free);
void      dll_remove(CartNode** head, const char* veg_id);
float     dll_get_total(CartNode* head);
void      dll_free_all(CartNode* head);

/* ── Standard Queue (Order Processing) ── */
void      queue_init(OrderQueue* q);
void      queue_enqueue(OrderQueue* q, Order o);
int       queue_dequeue(OrderQueue* q, Order* out);
void      queue_free(OrderQueue* q);

/* ── Circular Linked List (Delivery Boys) ── */
DeliveryNode* cll_build(void);
int           cll_assign_delivery(DeliveryNode* head, DeliveryBoy* out_boy);
void          cll_free(DeliveryNode* head);

/* ── Min-Heap (Admin Priority Queue) ── */
void heap_swap(MinHeap* h, int i, int j);
void heap_heapify_up(MinHeap* h, int idx);
void heap_heapify_down(MinHeap* h, int idx);
void heap_insert(MinHeap* h, Order o);
int  heap_extract_min(MinHeap* h, Order* out);


#endif /* End of MODELS_H include guard */
