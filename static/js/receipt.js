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
 *
 *   3. Deep DOM trees & Bootstrap utility classes
 *      Bootstrap classes like `d-flex`, `gap-2`, `text-muted` rely on
 *      the Bootstrap stylesheet. The cloned node INSIDE html2canvas's
 *      iframe has no stylesheet at all. Layout collapses to block-level
 *      stacking, fragmenting content across pages.
 *
 *   4. display:flex inside html2canvas
 *      html2canvas has historically poor support for complex flex
 *      nesting. The totals row (justify-content:flex-end) causes
 *      amounts to vanish or misalign. Fixed here by using <table>
 *      for all structured layouts — html2canvas renders tables reliably.
 *
 *   THE FIX: We never touch the live DOM.
 *   We programmatically BUILD a self-contained <div> string using
 *   ONLY strict inline hex styles, and use <table> for all layouts.
 *   No CSS variables. No Bootstrap. No external images. One flat,
 *   predictable, A4-width div → clean PDF every single time.
 *
 * FILENAME CONVENTION:
 *   <orderId>_<userId>_<date>.pdf
 *   e.g. ORD107_U1003_2025-04-08.pdf
 *
 * DEPENDENCY: html2pdf.js must be loaded BEFORE this file.
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
    bgHeader:    "#007acc",   /* Table header — primary blue             */
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

   3-part (legacy, stored before this fix):  veg_id:qty_g:price_per_1000g
   4-part (current, stored after this fix):  veg_id:name:qty_g:price_per_1000g
   ────────────────────────────────────────────────────────────────────── */
