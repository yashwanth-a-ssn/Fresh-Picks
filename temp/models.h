/*
 * models.h - Fresh Picks: GLOBAL Source of Truth (v4 — Binary Storage Edition)
 * ==============================================================================
 * This header is the SINGLE file every .c file must include.
 * It defines:
 *   1. Constants (string lengths, file paths, max sizes)
 *   2. ALL entity structs (User, Vegetable, Order, FreeItem, DeliveryBoy, AdminCreds)
 *   3. SLL Node structs for each entity (used by utils.c for binary I/O)
 *   4. Pre-existing DS structs (CartNode, QueueNode, DeliveryNode, MinHeap)
 *   5. Function prototypes for utils.c (formerly ds_utils.c)
 *
 * ARCHITECTURE CHANGE (v4):
 *   ALL data is now persisted as raw binary structs in .dat files.
 *   Text files (.txt) are no longer used for storage. The converter
 *   script (txt_to_bin_converter.c) performs the one-time migration.
 *   At runtime, utils.c loads each .dat file into an in-memory SLL
 *   and saves the SLL back to disk after any modification.
 *
 * ID FORMAT STANDARD (v4 — enforced across ALL entities):
 *   Every ID has a PREFIX followed by exactly 4 digits.
 *     Users        → U1001, U1002, ...
 *     Admins       → A1001, A1002, ...
 *     Vegetables   → V1001, V1002, ...
 *     Free Items   → VF1001, VF1002, ...
 *     Orders       → ORD1001, ORD1002, ...
 *     Delivery Boys→ D1001, D1002, ...
 *
 * HOW TO USE: Add this line at the top of every .c file:
 *   #include "models.h"
 *
 * CHANGELOG (v4):
 *   - Migrated all file path constants from .txt → .dat
 *   - Expanded buffer sizes to accommodate binary struct fields safely
 *   - Added SLL Node typedef for every entity struct
 *   - Added load_<entity>_sll() and save_<entity>_sll() prototypes
 *   - Standardised all IDs to prefix + 4 digits
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

   WHY LARGER BUFFERS?
   Binary structs are written to disk in their EXACT sizeof() form.
   Fields must be fixed-size arrays large enough to never overflow.
   Padding is cheap on disk; a buffer overflow is catastrophic.
   ═════════════════════════════════════════════════════════════ */

#define MAX_STR_LEN      128   /* Names, usernames, tags, phone, vehicle_no   */
#define MAX_ID_LEN        20   /* "U1001", "ORD1001", "VF1001" — with prefix  */
#define MAX_ADD_LEN      256   /* "Door,Street,Area,PIN" address string        */
#define MAX_LINE_LEN    2048   /* items_string: may hold 15+ items with names  */
#define MAX_ORDERS       200   /* Max orders the Min-Heap can hold at once      */
#define MAX_DELIVERY_BOYS 20   /* Max delivery boys in the Circular Linked List */
#define MAX_CART_ITEMS    50   /* Max distinct items a user's cart can hold     */
#define TIMESTAMP_LEN     32   /* "YYYY-MM-DD HH:MM:SS\0" with safe padding    */


/* ═════════════════════════════════════════════════════════════
   SECTION 2: BINARY FILE PATH CONSTANTS
   All persistent storage now uses .dat files.
   Each .dat file is a flat sequence of raw structs written by fwrite().
   ═════════════════════════════════════════════════════════════ */

/* Auth binary reads/writes these */
#define USERS_FILE        "users.dat"
#define ADMIN_FILE        "admin_creds.dat"

/* Order binary reads/writes these */
#define PRODUCTS_FILE     "products.dat"
#define ORDERS_FILE       "orders.dat"
#define FREE_INV_FILE     "free_inventory.dat"

/* Delivery binary reads/writes this */
#define DELIVERY_FILE     "delivery_boys.dat"

/* Inventory binary reads/writes these (aliases for clarity) */
#define SHOP_FILE         "shop_info.txt"   /* Shop info stays as plain text */

/* Cart directory — each user gets carts/<user_id>_cart.txt (pipe-delimited) */
#define CART_DIR          "carts/"

/* Delimiter used in pipe-delimited cart files and stdout output */
#define DELIMITER         "|"


