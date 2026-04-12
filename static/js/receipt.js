/**
 * receipt.js — Fresh Picks: Centralized PDF Receipt Generator
 * ============================================================
 * Loaded by both cart.html and user_orders.html via:
 *   <script src="/static/js/receipt.js"></script>
 *
 * PUBLIC API:
 *   generateStandardReceiptPDF(orderData)  → Promise<void>
 *
 * LAYOUT (pixel-perfect replica of the UI screenshot, light theme):
 *   ┌─────────────────────────────────────────┐
 *   │ 🧺 Fresh Picks          ORDER RECEIPT   │  ← two-col header table
 *   │ 123 Grocery Ave...       ┌──────────┐   │
 *   │ GSTIN | FSSAI            │  ORD112  │   │
 *   │ 📞 +91...                └──────────┘   │
 *   │                         12 Apr 2026...  │
 *   ├ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┤  ← dashed separator
 *   │ BILL TO   │ DELIVERY SLOT │ STATUS      │  ← 3-col meta row
 *   │ Name      │ Afternoon     │ ✓ PAID      │
 *   ├─────────────────────────────────────────┤
 *   │ CONTACT                                 │
 *   │ 📞 6382717541                           │
 *   │ ✉ yashwanth@...                         │
 *   │ No 11 - Flat No 11                      │  ← 4 lines, one per part
 *   │ Elumalai Street                         │
 *   │ West Tambaram                           │
 *   │ 600045                                  │
 *   ├─────────────────────────────────────────┤
 *   │ ║ DELIVERY PARTNER   (left accent bar)  │  ← box-shadow inset fix
 *   │ ║ 🛵 Ramesh  📞 9876543210              │
 *   ├─────────────────────────────────────────┤
 *   │  #  │ ITEM           │ RATE  │ QTY │ ₹  │  ← items table
 *   │  1  │ Coriander...   │ ₹.../kg│1kg │100 │
 *   ├─────────────────────────────────────────┤
 *   │ Subtotal                       ₹100.00  │
 *   │ Delivery Charges                  FREE  │
 *   │ Total Paid                    ₹100.00   │  ← bold blue
 *   ├ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┤
 *   │    Thank you for shopping with FreshPicks│
 *   └─────────────────────────────────────────┘
 *
 * THEME:        Light (#ffffff). Brand accent: #007acc.
 * ALL STYLES:   Hardcoded inline hex — zero CSS variables.
 * ALL LAYOUTS:  <table> — never display:flex (html2canvas compat).
 * LEFT BORDER:  box-shadow inset on delivery partner card (not border-left
 *               which html2canvas clips at the canvas edge).
 * ADDRESS:      "door,street,area,pincode" split into 4 separate lines.
 * SCALE:        3 (high-res capture).
 * FILENAME:     ${orderId}_${userId}_${date}.pdf
 *
 * DEPENDENCY: html2pdf.js must be loaded BEFORE this file.
 *   <script src="https://cdnjs.cloudflare.com/ajax/libs/html2pdf.js/0.10.1/html2pdf.bundle.min.js"></script>
 *   <script src="/static/js/receipt.js"></script>
 *
 * Team: CodeCrafters | Project: Fresh Picks | SDP-1
 */

/* ══════════════════════════════════════════════════════════════════════
   SECTION 1: COLOUR PALETTE  (light / print-friendly)
   Every token is a literal hex. No CSS variables anywhere.
   ══════════════════════════════════════════════════════════════════════ */
const RECEIPT_COLORS = {
    bg:           "#ffffff",
    bgAlt:        "#f5f9fd",
    bgMeta:       "#f0f6fc",
    bgHeader:     "#007acc",
    bgAgent:      "#f0f6fc",
    border:       "#cde0f0",
    borderDash:   "#b8d4e8",
    borderAccent: "#007acc",
    text:         "#1a1a2e",
    textMuted:    "#6b8faa",
    textLabel:    "#4a6fa5",
    textBright:   "#0a1628",
    textAccent:   "#007acc",
    textOnBlue:   "#ffffff",
    success:      "#166534",
    successBg:    "#dcfce7",
    successBorder:"#86efac",
    warning:      "#92400e",
    warningBg:    "#fef3c7",
    warningBorder:"#fcd34d",
    danger:       "#b91c1c",
    dangerBg:     "#fee2e2",
    dangerBorder: "#fca5a5",
    paidGreen:    "#166534",
    paidBg:       "#dcfce7",
    paidBorder:   "#86efac",
    phonePink:    "#e83e8c",
    mono:         "Consolas, 'Courier New', monospace",
    sans:         "'Segoe UI', Arial, system-ui, sans-serif"
};

