/*
 * order.c - Fresh Picks: Shopping Cart, Payment & Order Management
 * =================================================================
 * This is the CORE C backend for the entire shopping flow.
 * It is called by Flask (app.py) using subprocess.run() like this:
 *
 *   ./order <command> [arguments...]
 *
 * ALL database reads/writes happen HERE in C. Flask only routes
 * HTTP requests and passes results back to the browser as JSON.
 *
 * ─────────────────────────────────────────────────────────────────
 * COMMANDS (argv[1]):
 *   list_products                     -> Read vegetables.txt, print all
 *   add_to_cart   <uid> <vid> <grams> -> Add/update item in cart file
 *   view_cart     <uid>               -> Read user's cart, print as pipe-delimited
 *   remove_item   <uid> <vid>         -> Remove one item from cart
 *   checkout      <uid> <slot>        -> Stock recheck + freebie + create order
 *   get_order     <uid> <order_id>    -> Get one order's details
 *   get_orders    <uid>               -> Get all orders for a user
 *   admin_orders                      -> Read all orders into priority queue, print sorted
 *
 * ─────────────────────────────────────────────────────────────────
 * DATA STRUCTURES USED (explain these in your VIVA!):
 *
 *  1. DOUBLY LINKED LIST (DLL) - for the Active Cart
 *     - Each node holds one cart item (veg_id + quantity).
 *     - "Doubly" means each node has BOTH a next AND a prev pointer.
 *     - WHY DLL? So we can traverse forwards (display) AND remove any
 *       node instantly without re-traversing from the head. Removes are
 *       O(1) once you have the pointer to the node. A singly linked list
 *       would need O(n) traversal to find the previous node for deletion.
 *
 *  2. STANDARD QUEUE (FIFO) - for Order Processing
 *     - Once an order is confirmed and paid, it is "enqueued."
 *     - Orders are dequeued in the exact order they arrived (First In,
 *       First Out), like a billing counter at a grocery store.
 *     - WHY QUEUE? Ensures fairness — the first customer to place an
 *       order gets their groceries dispatched first.
 *
 *  3. CIRCULAR LINKED LIST (CLL) - for Delivery Boy Allocation
 *     - All delivery boys are stored in a circular list where the LAST
 *       node's next pointer points back to the FIRST node.
 *     - WHY CIRCULAR? Round-robin allocation is naturally circular:
 *       after the last delivery boy is assigned, the next assignment
 *       wraps back to the first one — perfectly modeled by a circle.
 *
 *  4. MIN-HEAP (Priority Queue) - for Admin Order View
 *     - Orders are stored in a Min-Heap sorted by delivery slot priority
 *       (Morning=1 is highest priority → dispatched first).
 *     - WHY MIN-HEAP? The admin needs to see the most urgent orders first
 *       so drivers can be dispatched in the right sequence. A heap gives
 *       O(log n) insertions and always yields the min (most urgent) next.
 *
 * Team: CodeCrafters | Project: Fresh Picks | SDP-1
 * =================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "models.h"   /* Global struct definitions — the source of truth */

/* ──────────────────────────────────────────────────────────────
   FILE PATH CONSTANTS
   ────────────────────────────────────────────────────────────── */
#define VEGETABLES_FILE    "vegetables.txt"
#define ORDERS_FILE        "orders.txt"
#define DELIVERY_FILE      "delivery_boys.txt"
#define FREE_INV_FILE      "free_inventory.txt"
#define CART_DIR           "carts/"    /* Each user has carts/<user_id>_cart.txt */

/* Max number of items in orders / delivery boys arrays */
#define MAX_ORDERS         200
#define MAX_DELIVERY_BOYS  20
#define MAX_CART_ITEMS     50


/* ══════════════════════════════════════════════════════════════
   SECTION 1: STRUCT DEFINITIONS FOR DATA STRUCTURES
   ══════════════════════════════════════════════════════════════ */

/*
 * STRUCT: Vegetable
 * Represents one product row from vegetables.txt
 * Schema: veg_id|category|name|stock_g|price_per_1000g|tag|validity_days
 */
typedef struct {
    char veg_id[MAX_ID_LEN];           /* e.g. "V1001" */
    char category[MAX_STR_LEN];        /* e.g. "Allium" */
    char name[MAX_STR_LEN];            /* e.g. "Onion" */
    int  stock_g;                      /* Stock in grams, e.g. 50000 */
    float price_per_1000g;             /* Price per 1kg, e.g. 40.0 */
    char tag[MAX_STR_LEN];             /* Badge text, e.g. "Farm Fresh" */
    int  validity_days;                /* Shelf life, e.g. 7 */
} Vegetable;

/*
 * STRUCT: FreeItem
 * Represents one row from free_inventory.txt
 * Schema: vf_id|name|stock_g|min_trigger_amt|free_qty_g
 */
typedef struct {
    char vf_id[MAX_ID_LEN];            /* e.g. "VF101" */
    char name[MAX_STR_LEN];            /* e.g. "Curry Leaves" */
    int  stock_g;                      /* Current free stock in grams */
    float min_trigger_amt;             /* Cart total needed to trigger this freebie */
    int  free_qty_g;                   /* How many grams to give for free */
} FreeItem;

/*
 * STRUCT: DeliveryBoy
 * Represents one row from delivery_boys.txt
 * Schema: boy_id|name|phone|vehicle_no|is_active|last_assigned
 */
typedef struct {
    char boy_id[MAX_ID_LEN];
    char name[MAX_STR_LEN];
    char phone[MAX_STR_LEN];
    char vehicle_no[MAX_STR_LEN];
    int  is_active;                    /* 1 = available, 0 = off-duty */
    int  last_assigned;               /* 1 = this was the most recently assigned boy */
} DeliveryBoy;

/*
 * STRUCT: Order
 * Represents one row from orders.txt
 * Schema: order_id|user_id|total_amount|delivery_slot|delivery_boy_id|status|items_string
 */
