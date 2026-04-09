/*
 * ds_utils.c - Fresh Picks: Data Structure Utility Library
 * =========================================================
 * This file contains the IMPLEMENTATIONS of all four data structures
 * used across the Fresh Picks backend:
 *
 *   1. DOUBLY LINKED LIST (DLL)    — for the shopping cart
 *   2. STANDARD QUEUE (FIFO)       — for order processing
 *   3. CIRCULAR LINKED LIST (CLL)  — for delivery boy allocation
 *   4. MIN-HEAP (Priority Queue)   — for admin order dispatch view
 *
 * WHY A SEPARATE FILE?
 *   Both order.c and any future .c files might need these structures.
 *   Putting them here avoids copy-pasting the same code into every file.
 *   It also means if we fix a bug in dll_remove(), it's fixed everywhere.
 *
 * HOW TO USE:
 *   1. #include "models.h" at the top of your .c file
 *      (models.h has the struct definitions AND the function prototypes)
 *   2. Compile this file alongside your own:
 *      gcc order.c ds_utils.c -o order
 *
 * Team: CodeCrafters | Project: Fresh Picks | SDP-1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "models.h"   /* All struct definitions + function prototypes live here */


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
DeliveryNode* cll_build(void) {
    FILE* fp = fopen(DELIVERY_FILE, "r");
    if (!fp) return NULL;

    DeliveryNode* head = NULL;  /* First node — the "top" of the circle */
    DeliveryNode* tail = NULL;  /* Last node — its next will point to head */

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        /* Parse: boy_id|name|phone|vehicle_no|is_active|last_assigned */
        DeliveryBoy boy;
        char* tok = strtok(line, "|");
        if (!tok) continue;
        strncpy(boy.boy_id,     tok, MAX_ID_LEN  - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(boy.name,       tok, MAX_STR_LEN - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(boy.phone,      tok, MAX_STR_LEN - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        strncpy(boy.vehicle_no, tok, MAX_STR_LEN - 1);  tok = strtok(NULL, "|");
        if (!tok) continue;
        boy.is_active     = atoi(tok);                   tok = strtok(NULL, "|");
        boy.last_assigned = tok ? atoi(tok) : 0;

        /* Only add ACTIVE delivery boys to the CLL */
        if (!boy.is_active) continue;

        /* Create a new node */
        DeliveryNode* node = (DeliveryNode*)malloc(sizeof(DeliveryNode));
        if (!node) continue;
        node->boy  = boy;
        node->next = NULL;

        if (head == NULL) {
            /* First node — it is both head and tail */
            head       = node;
            tail       = node;
            node->next = head;  /* Circle of one: points to itself */
        } else {
            /* Append to tail, then close the circle */
            tail->next = node;  /* Old tail → new node */
            tail       = node;  /* New tail is now this node */
            tail->next = head;  /* New tail → head (closes the circle) */
        }
    }
    fclose(fp);
    return head;
}

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
int cll_assign_delivery(DeliveryNode* head, DeliveryBoy* out_boy) {
    if (!head) return 0;  /* No delivery boys in the system */

    /* ── Step 1: Count nodes to prevent an infinite loop ─────────
       A CLL has no NULL — we'd loop forever without a counter. */
    int count = 0;
    DeliveryNode* walker = head;
    do {
        count++;
        walker = walker->next;
    } while (walker != head);

    /* ── Step 2: Find the boy who was last assigned ────────────── */
    DeliveryNode* curr     = head;
    DeliveryNode* chosen   = NULL;  /* The boy who will get THIS order */
    DeliveryNode* prev_boy = NULL;  /* The boy who got the LAST order  */
    int found = 0;

    for (int i = 0; i < count; i++) {
        if (curr->boy.last_assigned == 1) {
            prev_boy = curr;         /* Remember who had the flag */
            chosen   = curr->next;   /* Round-robin: next boy in circle */
            found    = 1;
            break;
        }
        curr = curr->next;
    }

    if (!found) {
        /* No one has the flag yet — this is the very first order.
           Assign to the head (first active boy in the list). */
        chosen = head;
    }

    /* ── Step 3 & 4: Update the last_assigned flags ─────────────── */
    if (prev_boy) prev_boy->boy.last_assigned = 0;  /* Clear old flag */
    chosen->boy.last_assigned = 1;                  /* Set new flag   */
    *out_boy = chosen->boy;    /* Return the assigned boy's details    */

    /* ── Step 5: Rewrite delivery_boys.txt with updated flags ────── */
    FILE* fp = fopen(DELIVERY_FILE, "w");
    if (!fp) return 0;

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