/* ══════════════════════════════════════════════════════════════════════
   SECTION 2: ITEM STRING PARSER
   4-part (current): veg_id:name:qty_g:price_per_1000g
   3-part (legacy):  veg_id:qty_g:price_per_1000g
   ══════════════════════════════════════════════════════════════════════ */
function _parseItems(itemsString) {
    if (!itemsString || !itemsString.trim()) return [];
    return itemsString.split(",").filter(Boolean).map(function(entry) {
        var parts = entry.trim().split(":");
        var id, name, qty, price;
        if (parts.length >= 4) {
            id = parts[0]; name = parts[1]; qty = parts[2]; price = parts[3];
        } else if (parts.length === 3) {
            id = parts[0]; qty = parts[1]; price = parts[2]; name = id;
        } else {
            return null;
        }
        var qtyNum   = parseInt(qty)    || 0;
        var priceNum = parseFloat(price) || 0;
        var isFree   = priceNum === 0;
        var qtyLabel = qtyNum >= 1000
            ? (qtyNum / 1000).toFixed(2) + " kg"
            : qtyNum + " g";
        var amount = isFree ? 0 : (qtyNum / 1000) * priceNum;
        return { id: id, name: name || id, qty: qtyLabel, price: priceNum, amount: amount, isFree: isFree };
    }).filter(Boolean);
}

/* ══════════════════════════════════════════════════════════════════════
   SECTION 3: STATUS / SLOT BADGE STYLES (light-theme print-safe)
   ══════════════════════════════════════════════════════════════════════ */
function _statusStyle(status) {
    var C = RECEIPT_COLORS;
    var map = {
        "Order Placed":     { bg: "#dbeafe", color: "#1d4ed8", border: "#93c5fd", label: "PAID"             },
        "Out for Delivery": { bg: C.warningBg, color: C.warning, border: C.warningBorder, label: "OUT FOR DELIVERY" },
        "Delivered":        { bg: C.paidBg,  color: C.paidGreen,  border: C.paidBorder,  label: "DELIVERED"  },
        "Cancelled":        { bg: C.dangerBg, color: C.danger,    border: C.dangerBorder, label: "CANCELLED"  }
    };
    return map[status] || map["Order Placed"];
}

function _slotStyle(slot) {
    var map = {
        "Morning":   { bg: "#dcfce7", color: "#166534", border: "#86efac" },
        "Afternoon": { bg: "#fef3c7", color: "#92400e", border: "#fcd34d" },
        "Evening":   { bg: "#dbeafe", color: "#1e40af", border: "#93c5fd" }
    };
    return map[slot] || map["Evening"];
}

/* ══════════════════════════════════════════════════════════════════════
   SECTION 4: ADDRESS FORMATTER
   Splits "door,street,area,pincode" into exactly 4 separate line divs.
   ══════════════════════════════════════════════════════════════════════ */
function _formatAddress(addressStr) {
    if (!addressStr || !addressStr.trim()) return "";
    var parts = addressStr.split(",").map(function(p) { return p.trim(); }).filter(Boolean);
    return parts.map(function(part) {
        return '<div style="font-size:12px; color:#6b8faa; line-height:1.7; margin:0;">' + part + '</div>';
    }).join("");
}

/* ══════════════════════════════════════════════════════════════════════
   SECTION 5: TIMESTAMP FORMATTER
   "2026-04-12 14:10:12"  →  "12 Apr 2026, 02:10 pm"
   ══════════════════════════════════════════════════════════════════════ */