typedef struct {
    char order_id[MAX_ID_LEN];
    char user_id[MAX_ID_LEN];
    float total_amount;
    char delivery_slot[MAX_STR_LEN];  /* "Morning", "Afternoon", or "Evening" */
    char delivery_boy_id[MAX_ID_LEN];
    char status[MAX_STR_LEN];         /* "PAID", "DISPATCHED", "DELIVERED" */
    char items_string[MAX_LINE_LEN];  /* e.g. "V1001:500,V1003:1000" */
    int  slot_priority;               /* 1=Morning, 2=Afternoon, 3=Evening — used for heap */
} Order;


/* ══════════════════════════════════════════════════════════════
   SECTION 2: DATA STRUCTURE 1 — DOUBLY LINKED LIST (Cart)
   ══════════════════════════════════════════════════════════════ */

/*
 * NODE: CartNode
 * One node in the Doubly Linked List representing one item in the cart.
 *
 * WHAT IS A DLL NODE?
 * Think of a DLL as a chain of wagons in a train.
 * Each wagon (node) holds CARGO (item data) and has a hook at the
 * FRONT (prev pointer) AND a hook at the BACK (next pointer).
 * This lets you move BOTH forward AND backward through the train.
 */
typedef struct CartNode {
    char veg_id[MAX_ID_LEN];      /* Which vegetable? e.g. "V1001" */
    char name[MAX_STR_LEN];       /* Display name e.g. "Onion" */
    int  qty_g;                   /* How many grams in cart */
    float price_per_1000g;        /* Store price so we can compute subtotal */
    float item_total;             /* qty_g / 1000.0 * price_per_1000g */
    int  is_free;                 /* 1 = freebie item (₹0), 0 = normal */

    struct CartNode *prev;        /* Pointer to the PREVIOUS node (or NULL if head) */
    struct CartNode *next;        /* Pointer to the NEXT node (or NULL if tail)     */
} CartNode;

/*
 * FUNCTION: dll_create_node
 * PURPOSE:  Allocate a new CartNode on the heap (dynamic memory).
 *           "The heap" is memory your program requests at runtime using malloc().
 * RETURNS:  Pointer to the new node, or NULL if malloc fails.
 */
CartNode* dll_create_node(const char* veg_id, const char* name,
                          int qty_g, float price_per_1000g, int is_free) {
    /* malloc = "memory allocate" — reserves space for one CartNode struct */
    CartNode* node = (CartNode*)malloc(sizeof(CartNode));
    if (!node) return NULL;  /* malloc can return NULL if system is out of memory */

    /* Copy data into the node */
    strncpy(node->veg_id, veg_id, MAX_ID_LEN - 1);
    strncpy(node->name,   name,   MAX_STR_LEN - 1);
    node->qty_g          = qty_g;
    node->price_per_1000g= price_per_1000g;
    node->item_total     = (qty_g / 1000.0f) * price_per_1000g;
    node->is_free        = is_free;

    /* New node starts with no neighbors */
    node->prev = NULL;
    node->next = NULL;
    return node;
}

/*
 * FUNCTION: dll_append
 * PURPOSE:  Add a new CartNode at the END (tail) of the DLL.
 *           "Append" always adds to the tail for O(1) if we track tail.
 *           Here we walk to the end for simplicity (cart is small, <= 50 items).
 * PARAMS:
 *   head** -> pointer to a pointer to the head node (so we can update head)
 */
void dll_append(CartNode** head, CartNode* new_node) {
    if (*head == NULL) {
        /* List is empty — the new node IS the head */
        *head = new_node;
        return;
    }
    /* Walk to the last node */
    CartNode* curr = *head;
    while (curr->next != NULL) {
        curr = curr->next;
    }
    /* Link: current tail <-> new_node */
    curr->next     = new_node;   /* Old tail points forward to new node */
    new_node->prev = curr;       /* New node points backward to old tail */
}

/*
 * FUNCTION: dll_update_or_append
 * PURPOSE:  If an item already exists in the DLL (by veg_id), UPDATE its
 *           quantity instead of creating a duplicate node.
 *           If not found, append a new node.
 *           This matches "Add to Cart" behaviour: adding an existing item
 *           increases quantity rather than duplicating the card.
 */
void dll_update_or_append(CartNode** head, const char* veg_id, const char* name,
                           int qty_g, float price_per_1000g, int is_free) {
    CartNode* curr = *head;
    while (curr != NULL) {
        if (strcmp(curr->veg_id, veg_id) == 0) {
            /* Item already in cart — update quantity and recompute total */
            curr->qty_g       = qty_g;   /* Overwrite (not add) — matches "set quantity" UX */
            curr->item_total  = (qty_g / 1000.0f) * price_per_1000g;
            return;
        }
        curr = curr->next;
    }
    /* Not found — create and append a brand new node */
    CartNode* node = dll_create_node(veg_id, name, qty_g, price_per_1000g, is_free);
    dll_append(head, node);
}

/*
 * FUNCTION: dll_remove
 * PURPOSE:  Delete a specific node from the DLL by veg_id.
 *           This is the KEY ADVANTAGE of a DLL over a singly linked list:
 *           we can re-link the surrounding nodes without re-traversing.
 *
 * HOW IT WORKS (VIVA ANSWER):
 *   Before: [prev] <-> [target] <-> [next]
 *   After:  [prev] <-----------> [next]
 *   The target node is skipped and freed from memory.
 */
void dll_remove(CartNode** head, const char* veg_id) {
    CartNode* curr = *head;
    while (curr != NULL) {
        if (strcmp(curr->veg_id, veg_id) == 0) {
            /* Found the node to delete — re-link its neighbors */
            if (curr->prev) curr->prev->next = curr->next;  /* Skip curr going forward  */
            if (curr->next) curr->next->prev = curr->prev;  /* Skip curr going backward */
            if (curr == *head) *head = curr->next;          /* If head is deleted, move head */
            free(curr);   /* Release memory back to the OS — very important! */
            return;
        }
        curr = curr->next;
    }
}

