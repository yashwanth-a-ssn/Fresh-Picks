"""
generate_receipt.py — Fresh Picks: Server-Side PDF Receipt Engine
=================================================================
Called by app.py's /api/download_receipt/<order_id> route.
Receives a data dict, draws an A4 light-theme receipt using fpdf2,
and writes it to a temp file that Flask streams back to the browser.

INSTALL:  pip install fpdf2
CALL:     generate_receipt(data, output_path)

DATA DICT KEYS (all strings unless noted):
  order_id       e.g. "ORD125"
  user_id        e.g. "U001"
  full_name      e.g. "Yashwanth A"
  user_phone     e.g. "6382717541"
  user_email     e.g. "yash@gmail.com"
  address        e.g. "No 11 - Flat No 11,Elumalai Street,West Tambaram,600045"
  slot           e.g. "Evening"
  status         e.g. "Delivered"
  timestamp      e.g. "2026-04-12 17:35:24"
  boy_name       e.g. "Suresh"
  boy_phone      e.g. "9876543211"
  total          float  e.g. 500.0
  items_string   e.g. "V1002:Small Onion:10000:50.00,VF101:Curry Leaves:50:0.00"

ITEMS_STRING FORMAT: veg_id:name:qty_g:price_per_1000g  (4-part, comma-separated)
  - If price_per_1000g == 0.0  → item is FREE
  - VF-prefixed ids            → always FREE

Team: CodeCrafters | Project: Fresh Picks | SDP-1
"""

from fpdf import FPDF
from datetime import datetime

# ─────────────────────────────────────────────────────────────
# BRAND COLOURS  (light theme — matches the PDF screenshot)
# ─────────────────────────────────────────────────────────────
C_WHITE      = (255, 255, 255)
C_BLACK      = ( 30,  30,  30)   # #1e1e1e  — body text
C_ACCENT     = (  0, 122, 204)   # #007acc  — brand blue
C_LABEL      = (136, 136, 136)   # #888888  — section labels
C_BORDER     = (220, 220, 220)   # #dcdcdc  — dividers
C_GREEN      = ( 40, 167,  69)   # #28a745  — FREE badge / success
C_GREEN_BG   = (235, 248, 238)   # free badge background
C_HEADER_BG  = (  0, 122, 204)   # table header blue fill
C_ROW_ALT    = (248, 249, 252)   # alternating row tint
C_STATUS_BG  = (235, 248, 238)   # delivered status bg
C_BOY_BG     = (235, 244, 255)   # delivery partner box bg
C_BOY_BORDER = (  0, 122, 204)   # delivery partner box border


def _rgb(color_tuple):
    """Unpack an (R,G,B) tuple into three args for fpdf set_* calls."""
    return color_tuple


def _parse_timestamp(ts: str) -> str:
    """Convert '2026-04-12 17:35:24' → '12 Apr 2026, 5:35 pm'"""
    try:
        dt = datetime.strptime(ts.strip(), "%Y-%m-%d %H:%M:%S")
        return dt.strftime("%-d %b %Y, %-I:%M %p").lower().replace("am", "am").replace("pm", "pm")
    except Exception:
        return ts