function _formatTimestamp(ts, fallback) {
    if (!ts) return fallback || "";
    try {
        var d = new Date(ts.replace(" ", "T"));
        if (isNaN(d)) return ts;
        var months = ["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"];
        var day  = d.getDate();
        var mon  = months[d.getMonth()];
        var yr   = d.getFullYear();
        var hr   = d.getHours();
        var min  = ("0" + d.getMinutes()).slice(-2);
        var ampm = hr >= 12 ? "pm" : "am";
        hr = hr % 12 || 12;
        return day + " " + mon + " " + yr + ", " + hr + ":" + min + " " + ampm;
    } catch (e) {
        return ts;
    }
}

/* ══════════════════════════════════════════════════════════════════════
   SECTION 6: HTML BUILDER  (_buildReceiptHTML)

   Matches the UI screenshot structure exactly:
     1. Two-column header (brand left, order-ID right)
     2. Dashed separator
     3. Three-column meta row (BILL TO | DELIVERY SLOT | STATUS)
     4. CONTACT block (phone, email, 4-line address)
     5. DELIVERY PARTNER card (box-shadow inset for left border)
     6. Items table (#|ITEM|RATE|QTY|AMOUNT)
     7. Totals rows (Subtotal / Delivery Charges / Total Paid)
     8. Dashed footer
   ══════════════════════════════════════════════════════════════════════ */
