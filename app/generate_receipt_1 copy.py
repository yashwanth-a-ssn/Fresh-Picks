"""
generate_receipt.py — Fresh Picks: Server-Side PDF Receipt Engine
=================================================================
Called by app.py's /api/download_receipt/<order_id> route.

INSTALL:  pip install fpdf2 requests Pillow
CALL:     generate_receipt(data, output_path)

FONTS (place in static/fonts/):
    DejaVuSans.ttf          — regular
    DejaVuSans-Bold.ttf     — bold
    Both from https://dejavu-fonts.github.io/

IMAGES
    fetch_product_images.py must live in the same directory.
    It downloads /product_images/<veg_id>.jpeg from Flask and
    caches resized PNGs to static/product_images_cache/.
    If a cached image is absent the row renders text-only (no crash).

MULTI-PAGE
    PAGE_BOTTOM = 275 mm.  Every item row and block is guarded.
    Overflow → add_page() → re-draw table header → continue.

DATA DICT:
    order_id, user_id, full_name, user_phone, user_email,
    address  ("line1,line2,line3,postcode"),
    slot, status, timestamp ("YYYY-MM-DD HH:MM:SS"),
    boy_name, boy_phone, total (float), items_string

ITEMS_STRING:  veg_id:name:qty_g:price_per_1000g  (comma-separated)

Team: CodeCrafters | Project: Fresh Picks | SDP-1
"""

import os
from fpdf import FPDF
from datetime import datetime

# ── Path helpers ──────────────────────────────────────────────
_HERE     = os.path.dirname(os.path.abspath(__file__))
_FONT_DIR = os.path.join(_HERE, "..", "static", "fonts")
_IMG_DIR  = os.path.join(_HERE, "..", "static", "product_images_cache")

def _font(name: str) -> str:
    return os.path.normpath(os.path.join(_FONT_DIR, name))

def _cached_img(veg_id: str) -> str | None:
    """
    Return the absolute path to a cached product PNG, or None.
    Tries to fetch+cache on-the-fly via fetch_product_images if missing.
    """
    path = os.path.normpath(os.path.join(_IMG_DIR, f"{veg_id}.png"))
    if os.path.isfile(path):
        return path
    # Try lazy-fetch (requires Flask to be running)
    try:
        from fetch_product_images import fetch_and_cache
        return fetch_and_cache(veg_id)
    except Exception:
        return None

# ── Brand colours ─────────────────────────────────────────────
C_WHITE        = (255, 255, 255)
C_BLACK        = ( 30,  30,  30)
C_ACCENT       = (  0, 122, 204)
C_LABEL        = (120, 120, 120)
C_BORDER       = (200, 200, 200)
C_BORDER_D     = (210, 210, 210)
C_GREEN        = ( 34, 153,  61)
C_GREEN_BG     = (230, 247, 235)
C_HEADER_BG    = (  0, 122, 204)
C_ROW_ALT      = (247, 249, 252)
C_STATUS_DEL   = ( 34, 153,  61)
C_STATUS_PLC   = (204, 120,   0)
C_STATUS_PLC_BG= (255, 248, 230)


# ── Parsers ───────────────────────────────────────────────────
def _parse_timestamp(ts: str) -> str:
    try:
        dt = datetime.strptime(ts.strip(), "%Y-%m-%d %H:%M:%S")
        return dt.strftime("%-d %b %Y, %I:%M %p")
    except Exception:
        return ts


def _parse_items(items_string: str) -> list:
    """
    Returns list of dicts, each with an extra 'veg_id' key used for image lookup.
    ₹ (U+20B9) and — (U+2014) passed directly; DejaVuSans handles both.
    """
    items = []
    if not items_string or not items_string.strip():
        return items
    for part in items_string.split(","):
        segs = [s.strip() for s in part.strip().split(":")]
        if len(segs) < 3 or not segs[0]:
            continue
        veg_id = segs[0]
        if len(segs) >= 4:
            name, qty_g, price_snap = segs[1], int(segs[2]) if segs[2].isdigit() else 0, float(segs[3]) if segs[3] else 0.0
        else:
            name, qty_g, price_snap = veg_id, int(segs[1]) if segs[1].isdigit() else 0, float(segs[2]) if segs[2] else 0.0

        is_free    = (price_snap == 0.0) or veg_id.startswith("VF")
        qty_kg     = qty_g / 1000.0
        qty_str    = f"{qty_kg:.2f} kg" if qty_kg >= 1 else f"{qty_g} g"
        rate_str   = "\u2014" if is_free else f"\u20b9{price_snap:.2f}/kg"
        amount     = (price_snap * qty_g) / 1000.0 if not is_free else 0.0
        amount_str = "FREE" if is_free else f"\u20b9{amount:.2f}"

        items.append(dict(veg_id=veg_id, name=name, qty_g=qty_g,
                          qty_str=qty_str, rate_str=rate_str, amount=amount,
                          amount_str=amount_str, is_free=is_free))
    return items


