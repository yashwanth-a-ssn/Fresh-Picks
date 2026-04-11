/**
 * receipt.js — Fresh Picks: Centralized PDF Receipt Generator
 * ============================================================
 * Loaded by both admin_orders.html and user_orders.html via:
 *   <script src="/static/js/receipt.js"></script>
 *
 * PUBLIC API:
 *   generateStandardReceiptPDF(orderData)  → Promise<void>
 *
 * WHY THIS APPROACH (programmatic HTML, not DOM clone)?
 *   html2canvas rasterises the live DOM by spawning an off-screen
 *   iframe and re-painting it. Several mechanisms break this process:
 *
 *   1. CSS Custom Properties (var(--accent) etc.)
 *      html2canvas's iframe inherits NONE of the parent document's
 *      :root CSS variables. Every var(--x) call resolves to the
 *      *initial value* (empty string / 0), producing transparent
 *      backgrounds and invisible text — the "blank PDF" symptom.
 *
 *   2. Dark themes & transparent backgrounds
 *      When html2canvas's background is not explicitly set, it defaults
 *      to rgba(0,0,0,0) — transparent. The browser then composites this
 *      over the PDF page's white, creating the "black PDF" symptom.
 *      The iframe also has no access to `prefers-color-scheme` or the
 *      parent's <body> background, so even `background:inherit` fails.
 *
 *   3. Deep DOM trees & Bootstrap utility classes
 *      Bootstrap classes like `d-flex`, `gap-2`, `text-muted` rely on
 *      the Bootstrap stylesheet. The cloned node INSIDE html2canvas's
 *      iframe has no stylesheet at all. Layout collapses to block-level
 *      stacking, fragmenting content across pages.
 *
 *   4. Cross-origin images and CORS
 *      Product images or CDN fonts will cause a SecurityError unless
 *      every resource has correct CORS headers AND useCORS:true is set.
 *
 *   THE FIX: We never touch the live DOM.
 *   We programmatically BUILD a self-contained <div> string using
 *   ONLY strict inline hex styles. No CSS variables. No Bootstrap.
 *   No external images. One flat, predictable, A4-width div → clean PDF
 *   every single time.
 *
 * FILENAME CONVENTION:
 *   <orderId>_<userId>_<date>.pdf
 *   e.g. ORD107_U1003_2025-04-08.pdf
 *
 * DEPENDENCY: html2pdf.js must be loaded before this file.
 *   <script src="https://cdnjs.cloudflare.com/ajax/libs/html2pdf.js/0.10.1/html2pdf.bundle.min.js"></script>
 *   <script src="/static/js/receipt.js"></script>
 *
 * Team: CodeCrafters | Project: Fresh Picks | SDP-1
 */

/* ──────────────────────────────────────────────────────────────────────
   SECTION 1: COLOUR PALETTE
   All values are strict hex codes. Absolutely no CSS variables.
   ────────────────────────────────────────────────────────────────────── */
const RECEIPT_COLORS = {
    bg:          "#1e1e1e",   /* VS Code dark background                */
    bgElevated:  "#252526",   /* Card / elevated surface                 */
    bgSection:   "#2d2d2d",   /* Section separator rows                  */
    border:      "#3e3e42",   /* Subtle border lines                     */
    accent:      "#007acc",   /* Primary blue (FreshPicks brand)         */
    accentLight: "#1a9eff",   /* Lighter blue for totals                 */
    success:     "#28a745",   /* Green — delivered / free items          */
    warning:     "#ffc107",   /* Amber — out for delivery                */
    danger:      "#dc3545",   /* Red — cancelled                         */
    text:        "#d4d4d4",   /* Primary text                            */
    textBright:  "#ffffff",   /* Headings / labels                       */
    textMuted:   "#858585",   /* Secondary / meta text                   */
    mono:        "Consolas, 'Courier New', monospace",
    sans:        "'Segoe UI', Arial, system-ui, sans-serif"
};