function _buildReceiptHTML(orderData) {
    var C            = RECEIPT_COLORS;
    var ss           = _statusStyle(orderData.status || "Order Placed");
    var sl           = _slotStyle(orderData.deliverySlot || "Morning");
    var items        = _parseItems(orderData.itemsString || "");
    var subtotal     = items.reduce(function(acc, it) { return acc + it.amount; }, 0);
    var total        = parseFloat(orderData.total || subtotal);
    var customerName = orderData.fullName || orderData.userId || "Customer";
    var dateStr      = orderData.date || new Date().toISOString().slice(0, 10);
    var displayTS    = _formatTimestamp(orderData.timestamp, dateStr);
    var addressLines = _formatAddress(orderData.userAddress || "");

    /* ── 1. HEADER ──────────────────────────────────────────────── */
    var headerHTML = [
        '<table style="width:100%; border-collapse:collapse; margin-bottom:0;">',
        '  <tr>',
        /* LEFT — brand */
        '    <td style="vertical-align:top; padding:0; width:55%;">',
        '      <table style="border-collapse:collapse; margin-bottom:6px;">',
        '        <tr>',
        '          <td style="padding:0 10px 0 0; vertical-align:middle; width:34px; line-height:1;">',
        '            <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" fill="#e3b172" viewBox="0 0 16 16">',
        '              <path d="M5.757 1.071a.5.5 0 0 1 .172.686L3.383 6h9.234L10.07 1.757a.5.5 0 1 1 .858-.514L13.783 6H15a1 1 0 0 1 1 1v1a1 1 0 0 1-1 1H1a1 1 0 0 1-1-1V7a1 1 0 0 1 1-1h1.217L5.07 1.243a.5.5 0 0 1 .686-.172zM3.394 10h9.212l-.69 3.104A2 2 0 0 1 9.96 15H6.04a2 2 0 0 1-1.956-1.896L3.394 10z"/>',
        '            </svg>',
        '          </td>',
        '          <td style="padding:0; vertical-align:middle;">',
        '            <span style="font-size:19px; font-weight:700;',
        '                         color:#007acc; letter-spacing:-0.2px;">',
        '              Fresh Picks',
        '            </span>',
        '          </td>',
        '        </tr>',
        '      </table>',
        '      <div style="font-size:11px; color:#6b8faa; line-height:1.65;">',
        '        123 Grocery Ave, Market City, Chennai &ndash; 600045',
        '      </div>',
        '      <div style="font-size:11px; color:#6b8faa; line-height:1.65;">',
        '        GSTIN: 22AAAAA0000A1Z5 &nbsp;|&nbsp; FSSAI: 10020042004971',
        '      </div>',
        '      <div style="font-size:11px; color:#6b8faa; line-height:1.65;">',
        '        <span style="color:#e83e8c;">&#128222;</span>',
        '        +91 98765 43210 &nbsp;|&nbsp; support@freshpicks.in',
        '      </div>',
        '    </td>',
        /* RIGHT — order ID */
        '    <td style="vertical-align:top; text-align:right; padding:0; width:45%;">',
        '      <div style="font-size:10px; color:#6b8faa; letter-spacing:0.1em;',
        '                  text-transform:uppercase; margin-bottom:5px;">',
        '        ORDER RECEIPT',
        '      </div>',
        '      <span style="font-family:Consolas,\'Courier New\',monospace;',
        '                   font-size:16px; font-weight:700; color:#007acc;',
        '                   border:2px solid #007acc; border-radius:4px;',
        '                   padding:3px 12px; background:#f0f6fc;',
        '                   display:inline-block; letter-spacing:0.02em;">',
        '        ' + (orderData.orderId || ""),
        '      </span>',
        '      <div style="font-size:11px; color:#6b8faa; margin-top:6px;">',
        '        ' + displayTS,
        '      </div>',
        '    </td>',
        '  </tr>',
        '</table>'
    ].join("\n");

    /* ── 2. THREE-COLUMN META ROW ────────────────────────────────── */
    var metaRowHTML = [
        '<table style="width:100%; border-collapse:collapse; page-break-inside:avoid;">',
        '  <tr>',
        /* BILL TO */
        '    <td style="vertical-align:top; padding:14px 20px 14px 0;',
        '               width:38%; border-bottom:1px solid #cde0f0;">',
        '      <div style="font-size:10px; color:#6b8faa; text-transform:uppercase;',
        '                  letter-spacing:0.09em; font-weight:700; margin-bottom:5px;">BILL TO</div>',
        '      <div style="font-size:14px; font-weight:700; color:#0a1628;">',
        '        ' + customerName,
        '      </div>',
        '    </td>',
        /* DELIVERY SLOT */
        '    <td style="vertical-align:top; padding:14px 20px;',
        '               width:32%; border-bottom:1px solid #cde0f0;',
        '               border-left:1px solid #cde0f0;">',
        '      <div style="font-size:10px; color:#6b8faa; text-transform:uppercase;',
        '                  letter-spacing:0.09em; font-weight:700; margin-bottom:5px;">DELIVERY SLOT</div>',
        '      <div style="font-size:14px; font-weight:700; color:#0a1628;">',
        '        ' + (orderData.deliverySlot || "&mdash;"),
        '      </div>',
        '    </td>',
        /* STATUS */
        '    <td style="vertical-align:top; padding:14px 0 14px 20px;',
        '               width:30%; border-bottom:1px solid #cde0f0;',
        '               border-left:1px solid #cde0f0;">',
        '      <div style="font-size:10px; color:#6b8faa; text-transform:uppercase;',
        '                  letter-spacing:0.09em; font-weight:700; margin-bottom:5px;">STATUS</div>',
        '      <span style="display:inline-block; background:' + ss.bg + ';',
        '                   color:' + ss.color + '; border:1.5px solid ' + ss.border + ';',
        '                   border-radius:5px; padding:3px 11px;',
        '                   font-size:12px; font-weight:700;">',
        '        &#10003; ' + ss.label,
        '      </span>',
        '    </td>',
        '  </tr>',
        '</table>'
    ].join("\n");

    /* ── 3. CONTACT BLOCK ────────────────────────────────────────── */
    var hasContact = orderData.userPhone || orderData.userEmail || orderData.userAddress;
    var contactHTML = "";
    if (hasContact) {
        contactHTML = [
            '<div style="padding:14px 0; border-bottom:1px solid #cde0f0;">',
            '  <div style="font-size:10px; color:#6b8faa; text-transform:uppercase;',
            '              letter-spacing:0.09em; font-weight:700; margin-bottom:8px;">CONTACT</div>',
            orderData.userPhone
                ? '<div style="font-size:13px; font-weight:600; color:#e83e8c; margin-bottom:4px; line-height:1.6;">'
                    + '<span style="color:#e83e8c;">&#128222;</span>&nbsp;' + orderData.userPhone
                    + '</div>'
                : "",
            orderData.userEmail
                ? '<div style="font-size:13px; color:#1a1a2e; margin-bottom:6px; line-height:1.6;">'
                    + '<span style="color:#6b8faa; font-size:14px;">&#9993;</span>&nbsp;' + orderData.userEmail
                    + '</div>'
                : "",
            addressLines,
            '</div>'
        ].join("\n");
    }

    /* ── 4. DELIVERY PARTNER card ────────────────────────────────── */
    /* LEFT BORDER: box-shadow inset 4px avoids the html2canvas border-left
       clipping bug. The inset shadow renders on ALL four rasterised edges. */
    var agentHTML = "";
    if (orderData.boyName) {
        agentHTML = [
            '<div style="margin:14px 0;',
            '            padding:14px 18px;',
            '            background:#f0f6fc;',
            '            border-radius:6px;',
            '            border:1px solid #cde0f0;',
            '            box-shadow: inset 4px 0 0 0 #007acc;">',
            '  <div style="font-size:10px; color:#6b8faa; text-transform:uppercase;',
            '              letter-spacing:0.09em; font-weight:700; margin-bottom:8px;">',
            '    DELIVERY PARTNER',
            '  </div>',
            '  <table style="border-collapse:collapse; width:100%;">',
            '    <tr>',
            '      <td style="font-size:22px; width:36px; vertical-align:middle;',
            '                 padding:0 10px 0 0; line-height:1;">&#x1F6F5;</td>',
            '      <td style="vertical-align:middle; padding:0;">',
            '        <div style="font-size:15px; font-weight:700; color:#0a1628; line-height:1.3;">',
            '          ' + orderData.boyName,
            '        </div>',
            orderData.boyPhone
                ? '        <div style="font-size:12px; color:#e83e8c; margin-top:3px; line-height:1.5;">'
                    + '<span style="color:#e83e8c;">&#128222;</span>&nbsp;' + orderData.boyPhone + '</div>'
                : "",
            '      </td>',
            '    </tr>',
            '  </table>',
            '</div>'
        ].join("\n");
    }

    /* ── 5. ITEMS TABLE ROWS ─────────────────────────────────────── */
    var itemRows;
    if (items.length > 0) {
        itemRows = items.map(function(it, idx) {
            return [
                '<tr style="background:' + (idx % 2 === 0 ? "#ffffff" : "#f5f9fd") + '; page-break-inside:avoid;">',
                '  <td style="padding:9px 10px; text-align:center; font-size:12px;',
                '             color:#6b8faa; border-bottom:1px solid #cde0f0; width:5%;">',
                '    ' + (idx + 1),
                '  </td>',
                '  <td style="padding:9px 10px; font-size:12.5px; font-weight:600;',
                '             color:#0a1628; border-bottom:1px solid #cde0f0; width:42%;">',
                '    ' + it.name + (it.isFree
                    ? '&nbsp;<span style="background:#dcfce7; color:#166534; font-size:10px;'
                        + 'font-weight:700; padding:1px 6px; border-radius:8px;">FREE</span>'
                    : ""),
                '  </td>',
                '  <td style="padding:9px 10px; text-align:right; font-size:12px;',
                '             color:#6b8faa; border-bottom:1px solid #cde0f0; width:20%;">',
                '    ' + (it.isFree ? "&mdash;" : "&#8377;" + it.price.toFixed(2) + "/kg"),
                '  </td>',
                '  <td style="padding:9px 10px; text-align:right; font-size:12px;',
                '             color:#1a1a2e; border-bottom:1px solid #cde0f0; width:13%;">',
                '    ' + it.qty,
                '  </td>',
                '  <td style="padding:9px 10px; text-align:right; font-size:12.5px;',
                '             font-weight:' + (it.isFree ? "700" : "600") + ';',
                '             color:' + (it.isFree ? "#166534" : "#007acc") + ';',
                '             border-bottom:1px solid #cde0f0; width:20%;">',
                '    &#8377;' + it.amount.toFixed(2),
                '  </td>',
                '</tr>'
            ].join("\n");
        }).join("\n");
    } else {
        itemRows = '<tr><td colspan="5" style="padding:16px; text-align:center;'
            + 'color:#6b8faa; font-size:12px;">&mdash; No items &mdash;</td></tr>';
    }

    /* ── 6. ASSEMBLE ─────────────────────────────────────────────── */
    return [
        '<div style="',
        '    background-color:#ffffff;',
        '    color:#1a1a2e;',
        '    font-family:\'Segoe UI\', Arial, system-ui, sans-serif;',
        '    font-size:13px;',
        '    line-height:1.5;',
        '    padding:26px 30px;',
        '    box-sizing:border-box;',
        '    width:100%;',
        '    border:1px solid #cde0f0;',
        '    border-radius:8px;',
        '">',

        /* Header */
        headerHTML,

        /* Dashed separator */
        '<div style="border-top:1.5px dashed #b8d4e8; margin:14px 0;"></div>',

        /* 3-col meta */
        metaRowHTML,

        /* Contact */
        contactHTML,

        /* Delivery partner */
        agentHTML,

        /* Items table */
        '<table style="width:100%; border-collapse:collapse; border:1px solid #cde0f0; page-break-inside:auto;',
        '              margin-top:6px;">',
        '  <thead>',
        '    <tr style="background:#007acc;">',
        '      <th style="padding:9px 10px; text-align:center; font-size:11px;',
        '                 color:#ffffff; font-weight:600; width:5%;',
        '                 border-bottom:1px solid #005fa3;">#</th>',
        '      <th style="padding:9px 10px; text-align:left; font-size:11px;',
        '                 color:#ffffff; font-weight:600; width:42%;',
        '                 border-bottom:1px solid #005fa3;">ITEM</th>',
        '      <th style="padding:9px 10px; text-align:right; font-size:11px;',
        '                 color:#ffffff; font-weight:600; width:20%;',
        '                 border-bottom:1px solid #005fa3;">RATE</th>',
        '      <th style="padding:9px 10px; text-align:right; font-size:11px;',
        '                 color:#ffffff; font-weight:600; width:13%;',
        '                 border-bottom:1px solid #005fa3;">QTY</th>',
        '      <th style="padding:9px 10px; text-align:right; font-size:11px;',
        '                 color:#ffffff; font-weight:600; width:20%;',
        '                 border-bottom:1px solid #005fa3;">AMOUNT</th>',
        '    </tr>',
        '  </thead>',
        '  <tbody>',
        itemRows,
        '  </tbody>',
        '</table>',

        /* Totals */
        '<div class="avoid-break" style="border-top:1px solid #cde0f0; margin-top:8px; padding-top:2px;">',
        '<table style="width:100%; border-collapse:collapse;">',
        '  <tr>',
        '    <td style="padding:8px 12px; font-size:13px; color:#6b8faa;',
        '               border-bottom:1px solid #cde0f0; width:70%;">Subtotal</td>',
        '    <td style="padding:8px 12px; font-size:13px; color:#1a1a2e;',
        '               text-align:right; border-bottom:1px solid #cde0f0; width:30%;">',
        '      &#8377;' + subtotal.toFixed(2),
        '    </td>',
        '  </tr>',
        '  <tr>',
        '    <td style="padding:8px 12px; font-size:13px; color:#6b8faa;',
        '               border-bottom:1px solid #cde0f0;">Delivery Charges</td>',
        '    <td style="padding:8px 12px; font-size:13px; font-weight:700;',
        '               color:#166534; text-align:right;',
        '               border-bottom:1px solid #cde0f0;">FREE</td>',
        '  </tr>',
        '  <tr>',
        '    <td style="padding:11px 12px; font-size:15px; font-weight:700;',
        '               color:#0a1628;">Total Paid</td>',
        '    <td style="padding:11px 12px; font-size:16px; font-weight:700;',
        '               color:#007acc; text-align:right;">',
        '      &#8377;' + total.toFixed(2),
        '    </td>',
        '  </tr>',
        '</table>',
        '</div>',

        /* Dashed footer */
        '<div style="border-top:1.5px dashed #b8d4e8; margin:16px 0 12px;"></div>',
        '<div style="text-align:center; color:#6b8faa; font-size:11px; line-height:1.9;">',
        '  <div>',
        '    Thank you for shopping with',
        '    <span style="color:#007acc; font-weight:600;">FreshPicks</span>!',
        '    &#x1F6D2;',
        '  </div>',
        '  <div>This is a computer-generated receipt. No signature required.</div>',
        '  <div style="margin-top:4px; font-family:Consolas,\'Courier New\',monospace;',
        '              font-size:9.5px; color:#9eb8cc;">',
        '    Receipt ID: ' + (orderData.orderId || "") + ' &nbsp;&middot;&nbsp;',
        '    Generated: ' + dateStr + ' &nbsp;&middot;&nbsp;',
        '    CodeCrafters SDP-1',
        '  </div>',
        '</div>',

        '</div>'
    ].join("\n");
}