/*
 * FUNCTION: dll_get_total
 * PURPOSE:  Walk the entire DLL and sum up all item_totals.
 *           This is the cart grand total used for the ₹100 minimum check.
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
 * PURPOSE:  Free ALL nodes in the DLL to prevent memory leaks.
 *           Always call this at the end of a function that uses a DLL!
 *           A "memory leak" is when you allocate with malloc() but forget
 *           to free() — the memory stays occupied until the program ends.
 */
void dll_free_all(CartNode* head) {
    CartNode* curr = head;
    while (curr != NULL) {
        CartNode* next = curr->next;
        free(curr);
        curr = next;
    }
}


/* ══════════════════════════════════════════════════════════════
   SECTION 3: DATA STRUCTURE 2 — STANDARD QUEUE (Order Processing)
   ══════════════════════════════════════════════════════════════ */

/*
 * NODE: QueueNode
 * One item in the FIFO order processing queue.
 * Each QueueNode holds one complete Order struct.
 *
 * WHAT IS A QUEUE?
 * Think of a bakery. The first customer (front) is served first.
 * New customers join at the back (rear). First In, First Out = FIFO.
 */
typedef struct QueueNode {
    Order order;
    struct QueueNode* next;   /* Points to the node BEHIND this one in the queue */
} QueueNode;

/*
 * STRUCT: OrderQueue
 * Holds the front and rear pointers of our FIFO queue.
 * Tracking BOTH front and rear lets us enqueue in O(1) (add to rear)
 * and dequeue in O(1) (remove from front).
 */
typedef struct {
    QueueNode* front;   /* The node to be processed NEXT (oldest) */
    QueueNode* rear;    /* Where the NEWEST order was added        */
    int size;
} OrderQueue;

/*
 * FUNCTION: queue_init
 * PURPOSE:  Initialize an empty OrderQueue.
 *           Must call this before using the queue!
 */
void queue_init(OrderQueue* q) {
    q->front = NULL;
    q->rear  = NULL;
    q->size  = 0;
}

/*
 * FUNCTION: queue_enqueue
 * PURPOSE:  Add a new Order to the BACK (rear) of the queue.
 *           This is called right after payment is confirmed.
 *
 * VIVA EXPLANATION: "Enqueue" = join the back of the line.
 */
void queue_enqueue(OrderQueue* q, Order o) {
    QueueNode* node = (QueueNode*)malloc(sizeof(QueueNode));
    if (!node) return;   /* Safety check */
    node->order = o;
    node->next  = NULL;

    if (q->rear == NULL) {
        /* Queue was empty — this is the only node (both front and rear) */
        q->front = node;
        q->rear  = node;
    } else {
        /* Link: old rear -> new node; new node becomes the rear */
        q->rear->next = node;
        q->rear       = node;
    }
    q->size++;
}

/*
 * FUNCTION: queue_dequeue
 * PURPOSE:  Remove and return the FRONT (oldest) Order from the queue.
 *           Returns 1 on success, 0 if queue is empty.
 *
 * VIVA EXPLANATION: "Dequeue" = the first person in line is served.
 */
int queue_dequeue(OrderQueue* q, Order* out) {
    if (q->front == NULL) return 0;  /* Queue is empty */

    QueueNode* temp = q->front;
    *out = temp->order;              /* Copy order data to caller */
    q->front = temp->next;           /* Move front pointer forward */
    if (q->front == NULL) q->rear = NULL; /* If queue is now empty, reset rear */
    free(temp);
    q->size--;
    return 1;
}

/*
 * FUNCTION: queue_free
 * PURPOSE:  Free all remaining nodes in the queue.
 */
void queue_free(OrderQueue* q) {
    Order dummy;
    while (queue_dequeue(q, &dummy));  /* Keep dequeuing until empty */
}


/* ══════════════════════════════════════════════════════════════
   SECTION 4: DATA STRUCTURE 3 — CIRCULAR LINKED LIST (Delivery)
   ══════════════════════════════════════════════════════════════ */

/*
 * NODE: DeliveryNode
 * One node in the Circular Linked List of delivery boys.
 *
 * WHAT IS A CIRCULAR LINKED LIST?
 * Like a regular singly linked list, but the LAST node's next
 * pointer does NOT point to NULL. Instead, it loops back to the
 * FIRST node, forming a perfect circle.
 *
 * WHY IS THIS PERFECT FOR ROUND-ROBIN?
 * Round-robin means "take turns, then repeat." After the last delivery
 * boy is assigned, the pointer naturally wraps to the first one.
 * A circle models this wrapping with zero extra code!
 */
typedef struct DeliveryNode {
    DeliveryBoy boy;
    struct DeliveryNode* next;   /* Points to next boy, or back to HEAD if last */
} DeliveryNode;

/*
 * FUNCTION: cll_build
 * PURPOSE:  Read delivery_boys.txt and build a Circular Linked List.
 *           Returns a pointer to the HEAD of the CLL.
 *           Returns NULL if no active delivery boys are found.
 */
DeliveryNode* cll_build() {
    FILE* fp = fopen(DELIVERY_FILE, "r");
    if (!fp) return NULL;

    DeliveryNode* head = NULL;   /* First node in the circle */
    DeliveryNode* tail = NULL;   /* Last node — its next will point back to head */

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';   /* Remove newline character */
        if (strlen(line) == 0) continue;    /* Skip blank lines */

        /* Parse pipe-delimited line: boy_id|name|phone|vehicle_no|is_active|last_assigned */
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
        b.is_active      = atoi(tok);                  tok = strtok(NULL, "|");
        b.last_assigned  = tok ? atoi(tok) : 0;

        /* Only include ACTIVE delivery boys (is_active == 1) */
        if (!b.is_active) continue;

        DeliveryNode* node = (DeliveryNode*)malloc(sizeof(DeliveryNode));
        if (!node) continue;
        node->boy  = b;
        node->next = NULL;

        if (head == NULL) {
            /* First node: head and tail are the same */
            head = node;
            tail = node;
            node->next = head;  /* Point back to itself — already a mini-circle! */
        } else {
            /* Append to end: tail -> new node -> head (keep the circle) */
            tail->next = node;
            node->next = head;  /* New tail's next ALWAYS points back to head */
            tail       = node;
        }
    }
    fclose(fp);
    return head;
}