/* ──────────────────────────────────────────────────────────────────────
   SECTION 2: ITEM STRING PARSER
   Handles both 3-part (legacy) and 4-part (current) formats.

   3-part: veg_id:qty_g:price_per_1000g
   4-part: veg_id:name:qty_g:price_per_1000g
   ────────────────────────────────────────────────────────────────────── */
function _parseItems(itemsString) {
    if (!itemsString) return [];
    return itemsString.split(",").filter(Boolean).map(entry => {
        const parts = entry.trim().split(":");
        let id, name, qty, price;
        if (parts.length >= 4) {
            [id, name, qty, price] = parts;
        } else if (parts.length === 3) {
            [id, qty, price] = parts;
            name = id;
        } else {
            return null;
        }
        const qtyNum   = parseInt(qty) || 0;
        const priceNum = parseFloat(price) || 0;
        const isFree   = priceNum === 0;
        const qtyLabel = qtyNum >= 1000
            ? `${(qtyNum / 1000).toFixed(2)} kg`
            : `${qtyNum} g`;
        const amount   = isFree ? 0 : (qtyNum / 1000) * priceNum;
        return { id, name: name || id, qty: qtyLabel, price: priceNum, amount, isFree };
    }).filter(Boolean);
}

/* ──────────────────────────────────────────────────────────────────────
   SECTION 3: STATUS → DISPLAY LABEL + HEX COLOR
   ────────────────────────────────────────────────────────────────────── */
function _statusStyle(status) {
    const map = {
        "Order Placed":     { bg: "#1a3a5c", color: "#9cdcfe", label: "Order Placed"     },
        "Out for Delivery": { bg: "#3a2e00", color: "#ffc107", label: "Out for Delivery" },
        "Delivered":        { bg: "#1a3a1a", color: "#28a745", label: "Delivered"         },
        "Cancelled":        { bg: "#3a1a1a", color: "#dc3545", label: "Cancelled"         },
    };
    return map[status] || map["Order Placed"];
}

function _slotStyle(slot) {
    const map = {
        "Morning":   { bg: "#1a3a1a", color: "#90ee90" },
        "Afternoon": { bg: "#3a2e00", color: "#ffd700" },
        "Evening":   { bg: "#1a1a4a", color: "#9cdcfe" },
    };
    return map[slot] || map["Evening"];
}

/* ──────────────────────────────────────────────────────────────────────
   SECTION 4: PROGRAMMATIC HTML BUILDER
   Constructs a complete, self-contained receipt <div>.
   EVERY style is a strict inline hex code.
   ────────────────────────────────────────────────────────────────────── */