def _parse_items(items_string: str):
    """
    Parse items_string into a list of dicts.
    Each part: veg_id:name:qty_g:price_per_1000g
    Supports both 4-part (with name) and 3-part (legacy, no name).
    Returns list of:
      { name, qty_g, qty_kg_str, rate_str, amount_str, is_free }
    """
    items = []
    if not items_string or not items_string.strip():
        return items

    for part in items_string.split(","):
        segments = [s.strip() for s in part.strip().split(":")]
        if len(segments) < 3 or not segments[0]:
            continue

        veg_id = segments[0]

        if len(segments) >= 4:
            name        = segments[1]
            qty_g       = int(segments[2])   if segments[2].isdigit() else 0
            price_snap  = float(segments[3]) if segments[3] else 0.0
        else:
            # Legacy 3-part: veg_id:qty_g:price
            name        = veg_id
            qty_g       = int(segments[1])   if segments[1].isdigit() else 0
            price_snap  = float(segments[2]) if segments[2] else 0.0

        is_free = (price_snap == 0.0) or veg_id.startswith("VF")

        qty_kg     = qty_g / 1000.0
        qty_str    = f"{qty_kg:.2f} kg" if qty_kg >= 1 else f"{qty_g} g"
        rate_str   = "—" if is_free else f"Rs.{price_snap:.2f}/kg"
        amount     = (price_snap * qty_g) / 1000.0 if not is_free else 0.0
        amount_str = "Rs.0.00" if is_free else f"Rs.{amount:.2f}"

        items.append({
            "name":       name,
            "qty_g":      qty_g,
            "qty_str":    qty_str,
            "rate_str":   rate_str,
            "amount":     amount,
            "amount_str": amount_str,
            "is_free":    is_free,
        })

    return items


class ReceiptPDF(FPDF):
    """Custom FPDF subclass for the FreshPicks A4 receipt."""

    def header(self):
        pass  # We draw our own header in generate_receipt()

    def footer(self):
        pass  # We draw our own footer