/*
 * FUNCTION: cll_assign_delivery
 * PURPOSE:  Use Round-Robin to pick the NEXT delivery boy.
 *
 * ALGORITHM (explain this in the VIVA!):
 *   1. Traverse the CLL to find the boy where last_assigned == 1.
 *      That's who got the PREVIOUS order.
 *   2. Move to that boy's ->next node.
 *   3. That next boy gets THIS order.
 *   4. Update: previous boy's last_assigned = 0, new boy's = 1.
 *   5. Save updates back to delivery_boys.txt.
 *
 * PARAMS:
 *   head     -> Head of the CLL
 *   out_boy  -> Pointer to a DeliveryBoy struct to fill with the assigned boy
 * RETURNS: 1 on success, 0 if no delivery boys available.
 */
int cll_assign_delivery(DeliveryNode* head, DeliveryBoy* out_boy) {
    if (!head) return 0;

    DeliveryNode* curr    = head;
    DeliveryNode* chosen  = NULL;
    DeliveryNode* prev_boy= NULL;

    /* Safety: count nodes to avoid infinite loop (CLL has no NULL terminator) */
    int count = 0;
    DeliveryNode* walker = head;
    do {
        count++;
        walker = walker->next;
    } while (walker != head);

    /* Find the boy who was last assigned (last_assigned == 1) */
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (curr->boy.last_assigned == 1) {
            prev_boy = curr;             /* Remember who had the last order */
            chosen   = curr->next;       /* The NEXT boy in the circle gets this order */
            found    = 1;
            break;
        }
        curr = curr->next;
    }

    if (!found) {
        /* No one has last_assigned=1 yet (first ever order) — assign to head */
        chosen = head;
    }

    /* Update in-memory flags */
    if (prev_boy) prev_boy->boy.last_assigned = 0;
    chosen->boy.last_assigned = 1;
    *out_boy = chosen->boy;    /* Copy the chosen boy's data to output */

    /* ── Write updated delivery_boys.txt ── */
    FILE* fp = fopen(DELIVERY_FILE, "w");
    if (!fp) return 0;

    /* Walk the CLL exactly 'count' times to rewrite every boy */
    DeliveryNode* w = head;
    for (int i = 0; i < count; i++) {
        fprintf(fp, "%s|%s|%s|%s|%d|%d\n",
            w->boy.boy_id,
            w->boy.name,
            w->boy.phone,
            w->boy.vehicle_no,
            w->boy.is_active,
            w->boy.last_assigned
        );
        w = w->next;
    }
    fclose(fp);
    return 1;
}

/*
 * FUNCTION: cll_free
 * PURPOSE:  Free all nodes in the CLL.
 *           IMPORTANT: We must break the circle first, or we'll loop forever!
 */
void cll_free(DeliveryNode* head) {
    if (!head) return;

    /* Count nodes first */
    int count = 0;
    DeliveryNode* curr = head;
    do { count++; curr = curr->next; } while (curr != head);

    /* Now free each node (walk exactly count times) */
    curr = head;
    for (int i = 0; i < count; i++) {
        DeliveryNode* next = curr->next;
        free(curr);
        curr = next;
    }
}


/* ══════════════════════════════════════════════════════════════
   SECTION 5: DATA STRUCTURE 4 — MIN-HEAP (Admin Priority Queue)
   ══════════════════════════════════════════════════════════════ */

/*
 * WHAT IS A MIN-HEAP?
 * A Min-Heap is a COMPLETE BINARY TREE where the PARENT is always
 * SMALLER than (or equal to) its CHILDREN.
 * This means the ROOT (index 0) always holds the SMALLEST value.
 *
 * WHY AN ARRAY FOR THE HEAP?
 * A complete binary tree can be perfectly represented in an array:
 *   - Root is at index 0
 *   - Left child of node i  is at index (2*i + 1)
 *   - Right child of node i is at index (2*i + 2)
 *   - Parent of node i      is at index ((i-1) / 2)
 *
 * WHY MIN-HEAP FOR ORDERS?
 * We want the admin to see the MOST URGENT (lowest priority number) order first:
 *   Priority 1 = Morning  (dispatch first!)
 *   Priority 2 = Afternoon
 *   Priority 3 = Evening
 * The Min-Heap always keeps the Priority-1 order at the top (root).
 */
typedef struct {
    Order data[MAX_ORDERS];   /* The heap stored as an array */
    int   size;               /* How many orders are currently in the heap */
} MinHeap;

/*
 * FUNCTION: heap_swap
 * PURPOSE:  Swap two Order elements in the heap array.
 *           Used by heapify_up and heapify_down.
 */
void heap_swap(MinHeap* h, int i, int j) {
    Order temp   = h->data[i];
    h->data[i]   = h->data[j];
    h->data[j]   = temp;
}

/*
 * FUNCTION: heap_heapify_up  (also called "Bubble Up" or "Sift Up")
 * PURPOSE:  After inserting a new element at the END of the array,
 *           move it UP the tree until the heap property is restored.
 *
 * VIVA EXPLANATION:
 *   Imagine inserting a "Priority 1 (Morning)" order at the bottom.
 *   It bubbles UP by swapping with its parent until it reaches the root.
 */
void heap_heapify_up(MinHeap* h, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        /* If current node has LOWER priority number than parent, swap them */
        if (h->data[idx].slot_priority < h->data[parent].slot_priority) {
            heap_swap(h, idx, parent);
            idx = parent;   /* Move up to where we just swapped */
        } else {
            break;  /* Heap property satisfied — stop */
        }
    }
}