/* ═════════════════════════════════════════════════════════════
   SECTION 3: OUTPUT MACROS
   ALL C binaries must print results in one of these two formats
   so bridge.py can split on '|' and parse status reliably.
   ═════════════════════════════════════════════════════════════ */

/* Usage: PRINT_SUCCESS("U1001")  →  prints: SUCCESS|U1001  */
#define PRINT_SUCCESS(data)   printf("SUCCESS|%s\n", (data))

/* Usage: PRINT_ERROR("Wrong password")  →  prints: ERROR|Wrong password  */
#define PRINT_ERROR(reason)   printf("ERROR|%s\n",   (reason))


/* ═════════════════════════════════════════════════════════════
   SECTION 4: ENTITY STRUCT DEFINITIONS
   Fixed-size, pointer-free structs that are safe to fread/fwrite.
   NO pointers inside any struct — binary I/O requires flat layout.
   ═════════════════════════════════════════════════════════════ */

/* ─────────────────────────────────────────────
   STRUCT: AdminCreds
   One admin account row loaded from admin_creds.dat
   DB Format: admin_id|username|password|admin_name|email
   ID Format: A1001, A1002, ...
   ───────────────────────────────────────────── */
typedef struct {
    char admin_id[MAX_ID_LEN];      /* e.g. "A1001"                        */
    char username[MAX_STR_LEN];     /* Login username, e.g. "Admin"        */
    char password[MAX_STR_LEN];     /* Plain-text password (demo only)     */
    char admin_name[MAX_STR_LEN];   /* Display name, e.g. "CodeCrafters"   */
    char email[MAX_STR_LEN];        /* e.g. "codecrafters@gmail.com"       */
} AdminCreds;

/* ─────────────────────────────────────────────
   STRUCT: User
   One customer account row loaded from users.dat
   DB Format: user_id|username|password|full_name|email|phone|address
   ID Format: U1001, U1002, ...
   ───────────────────────────────────────────── */
typedef struct {
    char user_id[MAX_ID_LEN];       /* e.g. "U1001"                        */
    char username[MAX_STR_LEN];     /* Login username                      */
    char password[MAX_STR_LEN];     /* Plain-text password (demo only)     */
    char full_name[MAX_STR_LEN];    /* Display name                        */
    char email[MAX_STR_LEN];        /* Email for comms                     */
    char phone[MAX_STR_LEN];        /* Phone number                        */
    char address[MAX_ADD_LEN];      /* "Door,Street,Area,PIN"              */
} User;

/* ─────────────────────────────────────────────
   STRUCT: Vegetable
   One product row loaded from products.dat
   DB Format: veg_id|category|name|stock_g|price_per_1000g|tag|validity_days
   ID Format: V1001, V1002, ...
   ───────────────────────────────────────────── */
typedef struct {
    char  veg_id[MAX_ID_LEN];           /* e.g. "V1001"             */
    char  category[MAX_STR_LEN];        /* e.g. "Allium"            */
    char  name[MAX_STR_LEN];            /* e.g. "Onion"             */
    int   stock_g;                      /* In grams, e.g. 50000     */
    float price_per_1000g;              /* Price per 1kg, e.g. 40.0 */
    char  tag[MAX_STR_LEN];             /* e.g. "Farm Fresh"        */
    int   validity_days;                /* Shelf life in days        */
} Vegetable;

/* ─────────────────────────────────────────────
   STRUCT: FreeItem
   One promotional freebie loaded from free_inventory.dat
   DB Format: vf_id|name|stock_g|min_trigger_amt|free_qty_g
   ID Format: VF1001, VF1002, ...

   NOTE: VF items are also listed in products.dat under the same
   VF-prefixed ID for receipt and display purposes. The free_inventory.dat
   file governs STOCK and TRIGGER logic; products.dat governs display.
   ───────────────────────────────────────────── */
typedef struct {
    char  vf_id[MAX_ID_LEN];        /* e.g. "VF1001"                          */
    char  name[MAX_STR_LEN];        /* e.g. "Curry Leaves"                    */
    int   stock_g;                  /* Current free stock in grams             */
    float min_trigger_amt;          /* Cart total (₹) needed to unlock freebie */
    int   free_qty_g;               /* Grams given free per qualifying order   */
} FreeItem;

/* ─────────────────────────────────────────────
   STRUCT: DeliveryBoy
   One row loaded from delivery_boys.dat
   DB Format: boy_id|name|phone|vehicle_no|is_active|last_assigned
   ID Format: D1001, D1002, ...
   ───────────────────────────────────────────── */