def generate_receipt(data: dict, output_path: str) -> None:
    """
    Draw the complete receipt and save to output_path.

    :param data:        dict with all order + user fields (see module docstring)
    :param output_path: absolute file path where the PDF will be written
    """

    pdf = ReceiptPDF(orientation="P", unit="mm", format="A4")
    pdf.set_auto_page_break(auto=False)
    pdf.add_page()

    # Page margins
    LEFT   = 15
    RIGHT  = 195          # 210 - 15
    W      = RIGHT - LEFT  # usable width = 180mm
    pdf.set_margins(LEFT, 10, 15)

    # ── Fonts ────────────────────────────────────────────────
    # fpdf2 ships with Helvetica (ASCII-safe) built-in.
    # Rupee sign (₹) is not in standard Latin-1 — we use "Rs." instead.
    FONT = "Helvetica"

    y = 15  # current vertical cursor

    # ─────────────────────────────────────────────────────────
    # SECTION 1 — HEADER  (logo left | ORDER RECEIPT chip right)
    # ─────────────────────────────────────────────────────────

    # Basket emoji via Unicode fallback text
    pdf.set_xy(LEFT, y)
    pdf.set_font(FONT, "B", 18)
    pdf.set_text_color(*C_ACCENT)
    pdf.cell(0, 8, "Fresh Picks", ln=False)

    # "ORDER RECEIPT" label (top-right)
    pdf.set_font(FONT, "", 7)
    pdf.set_text_color(*C_LABEL)
    pdf.set_xy(LEFT, y)
    pdf.cell(W, 5, "ORDER RECEIPT", align="R", ln=True)

    y += 8

    # Order-ID chip (rounded box, top-right)
    chip_w = 30
    chip_h =  8
    chip_x = RIGHT - chip_w
    chip_y = y - 3
    pdf.set_fill_color(*C_WHITE)
    pdf.set_draw_color(*C_ACCENT)
    pdf.set_line_width(0.5)
    pdf.rect(chip_x, chip_y, chip_w, chip_h, style="D")
    pdf.set_xy(chip_x, chip_y + 1)
    pdf.set_font(FONT, "B", 10)
    pdf.set_text_color(*C_ACCENT)
    pdf.cell(chip_w, chip_h - 2, data.get("order_id", ""), align="C")

    # Timestamp (below chip)
    ts_display = _parse_timestamp(data.get("timestamp", ""))
    pdf.set_xy(LEFT, chip_y + chip_h + 1)
    pdf.set_font(FONT, "", 7)
    pdf.set_text_color(*C_LABEL)
    pdf.cell(W, 4, ts_display, align="R", ln=True)

    # Sub-header: address / GSTIN / contact
    pdf.set_xy(LEFT, y)
    pdf.set_font(FONT, "", 7.5)
    pdf.set_text_color(*C_LABEL)
    sub_lines = [
        "123 Grocery Ave, Market City, Chennai - 600045",
        "GSTIN: 22AAAAA0000A1Z5  |  FSSAI: 10020042004971",
        "+91 98765 43210  |  support@freshpicks.in",
    ]
    for sl in sub_lines:
        pdf.set_x(LEFT)
        pdf.cell(0, 4, sl, ln=True)
        y += 4

    y += 4

    # ─────────────────────────────────────────────────────────
    # DASHED DIVIDER
    # ─────────────────────────────────────────────────────────
    def dashed_line(y_pos):
        pdf.set_draw_color(*C_BORDER)
        pdf.set_line_width(0.2)
        x = LEFT
        while x < RIGHT:
            pdf.line(x, y_pos, min(x + 3, RIGHT), y_pos)
            x += 5

    dashed_line(y)
    y += 5

    # ─────────────────────────────────────────────────────────
    # SECTION 2 — BILL TO / DELIVERY SLOT / STATUS  (3-column grid)
    # ─────────────────────────────────────────────────────────
    col_w = W / 3

    def section_label(x, y_pos, text):
        pdf.set_xy(x, y_pos)
        pdf.set_font(FONT, "B", 6.5)
        pdf.set_text_color(*C_LABEL)
        # uppercase label
        pdf.cell(col_w, 4, text.upper(), ln=False)

    def section_value(x, y_pos, text, color=C_BLACK, bold=False, size=10):
        pdf.set_xy(x, y_pos)
        pdf.set_font(FONT, "B" if bold else "", size)
        pdf.set_text_color(*color)
        pdf.cell(col_w, 5, text, ln=False)

    # BILL TO
    section_label(LEFT, y, "Bill To")
    section_label(LEFT + col_w, y, "Delivery Slot")
    section_label(LEFT + col_w * 2, y, "Status")
    y += 5

    section_value(LEFT, y, data.get("full_name", ""), bold=True, size=11)
    section_value(LEFT + col_w, y, data.get("slot", ""), bold=True, size=11)

    # Status badge
    status = data.get("status", "")
    sx = LEFT + col_w * 2
    badge_w = 35
    badge_h = 7
    if "deliver" in status.lower():
        pdf.set_fill_color(*C_STATUS_BG)
        pdf.set_draw_color(*C_GREEN)
    else:
        pdf.set_fill_color(255, 248, 230)
        pdf.set_draw_color(255, 165, 0)
    pdf.set_line_width(0.4)
    pdf.rect(sx, y - 1, badge_w, badge_h, style="FD")
    pdf.set_xy(sx, y)
    pdf.set_font(FONT, "B", 8)
    pdf.set_text_color(*C_GREEN if "deliver" in status.lower() else (200, 100, 0))
    check = "checkmark " if "deliver" in status.lower() else ""
    pdf.cell(badge_w, 5, f"v {status.upper()}", align="C")

    y += 10

    # ─────────────────────────────────────────────────────────
    # SECTION 3 — CONTACT DETAILS
    # ─────────────────────────────────────────────────────────
    pdf.set_xy(LEFT, y)
    pdf.set_font(FONT, "B", 6.5)
    pdf.set_text_color(*C_LABEL)
    pdf.cell(0, 4, "CONTACT", ln=True)
    y += 4

    # Phone (accent blue)
    pdf.set_xy(LEFT, y)
    pdf.set_font(FONT, "B", 9)
    pdf.set_text_color(*C_ACCENT)
    pdf.cell(0, 5, data.get("user_phone", ""), ln=True)
    y += 5

    # Email
    pdf.set_xy(LEFT, y)
    pdf.set_font(FONT, "", 8.5)
    pdf.set_text_color(*C_BLACK)
    pdf.cell(0, 4, data.get("user_email", ""), ln=True)
    y += 5

    # Address — 4 lines, each part separated by comma
    address_raw = data.get("address", "")
    addr_parts  = [p.strip() for p in address_raw.split(",")]
    # Pad to 4 parts
    while len(addr_parts) < 4:
        addr_parts.append("")

    pdf.set_font(FONT, "", 8)
    pdf.set_text_color(*C_LABEL)
    for i, part in enumerate(addr_parts[:4]):
        pdf.set_xy(LEFT, y)
        # All but last get a trailing comma
        display = part + ("," if i < 3 else "")
        pdf.cell(0, 4, display, ln=True)
        y += 4

    y += 4

    # ─────────────────────────────────────────────────────────
    # SECTION 4 — DELIVERY PARTNER BOX
    # ─────────────────────────────────────────────────────────
    box_h = 18
    pdf.set_fill_color(*C_BOY_BG)
    pdf.set_draw_color(*C_BOY_BORDER)
    pdf.set_line_width(0.6)
    pdf.rect(LEFT, y, W, box_h, style="FD")

    # Label
    pdf.set_xy(LEFT + 3, y + 2)
    pdf.set_font(FONT, "B", 6.5)
    pdf.set_text_color(*C_LABEL)
    pdf.cell(0, 4, "DELIVERY PARTNER")

    # Scooter emoji substitute
    pdf.set_xy(LEFT + 3, y + 7)
    pdf.set_font(FONT, "B", 14)
    pdf.set_text_color(*C_ACCENT)
    pdf.cell(10, 6, ">")   # stylised arrow as scooter substitute

    # Boy name
    pdf.set_xy(LEFT + 14, y + 7)
    pdf.set_font(FONT, "B", 11)
    pdf.set_text_color(*C_BLACK)
    pdf.cell(0, 5, data.get("boy_name", ""))

    # Boy phone
    pdf.set_xy(LEFT + 14, y + 13)
    pdf.set_font(FONT, "", 8.5)
    pdf.set_text_color(*C_ACCENT)
    pdf.cell(0, 4, data.get("boy_phone", ""))

    y += box_h + 6

    # ─────────────────────────────────────────────────────────
    # SECTION 5 — ITEMS TABLE
    # ─────────────────────────────────────────────────────────
    items = _parse_items(data.get("items_string", ""))

    # Column widths  (#  | Item | Rate | Qty | Amount)
    COL = [8, 85, 35, 27, 25]   # total = 180mm = W

    # Table header row
    pdf.set_fill_color(*C_HEADER_BG)
    pdf.set_draw_color(*C_HEADER_BG)
    pdf.set_line_width(0)
    pdf.rect(LEFT, y, W, 8, style="F")

    pdf.set_font(FONT, "B", 8)
    pdf.set_text_color(*C_WHITE)
    pdf.set_xy(LEFT, y + 1)

    headers = ["#", "ITEM", "RATE", "QTY", "AMOUNT"]
    aligns  = ["C", "L",    "R",    "R",   "R"]
    x = LEFT
    for i, (h, cw, al) in enumerate(zip(headers, COL, aligns)):
        pdf.set_xy(x + (1 if al == "L" else 0), y + 1)
        pdf.cell(cw, 6, h, align=al)
        x += cw

    y += 8

    # Item rows
    pdf.set_line_width(0.2)
    pdf.set_draw_color(*C_BORDER)

    for row_num, item in enumerate(items, start=1):
        row_h = 8
        # Alternating fill
        if row_num % 2 == 0:
            pdf.set_fill_color(*C_ROW_ALT)
            pdf.rect(LEFT, y, W, row_h, style="F")

        # Bottom border
        pdf.line(LEFT, y + row_h, RIGHT, y + row_h)

        # # column
        pdf.set_xy(LEFT, y + 1)
        pdf.set_font(FONT, "", 8)
        pdf.set_text_color(*C_LABEL)
        pdf.cell(COL[0], 6, str(row_num), align="C")

        # Name column
        name_x = LEFT + COL[0]
        pdf.set_xy(name_x + 1, y + 1)
        pdf.set_font(FONT, "B" if not item["is_free"] else "", 8.5)
        pdf.set_text_color(*C_BLACK)
        pdf.cell(COL[1] - 20, 6, item["name"])

        # FREE badge inline
        if item["is_free"]:
            badge_x = name_x + 1 + pdf.get_string_width(item["name"]) + 2
            bw, bh = 12, 5
            pdf.set_fill_color(*C_GREEN_BG)
            pdf.set_draw_color(*C_GREEN)
            pdf.set_line_width(0.3)
            pdf.rect(badge_x, y + 2, bw, bh, style="FD")
            pdf.set_xy(badge_x, y + 2)
            pdf.set_font(FONT, "B", 6)
            pdf.set_text_color(*C_GREEN)
            pdf.cell(bw, bh, "FREE", align="C")

        # Rate column
        x = LEFT + COL[0] + COL[1]
        pdf.set_xy(x, y + 1)
        pdf.set_font(FONT, "", 8)
        pdf.set_text_color(*C_BLACK if not item["is_free"] else C_LABEL)
        pdf.cell(COL[2], 6, item["rate_str"], align="R")

        # Qty column
        x += COL[2]
        pdf.set_xy(x, y + 1)
        pdf.set_text_color(*C_BLACK)
        pdf.cell(COL[3], 6, item["qty_str"], align="R")

        # Amount column
        x += COL[3]
        pdf.set_xy(x, y + 1)
        pdf.set_font(FONT, "B", 8.5)
        pdf.set_text_color(*C_GREEN if item["is_free"] else C_ACCENT)
        pdf.cell(COL[4], 6, item["amount_str"], align="R")

        y += row_h

    y += 4

    # ─────────────────────────────────────────────────────────
    # SECTION 6 — TOTALS
    # ─────────────────────────────────────────────────────────
    total = float(data.get("total", 0))

    def total_row(label, value, bold=False, color=C_BLACK):
        pdf.set_xy(LEFT, y)
        pdf.set_font(FONT, "B" if bold else "", 9 if not bold else 10)
        pdf.set_text_color(*C_LABEL)
        pdf.cell(W - 40, 6, label)
        pdf.set_text_color(*color)
        pdf.set_font(FONT, "B" if bold else "", 9 if not bold else 11)
        pdf.cell(40, 6, value, align="R", ln=True)

    total_row("Subtotal",         f"Rs.{total:.2f}")
    total_row("Delivery Charges", "FREE", color=C_GREEN)

    # Separator before grand total
    y = pdf.get_y() + 1
    pdf.set_draw_color(*C_BORDER)
    pdf.set_line_width(0.4)
    pdf.line(LEFT, y, RIGHT, y)
    y += 2
    pdf.set_y(y)

    # Grand total
    pdf.set_font(FONT, "B", 12)
    pdf.set_text_color(*C_BLACK)
    pdf.set_x(LEFT)
    pdf.cell(W - 40, 7, "Total Paid")
    pdf.set_text_color(*C_ACCENT)
    pdf.cell(40, 7, f"Rs.{total:.2f}", align="R", ln=True)

    y = pdf.get_y() + 6

    # ─────────────────────────────────────────────────────────
    # SECTION 7 — FOOTER
    # ─────────────────────────────────────────────────────────
    dashed_line(y)
    y += 5

    pdf.set_xy(LEFT, y)
    pdf.set_font(FONT, "", 8)
    pdf.set_text_color(*C_LABEL)
    pdf.multi_cell(W, 4,
        "Thank you for shopping with FreshPicks!\n"
        "This is a computer-generated receipt. No signature required.",
        align="C"
    )
    y = pdf.get_y() + 3

    # Receipt meta line
    order_id  = data.get("order_id", "")
    date_part = data.get("timestamp", "")[:10]
    pdf.set_xy(LEFT, y)
    pdf.set_font(FONT, "", 6.5)
    pdf.set_text_color(*C_LABEL)
    pdf.cell(W, 4,
        f"Receipt ID: {order_id}   Generated: {date_part}   CodeCrafters SDP-1",
        align="C"
    )

    # ── Write file ───────────────────────────────────────────
    pdf.output(output_path)