/*
 * FUNCTION: heap_insert
 * PURPOSE:  Add a new Order to the Min-Heap.
 *           1. Place it at the end of the array.
 *           2. Bubble it up to its correct position.
 *           Time complexity: O(log n) — very fast!
 */
void heap_insert(MinHeap* h, Order o) {
    if (h->size >= MAX_ORDERS) return;  /* Heap is full */
    h->data[h->size] = o;               /* Place at end */
    heap_heapify_up(h, h->size);        /* Restore heap order */
    h->size++;
}

/*
 * FUNCTION: heap_heapify_down  (also called "Sift Down")
 * PURPOSE:  After removing the root (min element), the last element
 *           is moved to the root. Then it "sinks down" to its correct place.
 */
void heap_heapify_down(MinHeap* h, int idx) {
    while (1) {
        int left  = 2 * idx + 1;
        int right = 2 * idx + 2;
        int smallest = idx;  /* Assume current is smallest */

        /* Check if left child is smaller than current smallest */
        if (left < h->size && h->data[left].slot_priority < h->data[smallest].slot_priority)
            smallest = left;

        /* Check if right child is smaller than current smallest */
        if (right < h->size && h->data[right].slot_priority < h->data[smallest].slot_priority)
            smallest = right;

        if (smallest != idx) {
            heap_swap(h, idx, smallest);
            idx = smallest;   /* Sink down further */
        } else {
            break;  /* Heap property satisfied */
        }
    }
}

/*
 * FUNCTION: heap_extract_min
 * PURPOSE:  Remove and return the ORDER with the LOWEST slot_priority.
 *           (i.e., the most urgent Morning order gets dispatched first)
 *           Returns 1 on success, 0 if heap is empty.
 *
 * ALGORITHM:
 *   1. Save the root (minimum = most urgent).
 *   2. Move the LAST element to the root position.
 *   3. Decrease heap size.
 *   4. Sift down the new root to restore heap property.
 */
int heap_extract_min(MinHeap* h, Order* out) {
    if (h->size == 0) return 0;
    *out = h->data[0];                    /* Root has minimum priority number */
    h->data[0] = h->data[h->size - 1];   /* Move last element to root */
    h->size--;
    if (h->size > 0) heap_heapify_down(h, 0);
    return 1;
}


/* ══════════════════════════════════════════════════════════════
   SECTION 6: HELPER FUNCTIONS
   ══════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: get_slot_priority
 * PURPOSE:  Convert a delivery slot name to an integer priority for the heap.
 *           Morning is most urgent (Priority 1), Evening is least (Priority 3).
 */
int get_slot_priority(const char* slot) {
    if (strcmp(slot, "Morning") == 0)   return 1;
    if (strcmp(slot, "Afternoon") == 0) return 2;
    return 3;  /* Evening or anything else */
}

/*
 * FUNCTION: generate_order_id
 * PURPOSE:  Generate a new unique Order ID by reading orders.txt
 *           and finding the current highest ID, then incrementing it.
 *
 * SCHEME: ORD101, ORD102, ORD103, ...
 */
void generate_order_id(char* out_id) {
    FILE* fp = fopen(ORDERS_FILE, "r");
    int max_num = 100;  /* IDs start at ORD101 */

    if (fp) {
        char line[MAX_LINE_LEN];
        while (fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\n")] = '\0';
            if (strlen(line) == 0) continue;
            /* Parse order_id (first field) */
            char oid[MAX_ID_LEN];
            strncpy(oid, line, MAX_ID_LEN - 1);
            char* pipe = strchr(oid, '|');
            if (pipe) *pipe = '\0';
            /* Extract the numeric part after "ORD" */
            if (strncmp(oid, "ORD", 3) == 0) {
                int num = atoi(oid + 3);
                if (num > max_num) max_num = num;
            }
        }
        fclose(fp);
    }
    /* New ID is one higher than the current maximum */
    snprintf(out_id, MAX_ID_LEN, "ORD%d", max_num + 1);
}

/*
 * FUNCTION: get_cart_filename
 * PURPOSE:  Build the path to a user's cart file.
 *           Each user gets their own file: carts/U1001_cart.txt
 */
void get_cart_filename(const char* user_id, char* out_path) {
    snprintf(out_path, MAX_LINE_LEN, "%s%s_cart.txt", CART_DIR, user_id);
}

/*
 * FUNCTION: load_cart_from_file
 * PURPOSE:  Read a user's cart .txt file and build a DLL from it.
 *           Cart file format: veg_id|name|qty_g|price_per_1000g|is_free (one item per line)
 *
 * RETURNS: Head of the newly built DLL (or NULL if cart is empty/missing).
 */
CartNode* load_cart_from_file(const char* user_id) {
    char path[MAX_LINE_LEN];
    get_cart_filename(user_id, path);

    FILE* fp = fopen(path, "r");
    if (!fp) return NULL;   /* No cart file yet = empty cart */

    CartNode* head = NULL;
    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        /* Parse: veg_id|name|qty_g|price_per_1000g|is_free */
        char veg_id[MAX_ID_LEN], name[MAX_STR_LEN];
        int  qty_g, is_free;
        float price;

        char* tok = strtok(line, "|");
        if (!tok) continue;
        strncpy(veg_id, tok, MAX_ID_LEN - 1);     tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(name, tok, MAX_STR_LEN - 1);       tok = strtok(NULL, "|");
        if (!tok) continue;
        qty_g = atoi(tok);                          tok = strtok(NULL, "|");
        if (!tok) continue;
        price = atof(tok);                          tok = strtok(NULL, "|");
        is_free = tok ? atoi(tok) : 0;

        /* Append to DLL */
        CartNode* node = dll_create_node(veg_id, name, qty_g, price, is_free);
        dll_append(&head, node);
    }
    fclose(fp);
    return head;
}

