/**
 * ui-helpers.js — Fresh Picks: Category Grouping + Min-Order Bar
 * ================================================================
 * Include this script in shop.html and admin_inventory.html.
 *
 * EXPORTS:
 *   initCategoryGroups(containerSelector)
 *   initMinOrderBar(fillSelector, msgSelector, currentTotal, minTotal = 100)
 *
 * Team: CodeCrafters | SDP-1
 */


/* ──────────────────────────────────────────────────────────────────────
   CATEGORY GROUPING
   ──────────────────────────────────────────────────────────────────────
   Usage: call after you have rendered product rows.

   Your product HTML must carry data-category="Vegetables" (or whatever).
   initCategoryGroups() will:
     1. Group those rows by category into .category-group divs.
     2. Render a clickable .category-header above each group.
     3. Default: first category is expanded, rest are collapsed.

   If you render via server-side Jinja, call from DOMContentLoaded.
   If you render via JS (fetch + inject), call after injection.

   EXAMPLE (Jinja shop.html product card template):
     <div class="product-card" data-category="{{ product.category }}">...</div>

   EXAMPLE (JS dynamic render):
     // After injecting cards:
     initCategoryGroups("#productsGrid");
*/
function initCategoryGroups(containerSelector) {
    const container = document.querySelector(containerSelector);
    if (!container) return;

    /* ── Step 1: Collect all product cards and their categories ── */
    const cards = Array.from(container.querySelectorAll("[data-category]"));
    if (cards.length === 0) return;

    /* ── Step 2: Build ordered map: category → [card, ...] ── */
    const catMap = new Map();
    cards.forEach(card => {
        const cat = card.getAttribute("data-category") || "Other";
        if (!catMap.has(cat)) catMap.set(cat, []);
        catMap.get(cat).push(card);
    });

    /* ── Step 3: Remove all cards from DOM temporarily ── */
    cards.forEach(c => c.remove());

    /* ── Step 4: Re-insert wrapped in .category-group divs ── */
    let firstGroup = true;
    catMap.forEach((groupCards, catName) => {
        const group = document.createElement("div");
        group.className = "category-group" + (firstGroup ? " expanded" : "");

        /* Header */
        const header = document.createElement("div");
        header.className = "category-header";
        header.innerHTML = `
            <span>
                <span class="cat-title">${catName}</span>
                <span class="cat-count">(${groupCards.length})</span>
            </span>
            <i class="bi bi-chevron-down cat-chevron"></i>
        `;

        /* Body */
        const body = document.createElement("div");
        body.className = "category-body";
        groupCards.forEach(c => body.appendChild(c));

        group.appendChild(header);
        group.appendChild(body);
        container.appendChild(group);

        /* Set initial max-height for expanded group */
        if (firstGroup) {
            /* Use setTimeout to let the browser paint first */
            setTimeout(() => {
                body.style.maxHeight = body.scrollHeight + "px";
            }, 0);
        }

        /* ── Toggle on header click ── */
        header.addEventListener("click", () => {
            const isExpanded = group.classList.contains("expanded");

            if (isExpanded) {
                /* Collapse: set to current height first, then animate to 0 */
                body.style.maxHeight = body.scrollHeight + "px";
                requestAnimationFrame(() => {
                    requestAnimationFrame(() => {
                        body.style.maxHeight = "0";
                    });
                });
                group.classList.remove("expanded");
            } else {
                /* Expand */
                body.style.maxHeight = body.scrollHeight + "px";
                group.classList.add("expanded");

                /* After transition, allow dynamic content to resize freely */
                body.addEventListener("transitionend", function handler() {
                    if (group.classList.contains("expanded")) {
                        body.style.maxHeight = body.scrollHeight + "px";
                    }
                    body.removeEventListener("transitionend", handler);
                });
            }
        });

        firstGroup = false;
    });
}


/* ──────────────────────────────────────────────────────────────────────
   MIN-ORDER BAR
   ──────────────────────────────────────────────────────────────────────
   IMPORTANT: The bar is NOT sticky. It flows naturally in the cart
   container, below the navbar. Apply position:static (the default).

   Usage:
     <!-- In your cart.html (not inside navbar): -->
     <div class="min-order-bar-wrapper">
       <div class="min-order-bar">
         <div class="bar-label">
           Min. Order ₹100
           <span class="bar-amount" id="barAmount">₹0.00</span>
         </div>
         <div class="bar-track">
           <div class="bar-fill" id="barFill" style="width:0%;"></div>
         </div>
         <div class="bar-msg" id="barMsg">Add ₹100.00 more to place an order.</div>
       </div>
     </div>

     Then call:
       updateMinOrderBar(currentTotal);

   Or to initialise once:
       initMinOrderBar("#barFill", "#barMsg", "#barAmount", currentTotal);
*/
function updateMinOrderBar(currentTotal, minTotal = 100) {
    const fillEl   = document.getElementById("barFill");
    const msgEl    = document.getElementById("barMsg");
    const amountEl = document.getElementById("barAmount");

    if (!fillEl) return;

    const pct     = Math.min((currentTotal / minTotal) * 100, 100);
    const reached = currentTotal >= minTotal;

    fillEl.style.width = pct + "%";

    if (reached) {
        fillEl.classList.add("reached");
        if (msgEl) {
            msgEl.textContent = "✔ Minimum order reached! You can place your order.";
            msgEl.classList.add("reached");
        }
    } else {
        fillEl.classList.remove("reached");
        const remaining = (minTotal - currentTotal).toFixed(2);
        if (msgEl) {
            msgEl.textContent = `Add ₹${remaining} more to place an order.`;
            msgEl.classList.remove("reached");
        }
    }

    if (amountEl) {
        amountEl.textContent = `₹${parseFloat(currentTotal).toFixed(2)}`;
    }
}

/* Alias for explicit initialisation call pattern */
function initMinOrderBar(fillSelector, msgSelector, amountSelector, currentTotal, minTotal = 100) {
    updateMinOrderBar(currentTotal, minTotal);
}