function _buildReceiptHTML(orderData) {
    const C   = RECEIPT_COLORS;
    const ss  = _statusStyle(orderData.status || "Order Placed");
    const sl  = _slotStyle(orderData.deliverySlot || "Morning");
    const items = _parseItems(orderData.itemsString || "");

    /* ── Computed totals ── */
    const subtotal = items.reduce((acc, it) => acc + it.amount, 0);
    const total    = parseFloat(orderData.total || subtotal);

    /* ── Invoice table rows ── */
    const tableRows = items.map((it, idx) => `
        <tr style="background:${idx % 2 === 0 ? C.bgElevated : C.bg};">
            <td style="padding:8px 10px; color:${C.text}; font-size:12px;">
                ${it.name}
                ${it.isFree
                    ? `<span style="color:${C.success}; font-size:10px; margin-left:6px;">FREE</span>`
                    : ""}
            </td>
            <td style="padding:8px 10px; text-align:right; color:${C.textMuted}; font-size:12px;">
                ${it.isFree ? "—" : `₹${it.price.toFixed(2)}/kg`}
            </td>
            <td style="padding:8px 10px; text-align:right; color:${C.text}; font-size:12px;">
                ${it.qty}
            </td>
            <td style="padding:8px 10px; text-align:right;
                       color:${it.isFree ? C.success : C.textBright}; font-size:12px;
                       font-weight:${it.isFree ? "700" : "400"};">
                ${it.isFree ? "₹0.00 (FREE)" : `₹${it.amount.toFixed(2)}`}
            </td>
        </tr>
    `).join("");

    const dateStr = orderData.date || new Date().toISOString().slice(0, 10);

    return `
<div style="
    background-color:${C.bg};
    color:${C.text};
    font-family:${C.sans};
    font-size:13px;
    line-height:1.55;
    padding:24px 28px;
    box-sizing:border-box;
    width:794px;
">

    <!-- ══ HEADER ══ -->
    <div style="
        text-align:center;
        margin-bottom:20px;
        padding-bottom:16px;
        border-bottom:2px solid ${C.accent};
    ">
        <div style="font-size:26px; margin-bottom:6px;">🥦</div>
        <h1 style="
            color:${C.accent};
            font-size:20px;
            font-weight:700;
            margin:0 0 4px;
            font-family:${C.sans};
        ">FreshPicks — Official Receipt</h1>
        <p style="color:${C.textMuted}; font-size:11px; margin:0;">
            123 Grocery Ave · Chennai – 600045 · support@freshpicks.in
        </p>
        <p style="color:${C.textMuted}; font-size:11px; margin:4px 0 0;">
            CodeCrafters | SDP-1 | Generated: ${dateStr}
        </p>
    </div>

    <!-- ══ ORDER META ROW ══ -->
    <div style="
        display:flex;
        justify-content:space-between;
        flex-wrap:wrap;
        gap:12px;
        margin-bottom:20px;
        padding:14px 16px;
        background:${C.bgElevated};
        border-radius:6px;
        border:1px solid ${C.border};
    ">
        <!-- Order ID -->
        <div>
            <div style="font-size:10px; color:${C.textMuted}; text-transform:uppercase; letter-spacing:0.06em; margin-bottom:3px;">Order ID</div>
            <div style="font-family:${C.mono}; font-size:14px; color:${C.accentLight}; font-weight:700;">
                ${orderData.orderId || "—"}
            </div>
        </div>
        <!-- Customer -->
        <div>
            <div style="font-size:10px; color:${C.textMuted}; text-transform:uppercase; letter-spacing:0.06em; margin-bottom:3px;">Customer</div>
            <div style="font-size:13px; color:${C.textBright}; font-weight:500;">
                ${orderData.userId || "—"}
            </div>
        </div>
        <!-- Delivery Slot -->
        <div>
            <div style="font-size:10px; color:${C.textMuted}; text-transform:uppercase; letter-spacing:0.06em; margin-bottom:3px;">Delivery Slot</div>
            <div style="
                display:inline-block;
                background:${sl.bg};
                color:${sl.color};
                border-radius:12px;
                padding:2px 10px;
                font-size:12px;
                font-weight:600;
            ">${orderData.deliverySlot || "—"}</div>
        </div>
        <!-- Status -->
        <div>
            <div style="font-size:10px; color:${C.textMuted}; text-transform:uppercase; letter-spacing:0.06em; margin-bottom:3px;">Status</div>
            <div style="
                display:inline-block;
                background:${ss.bg};
                color:${ss.color};
                border-radius:12px;
                padding:2px 10px;
                font-size:12px;
                font-weight:600;
            ">${ss.label}</div>
        </div>
        <!-- Timestamp -->
        <div>
            <div style="font-size:10px; color:${C.textMuted}; text-transform:uppercase; letter-spacing:0.06em; margin-bottom:3px;">Placed At</div>
            <div style="font-size:12px; color:${C.textMuted}; font-family:${C.mono};">
                ${orderData.timestamp || "—"}
            </div>
        </div>
    </div>

    <!-- ══ DELIVERY AGENT ══ -->
    ${orderData.boyName ? `
    <div style="
        margin-bottom:20px;
        padding:12px 16px;
        background:${C.bgSection};
        border-radius:6px;
        border-left:3px solid ${C.accent};
    ">
        <div style="font-size:10px; color:${C.textMuted}; text-transform:uppercase; letter-spacing:0.06em; margin-bottom:4px;">Delivery Agent</div>
        <div style="font-size:13px; color:${C.textBright}; font-weight:500;">
            ${orderData.boyName}
            ${orderData.boyPhone
                ? `<span style="color:${C.textMuted}; font-size:11px; margin-left:8px;">${orderData.boyPhone}</span>`
                : ""}
        </div>
    </div>
    ` : ""}

    <!-- ══ INVOICE TABLE ══ -->
    <div style="margin-bottom:20px;">
        <div style="
            font-size:10px;
            color:${C.textMuted};
            text-transform:uppercase;
            letter-spacing:0.06em;
            margin-bottom:8px;
        ">Items Ordered</div>

        <table style="
            width:100%;
            border-collapse:collapse;
            border:1px solid ${C.border};
            border-radius:6px;
            overflow:hidden;
        ">
            <!-- Table header -->
            <thead>
                <tr style="background:${C.accent};">
                    <th style="padding:10px; text-align:left; font-size:11px; color:#ffffff; font-weight:600; width:40%;">PRODUCT</th>
                    <th style="padding:10px; text-align:right; font-size:11px; color:#ffffff; font-weight:600; width:20%;">RATE</th>
                    <th style="padding:10px; text-align:right; font-size:11px; color:#ffffff; font-weight:600; width:15%;">QTY</th>
                    <th style="padding:10px; text-align:right; font-size:11px; color:#ffffff; font-weight:600; width:25%;">AMOUNT</th>
                </tr>
            </thead>
            <tbody>
                ${tableRows || `
                <tr>
                    <td colspan="4" style="padding:12px; text-align:center; color:${C.textMuted}; font-size:12px;">—</td>
                </tr>`}
            </tbody>
        </table>
    </div>

    <!-- ══ TOTALS ══ -->
    <div style="
        display:flex;
        justify-content:flex-end;
        margin-bottom:24px;
    ">
        <div style="
            min-width:240px;
            border:1px solid ${C.border};
            border-radius:6px;
            overflow:hidden;
        ">
            <div style="
                display:flex;
                justify-content:space-between;
                padding:8px 14px;
                background:${C.bgElevated};
                border-bottom:1px solid ${C.border};
            ">
                <span style="color:${C.textMuted}; font-size:12px;">Subtotal</span>
                <span style="color:${C.text}; font-size:12px;">₹${subtotal.toFixed(2)}</span>
            </div>
            <div style="
                display:flex;
                justify-content:space-between;
                padding:8px 14px;
                background:${C.bgElevated};
                border-bottom:1px solid ${C.border};
            ">
                <span style="color:${C.textMuted}; font-size:12px;">Delivery</span>
                <span style="color:${C.success}; font-size:12px; font-weight:600;">FREE</span>
            </div>
            <div style="
                display:flex;
                justify-content:space-between;
                padding:10px 14px;
                background:${C.bgSection};
            ">
                <span style="color:${C.textBright}; font-size:14px; font-weight:700;">GRAND TOTAL</span>
                <span style="color:${C.accentLight}; font-size:16px; font-weight:700;">₹${total.toFixed(2)}</span>
            </div>
        </div>
    </div>

    <!-- ══ FOOTER ══ -->
    <div style="
        text-align:center;
        padding-top:14px;
        border-top:1px dashed ${C.border};
        color:${C.textMuted};
        font-size:10px;
        line-height:1.7;
    ">
        <div>Thank you for shopping with <span style="color:${C.accent}; font-weight:600;">FreshPicks</span>!</div>
        <div>For support, contact us at support@freshpicks.in</div>
        <div style="margin-top:6px; font-family:${C.mono}; font-size:9px; color:#555555;">
            Receipt ID: ${orderData.orderId} · Generated by FreshPicks Admin Panel · CodeCrafters SDP-1
        </div>
    </div>

</div>
    `.trim();
}