/*
 * FUNCTION: save_cart_to_file
 * PURPOSE:  Traverse the DLL and write each node back to the cart file.
 *           This OVERWRITES the entire file each time — simple and safe.
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
 * FUNCTION: find_vegetable
 * PURPOSE:  Search vegetables.txt for a specific veg_id.
 *           Returns 1 if found (and fills the Vegetable struct), 0 if not found.
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
        strncpy(v.veg_id, tok, MAX_ID_LEN - 1);      tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(v.category, tok, MAX_STR_LEN - 1);   tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(v.name, tok, MAX_STR_LEN - 1);        tok = strtok(NULL, "|");
        if (!tok) continue;
        v.stock_g = atoi(tok);                         tok = strtok(NULL, "|");
        if (!tok) continue;
        v.price_per_1000g = atof(tok);                 tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(v.tag, tok, MAX_STR_LEN - 1);         tok = strtok(NULL, "|");
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
 *           Returns 1 on success, 0 if insufficient stock.
 *           This is the CRITICAL Stock Recheck step before payment.
 */
int deduct_vegetable_stock(const char* veg_id, int qty_g) {
    /* Read ALL vegetables into a temporary array */
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
        strncpy(v.veg_id,   tok, MAX_ID_LEN  - 1); tok = strtok(NULL, "|");
        strncpy(v.category, tok, MAX_STR_LEN - 1); tok = strtok(NULL, "|");
        strncpy(v.name,     tok, MAX_STR_LEN - 1); tok = strtok(NULL, "|");
        v.stock_g = atoi(tok);                       tok = strtok(NULL, "|");
        v.price_per_1000g = atof(tok);               tok = strtok(NULL, "|");
        strncpy(v.tag,      tok, MAX_STR_LEN - 1); tok = strtok(NULL, "|");
        v.validity_days = tok ? atoi(tok) : 0;

        vegs[count++] = v;
    }
    fclose(fp);

    /* Find the target vegetable and check/deduct stock */
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(vegs[i].veg_id, veg_id) == 0) {
            if (vegs[i].stock_g < qty_g) return 0;  /* Insufficient stock! */
            vegs[i].stock_g -= qty_g;               /* Deduct from stock */
            found = 1;
            break;
        }
    }
    if (!found) return 0;

    /* Rewrite the ENTIRE vegetables.txt with updated stock */
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
    return 1;  /* Success */
}

/*
 * FUNCTION: check_and_apply_freebies
 * PURPOSE:  If cart total >= 500, check free_inventory.txt.
 *           For EACH freebie where total >= min_trigger_amt AND stock > 0:
 *             1. Append a free CartNode (is_free=1, price=0) to the DLL.
 *             2. Deduct free_qty_g from free_inventory.txt.
 *
 * WHY ₹500 and not ₹100?
 *   The ₹100 minimum is for checkout eligibility (no delivery below ₹100).
 *   The ₹500 threshold is for PROMOTIONAL freebies — a marketing incentive
 *   to encourage larger orders (e.g., "Order ₹500+ and get Curry Leaves free!").
 */