# ── PDF subclass ──────────────────────────────────────────────
class ReceiptPDF(FPDF):
    def header(self): pass
    def footer(self): pass


# ── Drawing primitives ────────────────────────────────────────
def _solid_line(pdf, y, left, right, color=C_BORDER, lw=0.3):
    pdf.set_draw_color(*color)
    pdf.set_line_width(lw)
    pdf.line(left, y, right, y)


def _dashed_line(pdf, y, left, right,
                 color=C_BORDER_D, lw=0.2, dash=3.0, gap=2.0):
    pdf.set_draw_color(*color)
    pdf.set_line_width(lw)
    x = left
    while x < right:
        pdf.line(x, y, min(x + dash, right), y)
        x += dash + gap


def _table_header(pdf, y, F, left, col_widths) -> float:
    """Draw the blue items-table header row; return y after it."""
    hdr_h = 8
    pdf.set_fill_color(*C_HEADER_BG)
    pdf.set_draw_color(*C_HEADER_BG)
    pdf.set_line_width(0)
    pdf.rect(left, y, sum(col_widths), hdr_h, style="F")
    pdf.set_font(F, "B", 8)
    pdf.set_text_color(*C_WHITE)
    x = left
    for h, cw, al in zip(["#", "ITEM", "RATE", "QTY", "AMOUNT"],
                          col_widths,
                          ["C",  "L",   "R",   "R",    "R"]):
        pdf.set_xy(x + (1 if al == "L" else 0), y + 1)
        pdf.cell(cw, 6, h, align=al)
        x += cw
    return y + hdr_h