/* ══════════════════════════════════════════════════════════════════════
   SECTION 7: PUBLIC API — generateStandardReceiptPDF(orderData)

   orderData shape:
   {
     orderId:      "ORD112",
     userId:       "U001",
     fullName:     "Yashwanth A",
     userPhone:    "6382717541",
     userEmail:    "yashwantharumugam2007@gmail.com",
     userAddress:  "No 11 - Flat No 11,Elumalai Street,West Tambaram,600045",
     date:         "2026-04-12",
     timestamp:    "2026-04-12 14:10:12",
     status:       "Out for Delivery",
     deliverySlot: "Afternoon",
     total:        100.00,
     itemsString:  "V1001:Coriander Leaves:1000:100.00",
     boyName:      "Ramesh",
     boyPhone:     "9876543210"
   }
   ══════════════════════════════════════════════════════════════════════ */
async function generateStandardReceiptPDF(orderData) {

    if (!orderData || !orderData.orderId) {
        console.error("[receipt.js] generateStandardReceiptPDF: missing orderData.orderId");
        alert("Cannot generate PDF — order data is incomplete.");
        return;
    }

    var dateStr  = (orderData.date || new Date().toISOString().slice(0, 10)).replace(/\s+/g, "");
    var userId   = (orderData.userId || "UNKNOWN").replace(/\s+/g, "_");
    var filename = orderData.orderId + "_" + userId + "_" + dateStr + ".pdf";

    var htmlString = _buildReceiptHTML(orderData);

    /* Wrapper positions the receipt card off-screen.
       position:absolute (not fixed) + left far negative avoids
       html2canvas coordinate-offset bugs that crop the left side. */
    var wrapper = document.createElement("div");
    wrapper.style.cssText = [
        "position:absolute",
        "top:-99999px",
        "left:-9999px",
        "width:210mm",
        "background:#ffffff",
        "overflow:visible",
        "z-index:-9999",
        "pointer-events:none"
    ].join(";");

    wrapper.innerHTML = htmlString;
    document.body.appendChild(wrapper);

    var receiptEl = wrapper.firstElementChild;

    /* Two rAF cycles — ensures complete paint before canvas capture */
    await new Promise(function(resolve) {
        requestAnimationFrame(function() { requestAnimationFrame(resolve); });
    });

    var opt = {
        margin:      [8, 8, 8, 8],
        filename:    filename,
        image:       { type: "jpeg", quality: 0.99 },
        pagebreak:   { mode: "css", avoid: ".avoid-break" },
        html2canvas: {
            scale:           2,
            useCORS:         true,
            backgroundColor: "#ffffff",
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
        alert("PDF generation failed. Check the browser console (F12) for details.");
    } finally {
        if (wrapper.parentNode) {
            wrapper.parentNode.removeChild(wrapper);
        }
    }
}

/* ── CommonJS export (no-op in browser; for Node/Jest testing) ── */
if (typeof module !== "undefined" && module.exports) {
    module.exports = { generateStandardReceiptPDF, _parseItems };
}