typedef struct {
    char boy_id[MAX_ID_LEN];        /* e.g. "D1001"                            */
    char name[MAX_STR_LEN];         /* e.g. "Ramesh"                           */
    char phone[MAX_STR_LEN];        /* e.g. "9876543210"                       */
    char vehicle_no[MAX_STR_LEN];   /* e.g. "TN-22-AB-1234"                    */
    int  is_active;                 /* 1 = on duty, 0 = off duty               */
    int  last_assigned;             /* Round-robin flag: 1 = got the last order */
} DeliveryBoy;

/* ─────────────────────────────────────────────
   STRUCT: Order
   One row loaded from orders.dat
   DB Format: order_id|user_id|total|slot|boy_id|status|timestamp|items_string
   ID Format: ORD1001, ORD1002, ...

   items_string format (normalised in v4):
     "V1001:Onion:500:40.00,VF1001:Curry Leaves:50:0.00"
     Each token: veg_id:name:qty_g:price_at_order
     price_at_order is a SNAPSHOT — never changes after placement.
   ───────────────────────────────────────────── */
typedef struct {
    char  order_id[MAX_ID_LEN];           /* e.g. "ORD1001"                 */
    char  user_id[MAX_ID_LEN];            /* e.g. "U1001"                   */
    float total_amount;                   /* Grand total in Rupees           */
    char  delivery_slot[MAX_STR_LEN];     /* "Morning","Afternoon","Evening" */
    char  delivery_boy_id[MAX_ID_LEN];    /* e.g. "D1001"                   */
    char  status[MAX_STR_LEN];            /* "Order Placed","Out for Delivery",
                                             "Delivered","Cancelled"         */
    char  timestamp[TIMESTAMP_LEN];       /* "YYYY-MM-DD HH:MM:SS"          */
    char  items_string[MAX_LINE_LEN];     /* Comma-separated item tokens     */
    int   slot_priority;                  /* 1=Morning,2=Afternoon,3=Evening
                                             Used by Min-Heap for sorting    */
} Order;


/* ═════════════════════════════════════════════════════════════
   SECTION 5: SLL NODE STRUCTS (for utils.c binary I/O)

   Each entity gets its own Singly Linked List node.
   load_<entity>_sll() builds an in-memory SLL from the .dat file.
   save_<entity>_sll() writes the SLL back to the .dat file.

   WHY SLL and not an array?
   The number of records in each .dat file is not known at compile
   time. An SLL grows dynamically as fread() consumes records,
   so no hardcoded array limit can ever overflow.
   ═════════════════════════════════════════════════════════════ */

/* SLL node for User */
typedef struct UserNode {
    User             data;
    struct UserNode *next;
} UserNode;

/* SLL node for Vegetable */
typedef struct VegNode {
    Vegetable       data;
    struct VegNode *next;
} VegNode;

/* SLL node for Order */
typedef struct OrderNode {
    Order             data;
    struct OrderNode *next;
} OrderNode;

/* SLL node for FreeItem */
typedef struct FreeItemNode {
    FreeItem             data;
    struct FreeItemNode *next;
} FreeItemNode;

/* SLL node for DeliveryBoy */
typedef struct DeliveryBoyNode {
    DeliveryBoy             data;
    struct DeliveryBoyNode *next;
} DeliveryBoyNode;

/* SLL node for AdminCreds */
typedef struct AdminNode {
    AdminCreds       data;
    struct AdminNode *next;
} AdminNode;


/* ═════════════════════════════════════════════════════════════
   SECTION 6: PRE-EXISTING DATA STRUCTURE NODE / CONTAINER STRUCTS
   Used by the DLL (cart), Queue (order processing),
   CLL (delivery allocation), and Min-Heap (admin dispatch).
   Kept intact from v3 — these operate on in-memory runtime data,
   NOT on the persistent .dat files.
   ═════════════════════════════════════════════════════════════ */

/* ─────────────────────────────────────────────
   DOUBLY LINKED LIST NODE: CartNode
   Each node represents ONE item in a user's cart.
   Built at runtime from the user's cart file; NOT a .dat entity.
   ───────────────────────────────────────────── */