/* ──────────────────────────────────────────────────────────────────────
   SECTION 5: PUBLIC API
   generateStandardReceiptPDF(orderData)

   orderData shape:
   {
     orderId:      "ORD107",
     userId:       "U1003",
     date:         "2025-04-08",           // YYYY-MM-DD slice of timestamp
     timestamp:    "2025-04-08 14:30:00",
     status:       "Order Placed",
     deliverySlot: "Morning",
     total:        149.50,
     itemsString:  "V1001:Tomato:500:40.00,VF101:CurryLeaves:50:0.00",
     boyName:      "Ravi Kumar",           // optional
     boyPhone:     "9876543210"            // optional
   }

   Usage example (admin_orders.html):
     const { orderId, userId, timestamp, status, delivery_slot,
             total_amount, items_string, boy_name, boy_phone } = order;
     generateStandardReceiptPDF({
         orderId, userId,
         date: timestamp?.slice(0, 10),
         timestamp, status,
         deliverySlot: delivery_slot,
         total: total_amount,
         itemsString: items_string,
         boyName: boy_name,
         boyPhone: boy_phone
     });
   ────────────────────────────────────────────────────────────────────── */
async function generateStandardReceiptPDF(orderData) {

    if (!orderData || !orderData.orderId) {
        console.error("[receipt.js] generateStandardReceiptPDF: missing orderData.orderId");
        alert("Cannot generate PDF — order data is incomplete.");
        return;
    }

    /* ── Build filename: ORDERID_USERID_DATE.pdf ── */
    const dateStr  = (orderData.date || new Date().toISOString().slice(0, 10)).replace(/\s+/g, "");
    const userId   = (orderData.userId || "UNKNOWN").replace(/\s+/g, "_");
    const filename = `${orderData.orderId}_${userId}_${dateStr}.pdf`;

    /* ── Build the receipt HTML string ── */
    const htmlString = _buildReceiptHTML(orderData);

    /* ── Create an off-screen container ── */
    const stage = document.createElement("div");
    stage.style.cssText = [
        "position:fixed",
        "top:-99999px",
        "left:0",
        "width:794px",
        "background:#1e1e1e",
        "overflow:visible",
        "z-index:-9999",
        "pointer-events:none"
    ].join(";");

    stage.innerHTML = htmlString;
    document.body.appendChild(stage);

    const receiptEl = stage.firstElementChild;

    /* ── Flush layout (two animation frames) ── */
    await new Promise(resolve =>
        requestAnimationFrame(() => requestAnimationFrame(resolve))
    );

    /* ── html2pdf configuration ── */
    const opt = {
        margin:   10,
        filename,
        image:    { type: "jpeg", quality: 0.98 },
        html2canvas: {
            scale:           2,
            useCORS:         true,
            backgroundColor: "#1e1e1e",
            width:           794,
            windowWidth:     794,
            scrollX:         0,
            scrollY:         0,
            logging:         false
        },
        jsPDF: {
            unit:        "mm",
            format:      "a4",
            orientation: "portrait"
        }
    };

    try {
        await html2pdf().set(opt).from(receiptEl).save();
    } catch (err) {
        console.error("[receipt.js] PDF generation failed:", err);
        alert("PDF generation failed. Open the browser console (F12) for details.");
    } finally {
        /* Always clean up — even if pdf generation threw */
        if (stage.parentNode) {
            stage.parentNode.removeChild(stage);
        }
    }
}

/* Export for ES module usage (optional — works without it too) */
if (typeof module !== "undefined" && module.exports) {
    module.exports = { generateStandardReceiptPDF };
}