void check_and_apply_freebies(CartNode** head, float cart_total) {
    if (cart_total < 500.0f) return;  /* Freebie threshold not met */

    FILE* fp = fopen(FREE_INV_FILE, "r");
    if (!fp) return;

    FreeItem items[20];
    int count = 0;

    /* Read all free inventory items */
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

    /* Check each freebie: does cart meet the trigger amount AND is there stock? */
    for (int i = 0; i < count; i++) {
        if (cart_total >= items[i].min_trigger_amt && items[i].stock_g >= items[i].free_qty_g) {
            /* Add free item to the Cart DLL with price=0 and is_free=1 */
            dll_update_or_append(head,
                items[i].vf_id,
                items[i].name,
                items[i].free_qty_g,
                0.0f,    /* FREE — zero price */
                1        /* is_free = true */
            );
            /* Deduct from free inventory stock */
            items[i].stock_g -= items[i].free_qty_g;
        }
    }

    /* Rewrite free_inventory.txt with updated stock levels */
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


/* ══════════════════════════════════════════════════════════════
   SECTION 7: COMMAND HANDLER FUNCTIONS
   Each function below handles one command dispatched by main().
   ══════════════════════════════════════════════════════════════ */

/*
 * COMMAND: list_products
 * PURPOSE: Read vegetables.txt and print every vegetable in pipe-delimited format.
 *          Flask parses this and returns a JSON array to the frontend.
 *
 * OUTPUT FORMAT (one vegetable per line):
 *   veg_id|category|name|stock_g|price_per_1000g|tag|validity_days
 */
void cmd_list_products() {
    FILE* fp = fopen(VEGETABLES_FILE, "r");
    if (!fp) {
        PRINT_ERROR("Could not open vegetables.txt");
        return;
    }

    /* First line of output tells Flask how many records follow */
    printf("SUCCESS|");

    char line[MAX_LINE_LEN];
    int first = 1;
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
 * PURPOSE: Add (or update) one vegetable in the user's cart DLL, then save.
 *          argv: add_to_cart <user_id> <veg_id> <qty_grams>
 *
 * STEPS:
 *   1. Validate inputs
 *   2. Look up vegetable details from vegetables.txt (to get name + price)
 *   3. Load user's current cart DLL from file
 *   4. Update or append the item in the DLL
 *   5. Save the updated DLL back to file
 */
void cmd_add_to_cart(const char* user_id, const char* veg_id, int qty_g) {
    if (qty_g <= 0) { PRINT_ERROR("Quantity must be positive"); return; }
    if (qty_g % 50 != 0) { PRINT_ERROR("Quantity must be a multiple of 50g"); return; }

    /* Step 2: Look up vegetable */
    Vegetable v;
    if (!find_vegetable(veg_id, &v)) {
        PRINT_ERROR("Vegetable not found");
        return;
    }
    /* Check if requested qty exceeds current stock */
    if (qty_g > v.stock_g) {
        PRINT_ERROR("Insufficient stock");
        return;
    }

    /* Step 3: Load the DLL from cart file */
    CartNode* head = load_cart_from_file(user_id);

    /* Step 4: Update existing item or add new node */
    dll_update_or_append(&head, veg_id, v.name, qty_g, v.price_per_1000g, 0);

    /* Step 5: Persist DLL to file */
    save_cart_to_file(user_id, head);

    /* Clean up DLL memory */
    dll_free_all(head);

    PRINT_SUCCESS("Item added to cart");
}

/*
 * COMMAND: view_cart
 * PURPOSE: Load the user's cart DLL and print all items.
 *          Flask returns this as a JSON array of cart items.
 *          argv: view_cart <user_id>
 *
 * OUTPUT FORMAT:
 *   SUCCESS|<total>
 *   veg_id|name|qty_g|price_per_1000g|item_total|is_free  (one per line)
 */
void cmd_view_cart(const char* user_id) {
    CartNode* head = load_cart_from_file(user_id);
    float total    = dll_get_total(head);

    /* First line: status + grand total */
    printf("SUCCESS|%.2f\n", total);

    /* Subsequent lines: each cart item */
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
    dll_remove(&head, veg_id);  /* Remove node — DLL re-links neighbors automatically */
    save_cart_to_file(user_id, head);
    dll_free_all(head);
    PRINT_SUCCESS("Item removed from cart");
}

/*
 * COMMAND: checkout
 * PURPOSE: The most complex command — the full payment pipeline.
 *          argv: checkout <user_id> <delivery_slot>
 *
 * PIPELINE (explain each step in the VIVA!):
 *   Step 1: Load cart DLL from file
 *   Step 2: Minimum order check (₹100)
 *   Step 3: STOCK RECHECK — verify current stock for every cart item
 *           (prevents race conditions if another user bought the last item)
 *   Step 4: Apply freebies if total >= ₹500 (modifies the DLL)
 *   Step 5: Deduct stock from vegetables.txt for all cart items
 *   Step 6: Assign a delivery boy using the CLL round-robin algorithm
 *   Step 7: Build order record and enqueue it (FIFO queue)
 *   Step 8: Write the order to orders.txt
 *   Step 9: Delete the user's cart file (cart is now empty)
 *   Step 10: Print the confirmed order details for Flask
 */
void cmd_checkout(const char* user_id, const char* slot) {
    /* Step 1: Load cart */
    CartNode* head = load_cart_from_file(user_id);
    if (!head) {
        PRINT_ERROR("Cart is empty");
        return;
    }

    /* Step 2: Minimum order check */
    float total = dll_get_total(head);
    if (total < 100.0f) {
        dll_free_all(head);
        PRINT_ERROR("Minimum order is Rs.100");
        return;
    }

    /* Step 3: STOCK RECHECK — critical for preventing race conditions.
     * WHY? Between the user adding items and clicking "Pay Now", another
     * customer might have bought the same vegetable, depleting the stock.
     * We MUST re-verify stock at payment time, not just at "Add to Cart" time.
     *
     * We check BEFORE deducting to avoid a partial deduction on failure.
     */
    CartNode* curr = head;
    while (curr != NULL) {
        if (curr->is_free) { curr = curr->next; continue; }  /* Skip freebies */
        Vegetable v;
        if (!find_vegetable(curr->veg_id, &v)) {
            dll_free_all(head);
            PRINT_ERROR("Product no longer available");
            return;
        }
        if (v.stock_g < curr->qty_g) {
            dll_free_all(head);
            /* Format helpful message with the out-of-stock item's name */
            char err_msg[MAX_STR_LEN + 50];
            snprintf(err_msg, sizeof(err_msg),
                "Insufficient stock for %s (available: %dg)", v.name, v.stock_g);
            PRINT_ERROR(err_msg);
            return;
        }
        curr = curr->next;
    }

    /* Step 4: Apply freebies (modifies DLL if total >= ₹500) */
    check_and_apply_freebies(&head, total);
    /* Recalculate total AFTER freebies (freebies are ₹0, so total shouldn't change) */
    total = dll_get_total(head);

    /* Step 5: Deduct stock for all paid items */
    curr = head;
    while (curr != NULL) {
        if (!curr->is_free) {
            deduct_vegetable_stock(curr->veg_id, curr->qty_g);
        }
        curr = curr->next;
    }

    /* Step 6: Round-robin delivery boy assignment using CLL */
    DeliveryNode* cll_head = cll_build();
    DeliveryBoy assigned_boy;
    char boy_id[MAX_ID_LEN]   = "NONE";
    char boy_name[MAX_STR_LEN] = "Unassigned";
    char boy_phone[MAX_STR_LEN]= "N/A";

    if (cll_assign_delivery(cll_head, &assigned_boy)) {
        strncpy(boy_id,    assigned_boy.boy_id, MAX_ID_LEN  - 1);
        strncpy(boy_name,  assigned_boy.name,   MAX_STR_LEN - 1);
        strncpy(boy_phone, assigned_boy.phone,  MAX_STR_LEN - 1);
    }
    cll_free(cll_head);

    /* Step 7: Generate Order ID and build the order record */
    char order_id[MAX_ID_LEN];
    generate_order_id(order_id);

    /* Build the items_string: "V1001:500,V1003:1000,VF101:50" */
    char items_string[MAX_LINE_LEN] = "";
    curr = head;
    while (curr != NULL) {
        char part[64];
        snprintf(part, sizeof(part), "%s:%d", curr->veg_id, curr->qty_g);
        if (strlen(items_string) > 0) strncat(items_string, ",", MAX_LINE_LEN - strlen(items_string) - 1);
        strncat(items_string, part, MAX_LINE_LEN - strlen(items_string) - 1);
        curr = curr->next;
    }

    /* Build the Order struct */
    Order o;
    strncpy(o.order_id,        order_id,  MAX_ID_LEN  - 1);
    strncpy(o.user_id,         user_id,   MAX_ID_LEN  - 1);
    o.total_amount = total;
    strncpy(o.delivery_slot,   slot,      MAX_STR_LEN - 1);
    strncpy(o.delivery_boy_id, boy_id,    MAX_ID_LEN  - 1);
    strncpy(o.status,          "PAID",    MAX_STR_LEN - 1);
    strncpy(o.items_string,    items_string, MAX_LINE_LEN - 1);
    o.slot_priority = get_slot_priority(slot);

    /* Step 7b: Enqueue the order into the FIFO queue for processing.
     * WHY QUEUE? To simulate a real order management system where orders
     * are processed in the sequence they arrive. */
    OrderQueue q;
    queue_init(&q);
    queue_enqueue(&q, o);
    /* In a real system, a background worker would dequeue and process these.
     * For our demo, we immediately write the order to the file. */
    queue_free(&q);

    /* Step 8: Append the confirmed order to orders.txt */
    FILE* fp = fopen(ORDERS_FILE, "a");  /* "a" = append mode — never overwrites */
    if (!fp) {
        dll_free_all(head);
        PRINT_ERROR("Could not save order");
        return;
    }
    fprintf(fp, "%s|%s|%.2f|%s|%s|%s|%s\n",
        o.order_id, o.user_id, o.total_amount,
        o.delivery_slot, o.delivery_boy_id, o.status, o.items_string
    );
    fclose(fp);

    /* Step 9: Clear the user's cart (delete cart file) */
    char cart_path[MAX_LINE_LEN];
    get_cart_filename(user_id, cart_path);
    remove(cart_path);   /* C standard library: delete a file */

    /* Step 10: Print confirmed order for Flask to return to the browser */
    printf("SUCCESS|%s|%.2f|%s|%s|%s|%s\n",
        order_id, total, slot, boy_name, boy_phone, items_string
    );

    dll_free_all(head);
}

/*
 * COMMAND: get_orders
 * PURPOSE: Get all past orders for a specific user from orders.txt.
 *          argv: get_orders <user_id>
 */
void cmd_get_orders(const char* user_id) {
    FILE* fp = fopen(ORDERS_FILE, "r");
    if (!fp) {
        PRINT_ERROR("Could not open orders file");
        return;
    }

    printf("SUCCESS|\n");  /* Header line */

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        char orig_line[MAX_LINE_LEN];
        strncpy(orig_line, line, MAX_LINE_LEN - 1);
        orig_line[strcspn(orig_line, "\n")] = '\0';

        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        /* Check if this order belongs to our user */
        char* tok = strtok(line, "|");
        if (!tok) continue;
        tok = strtok(NULL, "|");  /* user_id is field 2 */
        if (!tok) continue;

        if (strcmp(tok, user_id) == 0) {
            printf("%s\n", orig_line);
        }
    }
    fclose(fp);
}

/*
 * COMMAND: admin_orders
 * PURPOSE: Load ALL orders from orders.txt into a Min-Heap sorted by
 *          delivery slot priority, then print them in sorted order.
 *          This gives the admin a dispatch-priority view.
 *
 * OUTPUT FORMAT:
 *   SUCCESS|<count>
 *   order_id|user_id|total|slot|boy_id|status|items   (sorted by priority)
 */
void cmd_admin_orders() {
    FILE* fp = fopen(ORDERS_FILE, "r");
    if (!fp) {
        PRINT_ERROR("Could not open orders file");
        return;
    }

    /* Build the Min-Heap from all orders in the file */
    MinHeap heap;
    heap.size = 0;

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        Order o;
        char* tok = strtok(line, "|");
        if (!tok) continue;
        strncpy(o.order_id, tok, MAX_ID_LEN - 1);          tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(o.user_id, tok, MAX_ID_LEN - 1);            tok = strtok(NULL, "|");
        if (!tok) continue;
        o.total_amount = atof(tok);                          tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(o.delivery_slot, tok, MAX_STR_LEN - 1);     tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(o.delivery_boy_id, tok, MAX_ID_LEN - 1);    tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(o.status, tok, MAX_STR_LEN - 1);             tok = strtok(NULL, "|");
        if (tok) strncpy(o.items_string, tok, MAX_LINE_LEN - 1);
        else o.items_string[0] = '\0';

        o.slot_priority = get_slot_priority(o.delivery_slot);

        /* Insert into Min-Heap — O(log n) per insertion */
        heap_insert(&heap, o);
    }
    fclose(fp);

    /* Extract orders from heap in priority order (Morning first) */
    printf("SUCCESS|%d\n", heap.size);
    Order out;
    while (heap_extract_min(&heap, &out)) {
        printf("%s|%s|%.2f|%s|%s|%s|%s\n",
            out.order_id, out.user_id, out.total_amount,
            out.delivery_slot, out.delivery_boy_id, out.status, out.items_string
        );
    }
}