# ── Main ──────────────────────────────────────────────────────
def generate_receipt(data: dict, output_path: str) -> None:
    """Draw the A4 receipt and save to output_path."""

    pdf = ReceiptPDF(orientation="P", unit="mm", format="A4")
    pdf.set_auto_page_break(auto=False)
    pdf.add_page()

    # Fonts
    pdf.add_font("UF", "",  _font("DejaVuSans.ttf"))
    pdf.add_font("UF", "B", _font("DejaVuSans-Bold.ttf"))
    F = "UF"

    # Layout
    LEFT        = 15
    RIGHT       = 195
    W           = RIGHT - LEFT
    PAGE_BOTTOM = 275
    pdf.set_margins(LEFT, 10, 15)

    # ── Column widths with image slot ─────────────────────────
    # With images: # | [img+name] | Rate | Qty | Amount = 180
    IMG_W  = 8    # mm — thumbnail width inside PDF
    IMG_H  = 8    # mm — thumbnail height (square)
    # COL[1] holds both the thumbnail and the name text
    COL = [8, 88, 35, 24, 25]   # total = 180 mm

    y = 15

    # ═════════════════════════════════════════════════════════
    # SECTION 1 — HEADER
    # ═════════════════════════════════════════════════════════
    pdf.set_xy(LEFT, y)
    pdf.set_font(F, "B", 18)
    pdf.set_text_color(*C_ACCENT)
    pdf.cell(0, 8, "Fresh Picks", ln=False)

    pdf.set_font(F, "", 7)
    pdf.set_text_color(*C_LABEL)
    pdf.set_xy(LEFT, y)
    pdf.cell(W, 5, "ORDER RECEIPT", align="R", ln=False)

    y += 8

    # Order-ID chip
    cw, ch = 30, 8
    cx, cy = RIGHT - cw, y - 3
    pdf.set_fill_color(*C_WHITE)
    pdf.set_draw_color(*C_ACCENT)
    pdf.set_line_width(0.5)
    pdf.rect(cx, cy, cw, ch, style="D")
    pdf.set_xy(cx, cy + 1)
    pdf.set_font(F, "B", 10)
    pdf.set_text_color(*C_ACCENT)
    pdf.cell(cw, ch - 2, data.get("order_id", ""), align="C")

    # Timestamp
    pdf.set_xy(LEFT, cy + ch + 1)
    pdf.set_font(F, "", 7)
    pdf.set_text_color(*C_LABEL)
    pdf.cell(W, 4, _parse_timestamp(data.get("timestamp", "")), align="R", ln=False)

    # Store sub-lines (3 lines matching dark-theme receipt)
    pdf.set_xy(LEFT, y)
    pdf.set_font(F, "", 7.5)
    pdf.set_text_color(*C_LABEL)
    for sl in [
        "123 Grocery Ave, Market City, Chennai \u2013 600045",
        "GSTIN: 22AAAAA0000A1Z5  |  FSSAI: 10020042004971",
        "+91 98765 43210  |  support@freshpicks.in",
    ]:
        pdf.set_x(LEFT)
        pdf.cell(0, 4, sl, ln=True)
        y += 4

    y += 5
    _dashed_line(pdf, y, LEFT, RIGHT)
    y += 6

    # ═════════════════════════════════════════════════════════
    # SECTION 2 — BILL TO / SLOT / STATUS
    # ═════════════════════════════════════════════════════════
    col_w = W / 3

    def _lbl(x, yp, txt):
        pdf.set_xy(x, yp); pdf.set_font(F, "B", 6.5)
        pdf.set_text_color(*C_LABEL); pdf.cell(col_w, 4, txt.upper(), ln=False)

    def _val(x, yp, txt, color=C_BLACK, bold=True, size=10):
        pdf.set_xy(x, yp); pdf.set_font(F, "B" if bold else "", size)
        pdf.set_text_color(*color); pdf.cell(col_w, 5, txt, ln=False)

    _lbl(LEFT,           y, "Bill To")
    _lbl(LEFT + col_w,   y, "Delivery Slot")
    _lbl(LEFT + col_w*2, y, "Status")
    y += 5

    _val(LEFT,         y, data.get("full_name", ""), size=11)
    _val(LEFT + col_w, y, data.get("slot", ""),      size=11)

    status  = data.get("status", "")
    is_del  = "deliver" in status.lower()
    sx      = LEFT + col_w * 2
    bw, bh  = 38, 7
    pdf.set_fill_color(*(C_GREEN_BG     if is_del else C_STATUS_PLC_BG))
    pdf.set_draw_color(*(C_STATUS_DEL   if is_del else C_STATUS_PLC))
    pdf.set_line_width(0.4)
    pdf.rect(sx, y - 1, bw, bh, style="FD")
    pdf.set_xy(sx, y)
    pdf.set_font(F, "B", 7.5)
    pdf.set_text_color(*(C_STATUS_DEL if is_del else C_STATUS_PLC))
    # ✓ for delivered, plain text for other statuses — no stray ">" or "v"
    badge_label = ("\u2713 " + status.upper()) if is_del else status.upper()
    pdf.cell(bw, 5, badge_label, align="C")

    y += 12

    # ── Solid divider before CONTACT ─────────────────────────
    _solid_line(pdf, y, LEFT, RIGHT)
    y += 5

    # ═════════════════════════════════════════════════════════
    # SECTION 3 — CONTACT
    # ═════════════════════════════════════════════════════════
    pdf.set_xy(LEFT, y); pdf.set_font(F, "B", 6.5)
    pdf.set_text_color(*C_LABEL); pdf.cell(0, 4, "CONTACT", ln=True)
    y += 5

    pdf.set_xy(LEFT, y); pdf.set_font(F, "B", 9)
    pdf.set_text_color(*C_ACCENT); pdf.cell(0, 5, data.get("user_phone", ""), ln=True)
    y += 5

    pdf.set_xy(LEFT, y); pdf.set_font(F, "", 8.5)
    pdf.set_text_color(*C_BLACK); pdf.cell(0, 4, data.get("user_email", ""), ln=True)
    y += 5

    addr_parts = [p.strip() for p in data.get("address", "").split(",")]
    while len(addr_parts) < 4:
        addr_parts.append("")
    pdf.set_font(F, "", 8); pdf.set_text_color(*C_LABEL)
    for i, part in enumerate(addr_parts[:4]):
        pdf.set_xy(LEFT, y)
        pdf.cell(0, 4, part + ("," if i < 3 and part else ""), ln=True)
        y += 4

    y += 3

    # ── Solid divider after address ───────────────────────────
    _solid_line(pdf, y, LEFT, RIGHT)
    y += 5

    # ═════════════════════════════════════════════════════════
    # SECTION 4 — DELIVERY PARTNER
    # White box, grey border, scooter icon from DejaVuSans
    # ═════════════════════════════════════════════════════════
    box_h = 18
    pdf.set_fill_color(*C_WHITE)
    pdf.set_draw_color(*C_BORDER)
    pdf.set_line_width(0.4)
    pdf.rect(LEFT, y, W, box_h, style="FD")

    # "DELIVERY PARTNER" label
    pdf.set_xy(LEFT + 3, y + 2)
    pdf.set_font(F, "B", 6.5); pdf.set_text_color(*C_LABEL)
    pdf.cell(0, 4, "DELIVERY PARTNER")

    # ── Scooter / moto icon ───────────────────────────────────
    # DejaVuSans does NOT include the 🛵 emoji (U+1F6F5).
    # We draw a simple two-circle + body shape using filled rects & ellipses,
    # or use the closest text substitute that the font DOES support.
    # Best option with DejaVuSans: draw a minimal vector scooter inline.
    # We use a compact SVG-style approach via FPDF primitives:
    #   - Two small filled circles (wheels)
    #   - A filled rect (body)
    #   - A smaller rect (handlebar)
    # All drawn at approximately (LEFT+3, y+8).
    icon_x  = LEFT + 3
    icon_y  = y + 8
    r_wheel = 2.0   # wheel radius mm

    # Rear wheel
    pdf.set_fill_color(*C_ACCENT)
    pdf.set_draw_color(*C_ACCENT)
    pdf.ellipse(icon_x,           icon_y + 1, r_wheel*2, r_wheel*2, style="F")
    # Front wheel
    pdf.ellipse(icon_x + 8,       icon_y + 1, r_wheel*2, r_wheel*2, style="F")
    # Body (connecting rect)
    pdf.rect(icon_x + r_wheel,    icon_y + 2, 6, 1.5, style="F")
    # Seat / handlebar bump
    pdf.rect(icon_x + 7,          icon_y,     1.5, 2,  style="F")

    # Name + phone (offset past the icon)
    pdf.set_xy(LEFT + 16, y + 7)
    pdf.set_font(F, "B", 11); pdf.set_text_color(*C_BLACK)
    pdf.cell(0, 5, data.get("boy_name", ""))

    pdf.set_xy(LEFT + 16, y + 13)
    pdf.set_font(F, "", 8.5); pdf.set_text_color(*C_ACCENT)
    pdf.cell(0, 4, data.get("boy_phone", ""))

    y += box_h + 6

    # ═════════════════════════════════════════════════════════
    # SECTION 5 — ITEMS TABLE  (with product images)
    # ═════════════════════════════════════════════════════════
    items = _parse_items(data.get("items_string", ""))
    y     = _table_header(pdf, y, F, LEFT, COL)

    for row_num, item in enumerate(items, start=1):
        # Row height: use taller row if image present, else compact
        img_path = _cached_img(item["veg_id"])
        ROW_H    = max(IMG_H + 2, 8)   # at least 10mm when image present, 8mm otherwise

        # ── Overflow guard ───────────────────────────────────
        if y + ROW_H > PAGE_BOTTOM:
            pdf.add_page()
            y = 15
            y = _table_header(pdf, y, F, LEFT, COL)

        pdf.set_line_width(0.2); pdf.set_draw_color(*C_BORDER)

        if row_num % 2 == 0:
            pdf.set_fill_color(*C_ROW_ALT)
            pdf.rect(LEFT, y, W, ROW_H, style="F")

        pdf.line(LEFT, y + ROW_H, RIGHT, y + ROW_H)

        # # column
        pdf.set_xy(LEFT, y + (ROW_H - 6) / 2)
        pdf.set_font(F, "", 8); pdf.set_text_color(*C_LABEL)
        pdf.cell(COL[0], 6, str(row_num), align="C")

        # Image column (inside COL[1])
        name_x = LEFT + COL[0]
        if img_path:
            try:
                pdf.image(img_path,
                          x=name_x + 1,
                          y=y + (ROW_H - IMG_H) / 2,
                          w=IMG_W, h=IMG_H)
            except Exception:
                img_path = None   # fall back silently

        # Name text (offset right of image if present)
        text_x_offset = (IMG_W + 2) if img_path else 1
        pdf.set_xy(name_x + text_x_offset, y + (ROW_H - 6) / 2)
        pdf.set_font(F, "B" if not item["is_free"] else "", 8.5)
        pdf.set_text_color(*C_BLACK)
        available_name_w = COL[1] - text_x_offset - (14 if item["is_free"] else 0)
        pdf.cell(available_name_w, 6, item["name"])

        # FREE badge (green pill next to name)
        if item["is_free"]:
            bx = name_x + text_x_offset + pdf.get_string_width(item["name"]) + 1
            # Clamp so badge doesn't overflow into Rate column
            max_bx = LEFT + COL[0] + COL[1] - 14
            bx = min(bx, max_bx)
            pdf.set_fill_color(*C_GREEN_BG); pdf.set_draw_color(*C_GREEN)
            pdf.set_line_width(0.3)
            pdf.rect(bx, y + (ROW_H - 5) / 2, 13, 5, style="FD")
            pdf.set_xy(bx, y + (ROW_H - 5) / 2)
            pdf.set_font(F, "B", 6); pdf.set_text_color(*C_GREEN)
            pdf.cell(13, 5, "FREE", align="C")

        # Rate
        x = LEFT + COL[0] + COL[1]
        pdf.set_xy(x, y + (ROW_H - 6) / 2)
        pdf.set_font(F, "", 8)
        pdf.set_text_color(*C_LABEL if item["is_free"] else C_BLACK)
        pdf.cell(COL[2], 6, item["rate_str"], align="R")

        # Qty
        x += COL[2]
        pdf.set_xy(x, y + (ROW_H - 6) / 2)
        pdf.set_font(F, "", 8); pdf.set_text_color(*C_BLACK)
        pdf.cell(COL[3], 6, item["qty_str"], align="R")

        # Amount
        x += COL[3]
        pdf.set_xy(x, y + (ROW_H - 6) / 2)
        pdf.set_font(F, "B", 8.5)
        pdf.set_text_color(*C_GREEN if item["is_free"] else C_ACCENT)
        pdf.cell(COL[4], 6, item["amount_str"], align="R")

        y += ROW_H

    y += 4

    # ═════════════════════════════════════════════════════════
    # SECTION 6 — TOTALS
    # ═════════════════════════════════════════════════════════
    if y + 35 > PAGE_BOTTOM:
        pdf.add_page(); y = 20

    total = float(data.get("total", 0))

    def _trow(label, value, bold=False, vcol=C_BLACK):
        nonlocal y
        pdf.set_xy(LEFT, y)
        pdf.set_font(F, "B" if bold else "", 10 if bold else 9)
        pdf.set_text_color(*C_LABEL); pdf.cell(W - 40, 6, label)
        pdf.set_font(F, "B" if bold else "", 11 if bold else 9)
        pdf.set_text_color(*vcol); pdf.cell(40, 6, value, align="R", ln=True)
        y = pdf.get_y()

    _trow("Subtotal",         f"\u20b9{total:.2f}")
    _trow("Delivery Charges", "FREE", vcol=C_GREEN)

    y += 1
    _solid_line(pdf, y, LEFT, RIGHT, color=C_BORDER, lw=0.4)
    y += 3; pdf.set_y(y)

    pdf.set_xy(LEFT, y); pdf.set_font(F, "B", 12); pdf.set_text_color(*C_BLACK)
    pdf.cell(W - 40, 7, "Total Paid")
    pdf.set_text_color(*C_ACCENT)
    pdf.cell(40, 7, f"\u20b9{total:.2f}", align="R", ln=True)
    y = pdf.get_y() + 6

    # ═════════════════════════════════════════════════════════
    # SECTION 7 — FOOTER
    # ═════════════════════════════════════════════════════════
    if y + 22 > PAGE_BOTTOM:
        pdf.add_page(); y = 20

    _dashed_line(pdf, y, LEFT, RIGHT)
    y += 5

    pdf.set_xy(LEFT, y); pdf.set_font(F, "", 8); pdf.set_text_color(*C_LABEL)
    pdf.multi_cell(W, 4.5,
        "Thank you for shopping with FreshPicks!\n"
        "This is a computer-generated receipt. No signature required.",
        align="C")
    y = pdf.get_y() + 3

    oid  = data.get("order_id", "")
    date = data.get("timestamp", "")[:10]
    pdf.set_xy(LEFT, y); pdf.set_font(F, "", 6.5); pdf.set_text_color(*C_LABEL)
    pdf.cell(W, 4,
        f"Receipt ID: {oid}   Generated: {date}   CodeCrafters - SDP-1",
        align="C")
    y = pdf.get_y() + 5

    _dashed_line(pdf, y, LEFT, RIGHT)

    pdf.output(output_path)