typedef struct CartNode {
    char  veg_id[MAX_ID_LEN];    /* Which vegetable? e.g. "V1001"              */
    char  name[MAX_STR_LEN];     /* Display name, e.g. "Onion"                 */
    int   qty_g;                 /* How many grams the user wants              */
    float price_per_1000g;       /* Price at time of adding to cart            */
    float item_total;            /* qty_g / 1000.0 * price_per_1000g           */
    int   is_free;               /* 1 = freebie (₹0), 0 = paid item           */

    struct CartNode *prev;       /* ← Points BACK to the previous node        */
    struct CartNode *next;       /* → Points FORWARD to the next node         */
} CartNode;

/* ─────────────────────────────────────────────
   STANDARD QUEUE NODE: QueueNode
   Each node holds one complete Order.
   Used for FIFO order processing after checkout.
   ───────────────────────────────────────────── */
typedef struct QueueNode {
    Order order;
    struct QueueNode *next;
} QueueNode;

/* ─────────────────────────────────────────────
   STANDARD QUEUE CONTAINER: OrderQueue
   Tracks both front (oldest = next to process)
   and rear (newest = just joined the queue).
   ───────────────────────────────────────────── */
typedef struct {
    QueueNode *front;
    QueueNode *rear;
    int        size;
} OrderQueue;

/* ─────────────────────────────────────────────
   CIRCULAR LINKED LIST NODE: DeliveryNode
   Each node holds one DeliveryBoy.
   Built at runtime from the in-memory DeliveryBoy SLL.
   The LAST node's next → FIRST node (forms a circle).
   ───────────────────────────────────────────── */
typedef struct DeliveryNode {
    DeliveryBoy         boy;
    struct DeliveryNode *next;
} DeliveryNode;

/* ─────────────────────────────────────────────
   MIN-HEAP CONTAINER: MinHeap
   Stores up to MAX_ORDERS orders in a flat array.
   Always keeps the MOST URGENT order at index 0.
   (Lower slot_priority number = more urgent)
   ───────────────────────────────────────────── */
typedef struct {
    Order data[MAX_ORDERS];
    int   size;
} MinHeap;


/* ═════════════════════════════════════════════════════════════
   SECTION 7: FUNCTION PROTOTYPES FOR utils.c

   WHY ARE PROTOTYPES NEEDED?
   When order.c calls load_user_sll(), the compiler needs to know
   the function signature BEFORE it sees the implementation in utils.c.
   These prototypes serve as that advance notice.
   ═════════════════════════════════════════════════════════════ */

/* ── SLL Load / Save (Binary I/O Abstraction Layer) ── */
UserNode*        load_user_sll(void);
void             save_user_sll(UserNode* head);
void             free_user_sll(UserNode* head);

VegNode*         load_veg_sll(void);
void             save_veg_sll(VegNode* head);
void             free_veg_sll(VegNode* head);

OrderNode*       load_order_sll(void);
void             save_order_sll(OrderNode* head);
void             free_order_sll(OrderNode* head);

FreeItemNode*    load_free_item_sll(void);
void             save_free_item_sll(FreeItemNode* head);
void             free_free_item_sll(FreeItemNode* head);

DeliveryBoyNode* load_delivery_boy_sll(void);
void             save_delivery_boy_sll(DeliveryBoyNode* head);
void             free_delivery_boy_sll(DeliveryBoyNode* head);

AdminNode*       load_admin_sll(void);
void             save_admin_sll(AdminNode* head);
void             free_admin_sll(AdminNode* head);

/* ── SLL Utility helpers ── */
int  sll_count_orders(OrderNode* head);
int  sll_count_users(UserNode* head);

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

/* ── Circular Linked List (Delivery Boys — built from SLL at runtime) ── */
DeliveryNode* cll_build_from_sll(DeliveryBoyNode* sll_head);
int           cll_assign_delivery(DeliveryNode* head, DeliveryBoy* out_boy,
                                  DeliveryBoyNode* sll_head);
void          cll_free(DeliveryNode* head);

/* ── Min-Heap (Admin Priority Queue) ── */
void heap_swap(MinHeap* h, int i, int j);
void heap_heapify_up(MinHeap* h, int idx);
void heap_heapify_down(MinHeap* h, int idx);
void heap_insert(MinHeap* h, Order o);
int  heap_extract_min(MinHeap* h, Order* out);


#endif /* End of MODELS_H include guard */