/* ══════════════════════════════════════════════════════════════
   SECTION 8: MAIN — Command Dispatcher
   ══════════════════════════════════════════════════════════════ */

/*
 * main()
 * PURPOSE: Reads argv[1] (the command name) and calls the matching
 *          handler function. This is the entry point Flask calls via:
 *            subprocess.run(["./order", "list_products"], ...)
 *
 * argc = argument count (includes the program name itself)
 * argv = argument values as strings
 *   argv[0] = "./order"
 *   argv[1] = command (e.g., "checkout")
 *   argv[2]+ = command arguments
 */
int main(int argc, char* argv[]) {
    /* We always need at least the command name */
    if (argc < 2) {
        PRINT_ERROR("No command provided. Usage: ./order <command> [args]");
        return 1;
    }

    const char* cmd = argv[1];  /* e.g., "list_products" */

    /* ── Dispatch: route command string to the right handler function ── */

    if (strcmp(cmd, "list_products") == 0) {
        cmd_list_products();

    } else if (strcmp(cmd, "add_to_cart") == 0) {
        /* Requires: user_id, veg_id, qty_grams */
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

    } else {
        /* Unknown command */
        char err[MAX_STR_LEN];
        snprintf(err, sizeof(err), "Unknown command: %s", cmd);
        PRINT_ERROR(err);
        return 1;
    }

    return 0;
}