function _parseItems(itemsString) {
    if (!itemsString || itemsString.trim() === "") return [];
    return itemsString.split(",").filter(Boolean).map(entry => {
        const parts = entry.trim().split(":");
        let id, name, qty, price;

        if (parts.length >= 4) {
            /* 4-part: veg_id:name:qty_g:price */
            [id, name, qty, price] = parts;
        } else if (parts.length === 3) {
            /* 3-part legacy: veg_id:qty_g:price — name falls back to id */
            [id, qty, price] = parts;
            name = id;
        } else {
            return null;
        }

        const qtyNum   = parseInt(qty)   || 0;
        const priceNum = parseFloat(price) || 0;
        const isFree   = priceNum === 0;

        /* Display quantity: ≥1000g → kg, else grams */
        const qtyLabel = qtyNum >= 1000
            ? `${(qtyNum / 1000).toFixed(2)} kg`
            : `${qtyNum} g`;

        const amount = isFree ? 0 : (qtyNum / 1000) * priceNum;

        return {
            id,
            name:    name || id,
            qty:     qtyLabel,
            price:   priceNum,
            amount,
            isFree
        };
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
   ALL layouts use <table> — never display:flex — for html2canvas compat.
   ────────────────────────────────────────────────────────────────────── */
function _buildReceiptHTML(orderData) {
    const C   = RECEIPT_COLORS;
    const ss  = _statusStyle(orderData.status || "Order Placed");
    const sl  = _slotStyle(orderData.deliverySlot || "Morning");
    const items = _parseItems(orderData.itemsString || "");

    /* ── Computed totals ── */
    const subtotal = items.reduce((acc, it) => acc + it.amount, 0);
    const total    = parseFloat(orderData.total || subtotal);

    const dateStr = (orderData.date || new Date().toISOString().slice(0, 10));

    /* ── Customer display — prefer fullName over userId ── */
    const customerName = orderData.fullName || orderData.userId || "Customer";

    /* ── User contact block ── */
    const hasContact = orderData.userPhone || orderData.userEmail || orderData.userAddress;
    const contactBlock = hasContact ? `
        <tr>
            <td style="padding:7px 14px; font-size:10px; color:${C.textMuted};
                       text-transform:uppercase; letter-spacing:0.06em;
                       border-bottom:1px solid ${C.border};
                       background:${C.bgElevated}; width:30%; white-space:nowrap;">CONTACT</td>
            <td style="padding:7px 14px; border-bottom:1px solid ${C.border};
                       background:${C.bgElevated}; font-size:11px; color:${C.text};">
                ${orderData.userPhone  ? `&#128222; ${orderData.userPhone}<br>` : ""}
                ${orderData.userEmail  ? `&#9993; ${orderData.userEmail}<br>` : ""}
                ${orderData.userAddress ? `<span style="color:${C.textMuted};">${orderData.userAddress.replace(/,/g, ", ")}</span>` : ""}
            </td>
        </tr>` : "";

    /* ── Invoice table rows ── */
    const tableRows = items.length > 0
        ? items.map((it, idx) => `
        <tr style="background:${idx % 2 === 0 ? C.bgElevated : C.bg};">
            <td style="padding:8px 10px; color:${C.text}; font-size:12px;
                       border-bottom:1px solid ${C.border};">
                ${it.name}${it.isFree
                    ? `&nbsp;<span style="color:${C.success}; font-size:10px;
                                         font-weight:700;">FREE</span>`
                    : ""}
            </td>
            <td style="padding:8px 10px; text-align:right; font-size:12px;
                       color:${C.textMuted}; border-bottom:1px solid ${C.border};">
                ${it.isFree ? "&mdash;" : `&#8377;${it.price.toFixed(2)}/kg`}
            </td>
            <td style="padding:8px 10px; text-align:right; font-size:12px;
                       color:${C.text}; border-bottom:1px solid ${C.border};">
                ${it.qty}
            </td>
            <td style="padding:8px 10px; text-align:right; font-size:12px;
                       font-weight:${it.isFree ? "700" : "400"};
                       color:${it.isFree ? C.success : C.textBright};
                       border-bottom:1px solid ${C.border};">
                ${it.isFree ? "&#8377;0.00" : `&#8377;${it.amount.toFixed(2)}`}
            </td>
        </tr>`).join("")
        : `<tr><td colspan="4"
                  style="padding:14px; text-align:center;
                         color:${C.textMuted}; font-size:12px;"
              >&mdash; No items &mdash;</td></tr>`;

    /* ── Meta table: Order ID / Customer / Slot / Status / Timestamp ── */
    const metaRows = [
        ["ORDER ID",      `<span style="font-family:${C.mono}; font-size:14px;
                                        color:${C.accentLight}; font-weight:700;">
                              ${orderData.orderId || "&mdash;"}
                           </span>`],
        ["BILL TO",       `<span style="font-size:13px; color:${C.textBright};
                                        font-weight:600;">
                              ${customerName}
                           </span>`],
        ["DELIVERY SLOT", `<span style="background:${sl.bg}; color:${sl.color};
                                        border-radius:10px; padding:2px 10px;
                                        font-size:12px; font-weight:600;">
                              ${orderData.deliverySlot || "&mdash;"}
                           </span>`],
        ["STATUS",        `<span style="background:${ss.bg}; color:${ss.color};
                                        border-radius:10px; padding:2px 10px;
                                        font-size:12px; font-weight:600;">
                              ${ss.label}
                           </span>`],
        ["PLACED AT",     `<span style="font-size:12px; color:${C.textMuted};
                                        font-family:${C.mono};">
                              ${orderData.timestamp || "&mdash;"}
                           </span>`],
    ].map(([label, value]) => `
        <tr>
            <td style="padding:7px 14px; font-size:10px; color:${C.textMuted};
                       text-transform:uppercase; letter-spacing:0.06em;
                       white-space:nowrap; border-bottom:1px solid ${C.border};
                       background:${C.bgElevated}; width:30%;">${label}</td>
            <td style="padding:7px 14px; border-bottom:1px solid ${C.border};
                       background:${C.bgElevated};">${value}</td>
        </tr>`).join("") + contactBlock;

    /* ── Delivery agent block (optional) ── */
    const agentBlock = orderData.boyName ? `
    <div style="margin-bottom:20px; padding:12px 16px;
                background:${C.bgSection}; border-radius:6px;
                border-left:4px solid ${C.accent};">
        <div style="font-size:10px; color:${C.textMuted};
                    text-transform:uppercase; letter-spacing:0.06em;
                    margin-bottom:4px;">Delivery Agent</div>
        <div style="font-size:13px; color:${C.textBright}; font-weight:500;">
            ${orderData.boyName}
            ${orderData.boyPhone
                ? `<span style="color:${C.textMuted}; font-size:11px;
                                margin-left:10px;">${orderData.boyPhone}</span>`
                : ""}
        </div>
    </div>` : "";

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
    <div style="text-align:center; margin-bottom:20px; padding-bottom:16px;
                border-bottom:2px solid ${C.accent};">
        <div style="font-size:26px; margin-bottom:6px;">&#x1F96C;</div>
        <div style="color:${C.accent}; font-size:20px; font-weight:700;
                    margin:0 0 4px; font-family:${C.sans};">
            CodeCrafters Fresh Picks
        </div>
        <div style="color:${C.textMuted}; font-size:11px; margin:0;">
            123 Grocery Ave, Market City, Chennai &ndash; 600045
        </div>
        <div style="color:${C.textMuted}; font-size:11px; margin:2px 0 0;">
            GSTIN: 22AAAAA0000A1Z5 &nbsp;|&nbsp; FSSAI: 10020042004971
        </div>
        <div style="color:${C.textMuted}; font-size:11px; margin:2px 0 0;">
            +91 98765 43210 &nbsp;|&nbsp; support@freshpicks.in
        </div>
        <div style="margin-top:10px;">
            <span style="background:${C.bgElevated}; color:${C.accentLight};
                         border:1px solid ${C.border}; border-radius:4px;
                         padding:3px 12px; font-size:13px; font-weight:700;
                         font-family:${C.mono};">
                ORDER RECEIPT &nbsp;&mdash;&nbsp; ${orderData.orderId || ""}
            </span>
        </div>
        <div style="color:${C.textMuted}; font-size:11px; margin-top:6px;">
            ${dateStr}
        </div>
    </div>

    <!-- ══ ORDER META TABLE ══ -->
    <div style="margin-bottom:20px;">
        <table style="width:100%; border-collapse:collapse;
                      border:1px solid ${C.border}; border-radius:6px;
                      overflow:hidden;">
            ${metaRows}
        </table>
    </div>

    <!-- ══ DELIVERY AGENT ══ -->
    ${agentBlock}

    <!-- ══ INVOICE TABLE ══ -->
    <div style="margin-bottom:20px;">
        <div style="font-size:10px; color:${C.textMuted}; text-transform:uppercase;
                    letter-spacing:0.06em; margin-bottom:8px;">Items In This Order</div>
        <table style="width:100%; border-collapse:collapse;
                      border:1px solid ${C.border}; overflow:hidden;">
            <thead>
                <tr style="background:${C.bgHeader};">
                    <th style="padding:10px; text-align:left; font-size:11px;
                               color:#ffffff; font-weight:600; width:40%;
                               border-bottom:1px solid ${C.border};">PRODUCT</th>
                    <th style="padding:10px; text-align:right; font-size:11px;
                               color:#ffffff; font-weight:600; width:20%;
                               border-bottom:1px solid ${C.border};">RATE</th>
                    <th style="padding:10px; text-align:right; font-size:11px;
                               color:#ffffff; font-weight:600; width:15%;
                               border-bottom:1px solid ${C.border};">QTY</th>
                    <th style="padding:10px; text-align:right; font-size:11px;
                               color:#ffffff; font-weight:600; width:25%;
                               border-bottom:1px solid ${C.border};">AMOUNT</th>
                </tr>
            </thead>
            <tbody>
                ${tableRows}
            </tbody>
        </table>
    </div>

    <!-- ══ TOTALS — uses <table> not display:flex (html2canvas compat) ══ -->
    <div style="margin-bottom:24px; text-align:right;">
        <table style="margin-left:auto; border-collapse:collapse;
                      border:1px solid ${C.border}; min-width:260px;">
            <tr>
                <td style="padding:8px 16px; background:${C.bgElevated};
                           color:${C.textMuted}; font-size:12px;
                           border-bottom:1px solid ${C.border};">Subtotal</td>
                <td style="padding:8px 16px; background:${C.bgElevated};
                           color:${C.text}; font-size:12px; text-align:right;
                           border-bottom:1px solid ${C.border};">
                    &#8377;${subtotal.toFixed(2)}</td>
            </tr>
            <tr>
                <td style="padding:8px 16px; background:${C.bgElevated};
                           color:${C.textMuted}; font-size:12px;
                           border-bottom:1px solid ${C.border};">Delivery Charges</td>
                <td style="padding:8px 16px; background:${C.bgElevated};
                           color:${C.success}; font-size:12px;
                           font-weight:600; text-align:right;
                           border-bottom:1px solid ${C.border};">FREE</td>
            </tr>
            <tr>
                <td style="padding:10px 16px; background:${C.bgSection};
                           color:${C.textBright}; font-size:14px;
                           font-weight:700;">Total Paid</td>
                <td style="padding:10px 16px; background:${C.bgSection};
                           color:${C.accentLight}; font-size:16px;
                           font-weight:700; text-align:right;">
                    &#8377;${total.toFixed(2)}</td>
            </tr>
        </table>
    </div>

    <!-- ══ FOOTER ══ -->
    <div style="text-align:center; padding-top:14px;
                border-top:1px dashed ${C.border};
                color:${C.textMuted}; font-size:10px; line-height:1.7;">
        <div>Thank you for shopping with
            <span style="color:${C.accent}; font-weight:600;">FreshPicks</span>!
            &#x1F6D2;
        </div>
        <div>This is a computer-generated receipt. No signature required.</div>
        <div style="margin-top:6px; font-family:${C.mono}; font-size:9px;
                    color:#555555;">
            Receipt ID: ${orderData.orderId || ""} &nbsp;&middot;&nbsp;
            Generated: ${dateStr} &nbsp;&middot;&nbsp;
            CodeCrafters SDP-1
        </div>
    </div>

</div>`.trim();
}

/* ──────────────────────────────────────────────────────────────────────
   SECTION 5: PUBLIC API — generateStandardReceiptPDF(orderData)

   orderData shape:
   {
     orderId:      "ORD107",
     userId:       "U1003",              // shown as Customer if no fullName
     fullName:     "Yashwanth Kumar",    // optional — from /api/get_profile
     userPhone:    "9876543210",         // optional
     userEmail:    "y@example.com",      // optional
     userAddress:  "12,Main St,Area,600001", // optional
     date:         "2025-04-08",
     timestamp:    "2025-04-08 14:30:00",
     status:       "Delivered",
     deliverySlot: "Afternoon",
     total:        100.00,
     itemsString:  "V1001:Coriander Leaves:1000:100.00,VF101:Curry Leaves:50:0.00",
     boyName:      "Ramesh",
     boyPhone:     "9876543210"
   }

   Usage (user_orders.html — use downloadOrderReceipt(orderId) which calls this):
     generateStandardReceiptPDF({
         orderId:      order.order_id,
         userId:       order.user_id,
         fullName:     localStorage.getItem("fp_full_name") || order.user_id,
         userPhone:    localStorage.getItem("fp_phone")     || "",
         userEmail:    localStorage.getItem("fp_email")     || "",
         userAddress:  localStorage.getItem("fp_address")   || "",
         date:         order.timestamp?.slice(0, 10),
         timestamp:    order.timestamp,
         status:       order.status,
         deliverySlot: order.delivery_slot,
         total:        order.total_amount,
         itemsString:  order.items_string,
         boyName:      order.boy_name,
         boyPhone:     order.boy_phone
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

    /* ── Build the receipt HTML string — all inline hex, no CSS vars ── */
    const htmlString = _buildReceiptHTML(orderData);

    /* ── Mount off-screen container ── */
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

    /* ── Flush layout: two rAF cycles ensure full paint ── */
    await new Promise(resolve =>
        requestAnimationFrame(() => requestAnimationFrame(resolve))
    );

    /* ── html2pdf configuration ── */
    const opt = {
        margin:   [8, 6, 8, 6],   /* top, right, bottom, left in mm */
        filename,
        image:    { type: "jpeg", quality: 0.98 },
        html2canvas: {
            scale:           2,
            useCORS:         true,
            backgroundColor: "#1e1e1e",   /* canvas background = dark */
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
        alert("PDF generation failed. Check browser console (F12) for details.");
    } finally {
        /* Always clean up the off-screen node */
        if (stage.parentNode) {
            stage.parentNode.removeChild(stage);
        }
    }
}

/* ── CommonJS export (no-op in browser; useful for Node testing) ── */
if (typeof module !== "undefined" && module.exports) {
    module.exports = { generateStandardReceiptPDF, _parseItems };
}
