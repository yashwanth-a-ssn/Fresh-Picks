"""
app.py - Fresh Picks: Main Flask Application (v3 — HTTPS/SSL Edition)
======================================================================

DEVELOPER NOTE: SSL, HTTPS & MULTI-DEVICE SETUP

WHY HTTPS ON COLLEGE WI-FI?
─────────────────────────
    HTTP sends data as plain text. On a shared Wi-Fi network
    (like a college lab or hotspot), anyone on the same network
    can run a packet-sniffer (e.g. Wireshark) and read:
    username=alice&password=Alice@123

    HTTPS encrypts everything using SSL/TLS so it becomes:
    Gx92#@!k%LpQ... (unreadable without our key.pem)

SELF-SIGNED CERTIFICATE WARNING:
───────────────────────────────
    When you open https://<ip>:5000 on any browser, you will see:
    "Your connection is not private"
    NET::ERR_CERT_AUTHORITY_INVALID

    This is EXPECTED. Our cert.pem is "self-signed" — we created
    it ourselves rather than paying a Certificate Authority (CA)
    like DigiCert or Let's Encrypt. Browsers don't trust unknown
    issuers by default, so they warn you.

HOW TO BYPASS (for demo):
    Chrome/Edge: Click "Advanced" -> "Proceed to <IP> (unsafe)"
    Firefox:     Click "Advanced" -> "Accept the Risk"
    (Do this once per device per session.)

PORT 5000 - WHY THIS NUMBER?
────────────────────────────
    Think of your laptop's IP address as a building address.
    Ports are "apartment numbers" inside that building.
        Port  80  -> Standard HTTP  (web browsers default)
        Port 443  -> Standard HTTPS (web browsers default)
        Port 5000 -> Flask's development default

    Ports below 1024 need admin/root privileges on most OSes.
    Port 5000 is above that threshold, so Flask can use it freely
    without elevated permissions. That's why it's the standard
    Flask dev port.

HOW TO CONNECT FROM A TEAMMATE'S DEVICE:
────────────────────────────────────────
    Step 1 - Find the host laptop's IPv4 address:
    Windows:  Open CMD -> type: ipconfig
                Look for: "IPv4 Address . . . : 192.168.x.x"
    Mac/Linux: Open Terminal -> type: ifconfig
                Look for: "inet 192.168.x.x" under en0 or wlan0

    Step 2 - Make sure VERSION B (host="0.0.0.0") is active.

    Step 3 - On the teammate's device, open a browser and go to:
    https://192.168.x.x:5000
    (Replace 192.168.x.x with the actual IP from Step 1)

    Step 4 - Accept the self-signed cert warning (Advanced ->
    Proceed) and the app will load!

    IMPORTANT: All devices must be on the SAME Wi-Fi network.
    The laptop running app.py and the phone/laptop visiting
    the site must share the same router/hotspot.

======================================================================

ROUTES:
  GET  /                        -> Landing page (index.html)
  GET  /login                   -> Login page
  GET  /register                -> Register page
  POST /api/login               -> Validate login via C binary
  POST /api/register            -> Register new user via C binary
  GET  /user_home               -> User dashboard (requires login)
  GET  /admin_dash              -> Admin dashboard (requires admin login)
  GET  /profile                 -> User profile page
  GET  /security                -> Security/Change Password page (Module 5)
  POST /api/get_profile         -> Fetch profile data via C binary
  POST /api/change_password     -> Unified password change (user OR admin)
  GET  /logout                  -> Clear session and redirect to home

HOW TO RUN:
  1. python generate_certs.py   (once, to create cert.pem + key.pem)
  2. python app.py              (every time to start the server)

Team: CodeCrafters | Project: Fresh Picks | SDP-1
"""

import ssl   # Python's built-in SSL module for HTTPS configuration.
             # ssl.SSLContext wraps our cert.pem + key.pem into a
             # configuration object that Flask/Werkzeug understands.
import os    # Used to build file paths to cert.pem and key.pem.

from datetime import datetime

from flask import (
    Flask,              # Creates and initializes the web application
    render_template,    # Renders HTML templates for frontend pages
    request,            # Retrieves incoming data from client requests
    jsonify,            # Converts data into JSON HTTP responses
    session,            # Stores user session data across requests
    redirect,           # Redirects user to a different route
    url_for,            # Generates dynamic URLs for application routes
    send_from_directory # Safely serves files from a specified folder (e.g., images, uploads, static files)
)

from bridge import run_c_binary  # Our reusable C-caller function
# Executes C backend binary and returns output

# ─────────────────────────────────────────────────────────────
# Flask App Setup
# ─────────────────────────────────────────────────────────────
app = Flask(
    __name__,
    template_folder = "../templates",   # HTML files are in /templates/
    static_folder   = "../static"       # CSS/JS files are in /static/
)

# Secret key for session encryption.
# In production, use a long random string from os.urandom(24).
app.secret_key = "fresh_picks_secret_codecrafters_2026"

# ─────────────────────────────────────────────────────────────
# SSL CERTIFICATE PATHS
# These files are generated by generate_certs.py (run it once).
# We build the path relative to THIS file (app.py) so it works
# regardless of which directory you launch the server from.
#
#   os.path.abspath(__file__)  -> full path to app.py itself
#   os.path.dirname(...)       -> folder that contains app.py
#   os.path.join(..., "x.pem")-> full path to cert/key in same folder
# ─────────────────────────────────────────────────────────────
_HERE     = os.path.dirname(os.path.abspath(__file__))
CERT_FILE = os.path.join(_HERE, "cert.pem")  # Public certificate
KEY_FILE  = os.path.join(_HERE, "key.pem")   # Private key — keep secret!

# Helper Function
def _safe_float(value, default=0.0):
    try:
        return float(value)
    except (ValueError, TypeError):
        return default
# =============================================================
# PAGE ROUTES - These serve HTML pages
# =============================================================

@app.route("/")
def index():
    # Only shows User Login & Registration (Admin abstracted)
    return render_template("index.html")

@app.route("/login/<role>")
def login_page(role):
    # Unified Login: handles both /login/user and /login/admin
    if role not in ["user", "admin"]:
        return redirect("/")
    
    # Auto-redirect if already logged in
    if session.get("role") == role:
        return redirect("/admin_dash" if role == "admin" else "/user_home")
        
    return render_template("login.html", role=role)

@app.route("/register")
def register():
    return render_template("register.html")

@app.route("/user_home")
def user_home():
    if session.get("role") != "user": return redirect("/login/user")
    return render_template("user_home.html", username=session.get("username"))

@app.route("/admin_dash")
def admin_dash():
    if session.get("role") != "admin": return redirect("/login/admin")
    return render_template("admin_dash.html", username=session.get("username"), admin_name=session.get("admin_name", "Admin"))

@app.route("/profile")
def profile():
    if session.get("role") != "user": return redirect("/login/user")
    return render_template("profile.html")

@app.route("/security")
def security():
    if "user_id" not in session: return redirect("/")
    return render_template("security.html")

@app.route("/logout")
def logout():
    session.clear()
    return redirect("/")

@app.route("/shop")
def shop():
    # Renamed from /vegetables
    if session.get("role") != "user": return redirect("/login/user")
    return render_template("shop.html")

@app.route("/cart")
def cart():
    if session.get("role") != "user": return redirect("/login/user")
    return render_template("cart.html")

@app.route("/user_orders")
def user_orders_page():
    if session.get("role") != "user": return redirect("/login/user")
    return render_template("user_orders.html", username=session.get("username"))

@app.route("/admin_inventory")
def admin_inventory():
    if session.get("role") != "admin": return redirect("/login/admin")
    
    veg_result = run_c_binary("order", ["list_products"])
    vegetables = []
    if veg_result["status"] == "SUCCESS":
        raw_lines = veg_result["raw_output"].strip().split("\n")
        for line in raw_lines[1:]:
            parts = line.split("|")
            if len(parts) >= 7:
                vegetables.append({
                    "veg_id": parts[0], "category": parts[1], "name": parts[2],
                    "stock_g": int(parts[3]), "price_per_1000g": float(parts[4]),
                    "tag": parts[5], "validity_days": int(parts[6])
                })

    promo_result = run_c_binary("inventory", ["list_promo"])
    free_items = []
    if promo_result["status"] == "SUCCESS":
        raw_lines = promo_result["raw_output"].strip().split("\n")
        for line in raw_lines[1:]:
            parts = line.split("|")
            if len(parts) >= 5:
                free_items.append({
                    "vf_id": parts[0], "name": parts[1], "stock_g": int(parts[2]),
                    "min_trigger_amt": float(parts[3]), "free_qty_g": int(parts[4])
                })

    return render_template("admin_inventory.html", vegetables=vegetables, free_items=free_items, admin_name=session.get("admin_name", "Admin"))

@app.route("/admin_orders")
def admin_orders():
    if session.get("role") != "admin": return redirect("/login/admin")
    
    current_hour = datetime.now().hour
    if   7 <= current_hour <= 10: active_slot = "Morning"
    elif 12 <= current_hour <= 14: active_slot = "Afternoon"
    elif 17 <= current_hour <= 19: active_slot = "Evening"
    else: active_slot = None

    jit_promoted = 0
    if active_slot:
        jit_result = run_c_binary("order", ["batch_promote_slot", active_slot])
        if jit_result["status"] == "SUCCESS":
            try: jit_promoted = int(jit_result["data"].strip())
            except ValueError: jit_promoted = 0

    return render_template("admin_orders.html", username=session.get("username"), admin_name=session.get("admin_name", "Admin"), jit_promoted=jit_promoted, jit_slot=active_slot or "")

@app.route('/product_images/<filename>')
def serve_product_image(filename):
    # Image Bridge to /images/ root folder
    root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    return send_from_directory(os.path.join(root_dir, "images"), filename)

# =============================================================
# API ROUTES - These return JSON, called by JavaScript fetch()
#
# RELATIVE PATHS — WHY THIS MATTERS FOR HTTPS:
#   All fetch() calls in our HTML/JS files use relative paths:
#     fetch("/api/login", ...)              <- CORRECT
#     fetch("http://localhost/api/login")   <- WRONG
#
#   A hardcoded "http://..." URL causes a "Mixed Content" error
#   when the page is served over HTTPS:
#     "Blocked: The page was loaded over HTTPS, but requested
#      an insecure HTTP resource."
#
#   Relative paths inherit the current scheme+host automatically:
#     https://192.168.1.5:5000/api/login  (Wi-Fi demo mode)
#     https://localhost:5000/api/login    (local testing mode)
#   No code changes needed when switching between modes.
# =============================================================

@app.route("/api/login", methods=["POST"])
def api_login():
    data = request.get_json()
    username, password, role = data.get("username", "").strip(), data.get("password", "").strip(), data.get("role", "user").strip()

    if not username or not password: return jsonify({"status": "ERROR", "message": "Required fields missing"})

    if role == "admin": result = run_c_binary("auth", ["login_admin", username, password])
    else:               result = run_c_binary("auth", ["login_user", username, password])

    if result["status"] == "SUCCESS":
        session["role"], session["username"] = role, username
        if role == "admin":
            admin_parts = result["data"].split("|")
            session["user_id"] = admin_parts[0] if len(admin_parts) > 0 else "A1001"
            session["admin_name"] = admin_parts[1] if len(admin_parts) > 1 else "Admin"
        else:
            session["user_id"] = result["data"]

        return jsonify({"status": "SUCCESS", "role": role, "redirect": "/admin_dash" if role == "admin" else "/user_home"})
    return jsonify({"status": "ERROR", "message": result["data"]})

@app.route("/api/register", methods=["POST"])
def api_register():
    data = request.get_json()
    u, p, fn, e, ph = data.get("username", ""), data.get("password", ""), data.get("full_name", ""), data.get("email", ""), data.get("phone", "")
    address = f"{data.get('door','')},{data.get('street','')},{data.get('area','')},{data.get('pincode','')}"

    if not all([u, p, fn, e, ph, data.get('door'), data.get('street'), data.get('area'), data.get('pincode')]):
        return jsonify({"status": "ERROR", "message": "All fields required"})

    result = run_c_binary("auth", ["register", u, p, fn, e, ph, address])
    return jsonify({"status": result["status"], "message": "Registration successful" if result["status"] == "SUCCESS" else result["data"]})

@app.route("/api/get_profile", methods=["POST"])
def api_get_profile():
    if "user_id" not in session: return jsonify({"status": "ERROR", "message": "Not logged in"})
    result = run_c_binary("auth", ["get_profile", session["user_id"]])
    if result["status"] == "SUCCESS":
        parts = result["data"].split("|")
        return jsonify({"status": "SUCCESS", "user_id": parts[0], "username": parts[1], "full_name": parts[2], "email": parts[3], "phone": parts[4], "address": parts[5]})
    return jsonify({"status": "ERROR", "message": result["data"]})

@app.route("/api/change_password", methods=["POST"])
def api_change_password():
    if "user_id" not in session: return jsonify({"status": "ERROR", "message": "Not logged in"})
    data = request.get_json()
    old_p, new_p = data.get("old_password", "").strip(), data.get("new_password", "").strip()
    
    if session.get("role") == "admin": result = run_c_binary("auth", ["change_pass_admin", session["user_id"], old_p, new_p])
    else:                              result = run_c_binary("auth", ["change_pass_user", session["user_id"], old_p, new_p])
    return jsonify({"status": result["status"], "message": result["data"]})

@app.route("/api/list_products", methods=["GET"])
def api_list_products():
    result = run_c_binary("order", ["list_products"])
    if result["status"] != "SUCCESS": return jsonify({"status": "ERROR", "message": result["data"]})
    
    products = []
    for line in result["raw_output"].strip().split("\n")[1:]:
        parts = line.strip().split("|")
        if len(parts) >= 7: products.append({"veg_id": parts[0], "category": parts[1], "name": parts[2], "stock_g": int(parts[3]), "price_per_1000g": float(parts[4]), "tag": parts[5], "validity_days": int(parts[6])})
    return jsonify({"status": "SUCCESS", "products": products})

@app.route("/api/update_stock", methods=["POST"])
def api_update_stock():
    if session.get("role") != "admin":
        return jsonify({"status": "ERROR", "message": "Admin only"})
    data = request.get_json()
    veg_id   = data.get("veg_id",   "").strip()
    stock_g  = int(data.get("stock_g",  0))
    price    = float(data.get("price",  0))
    validity = int(data.get("validity", 1))
    if not veg_id:
        return jsonify({"status": "error", "message": "veg_id required"})
    result = run_c_binary("inventory", [
        "update_stock", veg_id, str(stock_g), str(price), str(validity)
    ])
    return jsonify({
        "status":  "SUCCESS" if result["status"] == "SUCCESS" else "ERROR",
        "message": result["data"]
    })

@app.route("/api/update_promo_stock", methods=["POST"])
def api_update_promo_stock():
    if session.get("role") != "admin":
        return jsonify({"status": "ERROR", "message": "Admin only"})
    data    = request.get_json()
    vf_id   = data.get("vf_id",   "").strip()
    stock_g = int(data.get("stock_g", 0))
    if not vf_id:
        return jsonify({"status": "error", "message": "vf_id required"})
    result = run_c_binary("inventory", ["update_promo_stock", vf_id, str(stock_g)])
    return jsonify({
        "status":  "success" if result["status"] == "SUCCESS" else "error",
        "message": result["data"]
    })

@app.route("/api/add_to_cart", methods=["POST"])
def api_add_to_cart():
    if "user_id" not in session: return jsonify({"status": "ERROR", "message": "Not logged in"})
    data = request.get_json()
    result = run_c_binary("order", ["add_to_cart", session["user_id"], data.get("veg_id", "").strip(), str(int(data.get("qty_g", 0)))])
    return jsonify({"status": result["status"], "message": result["data"]})

@app.route("/api/view_cart", methods=["POST"])
def api_view_cart():
    if "user_id" not in session: return jsonify({"status": "ERROR", "message": "Not logged in"})
    result = run_c_binary("order", ["view_cart", session["user_id"]])
    if result["status"] != "SUCCESS": return jsonify({"status": "ERROR", "message": result["data"]})
    
    lines = result["raw_output"].strip().split("\n")
    items = []
    for line in lines[1:]:
        parts = line.strip().split("|")
        if len(parts) >= 6: items.append({"veg_id": parts[0], "name": parts[1], "qty_g": int(parts[2]), "price_per_1000g": float(parts[3]), "item_total": float(parts[4]), "is_free": int(parts[5])})
    return jsonify({"status": "SUCCESS", "total": float(lines[0].split("|")[1]), "items": items})

@app.route("/api/update_cart_qty", methods=["POST"])
def api_update_cart_qty():
    """
    Called by onQtyChange debounce in cart.html.
    Updates one item's quantity in the user's cart DLL (re-writes cart file via C).
    Body: { "veg_id": "V1001", "qty_g": 750 }
    """
    if "user_id" not in session:
        return jsonify({"status": "ERROR", "message": "Not logged in"})

    data   = request.get_json()
    veg_id = data.get("veg_id", "").strip()
    qty_g  = int(data.get("qty_g", 0))

    if not veg_id or qty_g <= 0:
        return jsonify({"status": "ERROR", "message": "Invalid veg_id or qty_g"})

    # Re-uses the existing add_to_cart C command which does update_or_append
    result = run_c_binary("order", [
        "add_to_cart", session["user_id"], veg_id, str(qty_g)
    ])
    return jsonify({
        "status":  result["status"],
        "message": result["data"]
    })

@app.route("/api/remove_item", methods=["POST"])
def api_remove_item():
    if "user_id" not in session: return jsonify({"status": "ERROR", "message": "Not logged in"})
    result = run_c_binary("order", ["remove_item", session["user_id"], request.get_json().get("veg_id", "").strip()])
    return jsonify({"status": result["status"], "message": result["data"]})

@app.route("/api/checkout", methods=["POST"])
def api_checkout():
    if "user_id" not in session: return jsonify({"status": "ERROR", "message": "Not logged in"})
    slot = request.get_json().get("delivery_slot", "").strip()
    if slot not in ["Morning", "Afternoon", "Evening"]: return jsonify({"status": "ERROR", "message": "Invalid slot"})
    
    result = run_c_binary("order", ["checkout", session["user_id"], slot])
    if result["status"] != "SUCCESS": return jsonify({"status": "ERROR", "message": result["data"]})
    
    parts = result["raw_output"].strip().split("\n")[0].split("|")
    return jsonify({"status": "SUCCESS", "order_id": parts[1], "total": float(parts[2]), "slot": parts[3], "boy_name": parts[4], "boy_phone": parts[5], "items": parts[6] if len(parts)>6 else ""})

@app.route("/api/get_user_orders", methods=["POST"])
def api_get_user_orders():
    # Kept as POST per your request
    if "user_id" not in session: return jsonify({"status": "ERROR", "message": "Not logged in"})
    result = run_c_binary("order", ["get_orders", session["user_id"]])
    if result["status"] != "SUCCESS": return jsonify({"status": "ERROR", "message": result["data"]})
    
    orders = []
    for line in result["raw_output"].strip().split("\n")[1:]:
        parts = line.strip().split("|")
        if len(parts) >= 8: orders.append({"order_id": parts[0], "user_id": parts[1], "total_amount": float(parts[2]), "delivery_slot": parts[3], "delivery_boy_id": parts[4], "status": parts[5], "timestamp": parts[6], "items_string": parts[7]})
    return jsonify({"status": "SUCCESS", "orders": orders})

@app.route("/api/admin_orders", methods=["GET"])
def api_admin_orders():
    if session.get("role") != "admin": return jsonify({"status": "ERROR", "message": "Admin only"})
    result = run_c_binary("order", ["admin_orders"])
    if result["status"] != "SUCCESS": return jsonify({"status": "ERROR", "message": result["data"]})
    
    orders = []
    for line in result["raw_output"].strip().split("\n")[1:]:
        parts = line.strip().split("|")
        if len(parts) >= 8: orders.append({"order_id": parts[0], "user_id": parts[1], "total_amount": float(parts[2]), "delivery_slot": parts[3], "delivery_boy_id": parts[4], "status": parts[5], "timestamp": parts[6], "items": parts[7], "boy_name": parts[8] if len(parts)>8 else "Unknown", "boy_phone": parts[9] if len(parts)>9 else "N/A"})
    return jsonify({"status": "SUCCESS", "orders": orders})

@app.route("/api/get_admin_orders", methods=["POST"])
def api_get_admin_orders():
    """
    POST /api/get_admin_orders
    Called by admin_orders.html on page load.
    NOW delegates to: ./delivery list_all_orders
    (previously called ./order admin_orders which was heap-sorted + active-only)

    Returns all orders, newest-first, with boy enrichment.
    """
    if session.get("role") != "admin":
        return jsonify({"status": "ERROR", "message": "Admin access required"}), 403

    result = run_c_binary("delivery", ["list_all_orders"])

    if result["status"] != "SUCCESS":
        return jsonify({"status": "SUCCESS", "orders": [],
                        "warning": result.get("data", "")})

    orders = []
    raw_lines = result["raw_output"].strip().split("\n")
    for line in raw_lines[1:]:
        line = line.strip()
        if not line:
            continue
        parts = line.split("|")
        if len(parts) < 8:
            continue
        orders.append({
            "order_id":        parts[0],
            "user_id":         parts[1],
            "total_amount":    _safe_float(parts[2]),
            "delivery_slot":   parts[3],
            "delivery_boy_id": parts[4],
            "status":          parts[5],
            "timestamp":       parts[6],
            "items_string":    parts[7],
            "boy_name":        parts[8]  if len(parts) > 8  else "Unknown",
            "boy_phone":       parts[9]  if len(parts) > 9  else "N/A",
        })

    return jsonify({"status": "SUCCESS", "orders": orders})   

@app.route("/api/update_order_status", methods=["POST"])
def api_update_order_status():
    """
    POST /api/update_order_status
    Body: { "order_id": "ORD107", "status": "Out for Delivery" }

    Delegates to: ./delivery update_status <order_id> <status>

    Valid status values (must match delivery.c string literals exactly):
      "Order Placed" | "Out for Delivery" | "Delivered" | "Cancelled"
    """
    if session.get("role") != "admin":
        return jsonify({"status": "ERROR", "message": "Admin access required"}), 403

    data       = request.get_json(silent=True) or {}
    order_id   = data.get("order_id",  "").strip()
    new_status = data.get("status",    "").strip()

    VALID_STATUSES = {"Order Placed", "Out for Delivery", "Delivered", "Cancelled"}
    if not order_id:
        return jsonify({"status": "ERROR", "message": "order_id is required"})
    if new_status not in VALID_STATUSES:
        return jsonify({"status": "ERROR", "message": f"Invalid status: {new_status}"})

    # ── Delegate to the NEW delivery binary ───────────────────────────
    result = run_c_binary("delivery", ["update_status", order_id, new_status])
    return jsonify({
        "status":  result["status"],
        "message": result.get("data", "")
    })

@app.route("/api/promote_slot_orders", methods=["POST"])
def api_promote_slot_orders():
    """
    POST /api/promote_slot_orders
    Body: { "slot": "Morning" }

    Delegates to: ./delivery batch_promote_slot <slot>
    Returns: { "status": "SUCCESS", "promoted": <int> }
    """
    if session.get("role") != "admin":
        return jsonify({"status": "ERROR", "message": "Admin only"})

    slot = (request.get_json(silent=True) or {}).get("slot", "").strip()
    if slot not in ["Morning", "Afternoon", "Evening"]:
        return jsonify({"status": "ERROR", "message": "Invalid slot"})

    result = run_c_binary("delivery", ["batch_promote_slot", slot])
    if result["status"] != "SUCCESS":
        return jsonify({"status": "ERROR", "message": result.get("data", "")})

    try:
        promoted = int(result["data"].strip())
    except (ValueError, AttributeError):
        promoted = 0

    return jsonify({"status": "SUCCESS", "promoted": promoted})

@app.route("/api/cancel_order", methods=["POST"])
def api_cancel_order():
    """
    POST /api/cancel_order
    Body: { "order_id": "ORD107" }

    Business rules enforced in delivery.c:
      - Only "Order Placed" orders may be cancelled.
      - ₹50 cancellation fee is surfaced in the UI disclaimer only
        (actual refund processing is outside scope for this project).

    Delegates to: ./delivery cancel_order <order_id>

    Returns:
      { "status": "SUCCESS", "message": "Order cancelled" }
      { "status": "ERROR",   "message": "Only Order Placed orders..." }
    """
    # Both admin and logged-in users may cancel their own orders.
    if "user_id" not in session:
        return jsonify({"status": "ERROR", "message": "Not logged in"}), 401

    data     = request.get_json(silent=True) or {}
    order_id = data.get("order_id", "").strip()

    if not order_id:
        return jsonify({"status": "ERROR", "message": "order_id is required"})

    result = run_c_binary("delivery", ["cancel_order", order_id])
    return jsonify({
        "status":  result["status"],
        "message": result.get("data", "")
    })

@app.route("/api/get_active_orders", methods=["GET"])
def api_get_active_orders():
    """
    GET /api/get_active_orders

    Returns only "Order Placed" and "Out for Delivery" orders,
    enriched with boy_name + boy_phone.

    Delegates to: ./delivery get_active_orders

    Returns:
      { "status": "SUCCESS", "orders": [...] }
    """
    if session.get("role") != "admin":
        return jsonify({"status": "ERROR", "message": "Admin only"}), 403

    result = run_c_binary("delivery", ["get_active_orders"])
    if result["status"] != "SUCCESS":
        return jsonify({"status": "SUCCESS", "orders": [],
                        "warning": result.get("data", "")})

    orders = []
    raw_lines = result["raw_output"].strip().split("\n")
    for line in raw_lines[1:]:
        line = line.strip()
        if not line:
            continue
        parts = line.split("|")
        if len(parts) < 8:
            continue
        orders.append({
            "order_id":        parts[0],
            "user_id":         parts[1],
            "total_amount":    _safe_float(parts[2]),
            "delivery_slot":   parts[3],
            "delivery_boy_id": parts[4],
            "status":          parts[5],
            "timestamp":       parts[6],
            "items_string":    parts[7],
            "boy_name":        parts[8]  if len(parts) > 8  else "Unknown",
            "boy_phone":       parts[9]  if len(parts) > 9  else "N/A",
        })

    return jsonify({"status": "SUCCESS", "orders": orders})

@app.route("/api/assign_agent", methods=["POST"])
def api_assign_agent():
    """
    POST /api/assign_agent
    Body: { "order_id": "ORD107", "boy_id": "D003" }

    Delegates to: ./delivery assign_agent <order_id> <boy_id>
    Returns: { "status": "SUCCESS", "message": "Agent assigned" }
    """
    if session.get("role") != "admin":
        return jsonify({"status": "ERROR", "message": "Admin only"}), 403

    data     = request.get_json(silent=True) or {}
    order_id = data.get("order_id", "").strip()
    boy_id   = data.get("boy_id",   "").strip()

    if not order_id or not boy_id:
        return jsonify({"status": "ERROR", "message": "order_id and boy_id are required"})

    result = run_c_binary("delivery", ["assign_agent", order_id, boy_id])
    return jsonify({
        "status":  result["status"],
        "message": result.get("data", "")
    })

# ─────────────────────────────────────────────────────────────
# NOTE FOR BRIDGE.PY:
# ─────────────────────────────────────────────────────────────
# The run_c_binary() function in bridge.py must return both
# result["data"] (the part after "SUCCESS|" or "ERROR|")
# AND result["raw_output"] (the complete stdout string).
#
# If your current bridge.py does not include "raw_output",
# add this to the return dict in run_c_binary():
#
#   return {
#       "status":     parts[0],
#       "data":       "|".join(parts[1:]),
#       "raw_output": output   # <-- ADD THIS LINE
#   }
#
# This lets Flask parse multi-line output (used by list_products,
# view_cart, get_orders, admin_orders).
# ─────────────────────────────────────────────────────────────


# =============================================================
# START THE SERVER — with SSL/HTTPS
# =============================================================

if __name__ == "__main__":

    # ─────────────────────────────────────────────────────────
    # SSL CONTEXT SETUP
    # ─────────────────────────────────────────────────────────
    # ssl.SSLContext is a Python object that holds our certificate
    # and private key. We pass it to app.run() so Flask/Werkzeug
    # wraps every connection in TLS encryption.
    #
    # ssl.PROTOCOL_TLS_SERVER:
    #   Tells Python we are the SERVER (not a client connecting to
    #   someone else). It automatically negotiates the best available
    #   TLS version (1.2 or 1.3) supported by both sides.
    # ─────────────────────────────────────────────────────────
    ssl_ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)

    # load_cert_chain(certfile, keyfile):
    #   Loads our public certificate AND private key into the context.
    #   certfile = cert.pem -> shown to browsers to prove our identity
    #   keyfile  = key.pem  -> used to decrypt incoming encrypted data
    #
    # If files don't exist, we print a helpful message and exit cleanly.
    try:
        ssl_ctx.load_cert_chain(certfile=CERT_FILE, keyfile=KEY_FILE)
    except FileNotFoundError:
        print()
        print("  ERROR: cert.pem or key.pem not found!")
        print("  Fix:   cd Fresh-Picks/app && python generate_certs.py")
        print()
        exit(1)

    # ─────────────────────────────────────────────────────────
    # CHOOSE YOUR LAUNCH MODE
    # Comment out the version you are NOT using.
    # Only one HOST value should be active at a time.
    # ─────────────────────────────────────────────────────────

    # ══════════════════════════════════════════════════════════
    # VERSION A — LOCAL TESTING (Private, single laptop only)
    # ══════════════════════════════════════════════════════════
    # host="127.0.0.1" is the "loopback" address.
    # Traffic on 127.0.0.1 never leaves your laptop — it loops
    # back internally. It's invisible to any other device on the
    # network, providing a private development environment.
    #
    # USE WHEN: Testing alone, not needing other devices.
    # ACCESS:   https://localhost:5000
    # ══════════════════════════════════════════════════════════

    # ══════════════════════════════════════════════════════════
    # VERSION B — DEMO MODE (Wi-Fi, all devices on same network)
    # ══════════════════════════════════════════════════════════
    # host="0.0.0.0" means INADDR_ANY — a special address that
    # tells the OS kernel: "Bind to ALL available network interfaces
    # simultaneously."
    #
    # Your laptop has multiple network interfaces, each with its own IP:
    #   127.0.0.1    -> Loopback (internal only)
    #   192.168.x.x  -> Wi-Fi adapter (router-assigned, LAN-visible)
    #   10.x.x.x     -> Sometimes VPN or Ethernet adapter
    #
    # 0.0.0.0 listens on ALL of them at once. Any device on the
    # same LAN/Wi-Fi can then reach your server using your Wi-Fi IP.
    #
    # HOW TO FIND YOUR Wi-Fi IP:
    #   Windows:  CMD -> ipconfig
    #             "Wireless LAN adapter Wi-Fi:"
    #             "   IPv4 Address. . . . .: 192.168.x.x"  <- this one
    #
    #   Mac:      Terminal -> ifconfig en0
    #             "inet 192.168.x.x netmask ..."            <- this one
    #
    #   Linux:    Terminal -> ip addr show wlan0
    #             "inet 192.168.x.x/24 ..."                 <- this one
    #
    # TEAMMATES: Open browser on their device and go to:
    #   https://192.168.x.x:5000  (your IP, not theirs)
    #   Accept the certificate warning -> Advanced -> Proceed
    #
    # USE WHEN: College demo, professor review, team testing.
    # ══════════════════════════════════════════════════════════


    """
    === LAUNCH MODE ===
    """

    # UNCOMMENT for Version A (Local)
    # HOST = "127.0.0.1"   

    # UNCOMMENT for Version B (Demo/Wi-Fi)
    HOST = "0.0.0.0"   

    PORT  = 5000   # Flask's standard dev port. Free to use without admin rights.
    DEBUG = True   # Auto-reload on code changes. Set False for production.


    # ─────────────────────────────────────────────────────────
    # GET LOCAL IPv4 ADDRESS (Wi-Fi / LAN IP)
    # ─────────────────────────────────────────────────────────
    # This function automatically detects your machine's active
    # network IP address (usually 192.168.x.x).
    #
    # HOW IT WORKS:
    # - Creates a UDP socket (no actual data is sent)
    # - "Connects" to a public DNS server (8.8.8.8)
    # - OS selects the correct network interface (Wi-Fi/Ethernet)
    # - Extracts the local IP assigned to that interface
    #
    # WHY THIS IS USEFUL:
    # - Eliminates manual lookup using ipconfig / ifconfig
    # - Automatically prints the correct Wi-Fi URL for demos
    # - Makes your project look clean and production-ready
    #
    # FALLBACK:
    # - If detection fails, returns 127.0.0.1 (safe default)
    # ─────────────────────────────────────────────────────────

    import socket

    def get_local_ip():
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(("8.8.8.8", 80))  # No real connection made
            ip = s.getsockname()[0]
            s.close()
            return ip
        except:
            return "127.0.0.1"    

    LOCAL_IP = get_local_ip()
    # ─────────────────────────────────────────────────────────
    # START-UP BANNER
    # Clearly shows which mode is active and connection details.
    # ─────────────────────────────────────────────────────────
    print()
    print("=" * 62)
    print("  FreshPicks  |  CodeCrafters  |  SDP-1  |  HTTPS Server")
    print("=" * 62)
    print()
    if HOST == "127.0.0.1":
        print("  Mode        :  VERSION A  (Local only)")
        print(f"  Home         :  https://localhost:{PORT}/")
        print(f"  Admin Portal :  https://localhost:{PORT}/login/admin")
        print(f"  User Portal  :  https://localhost:{PORT}/login/user")
        print()
        print("  Not visible to other devices on this network.")
    else:
        print("  Mode        :  VERSION B  (Wi-Fi / Demo Mode)")
        print()
        print(f"  Home         :  https://{LOCAL_IP}:{PORT}/")
        print(f"  Admin Portal :  https://{LOCAL_IP}:{PORT}/login/admin")  # <-- HTTPS absolute URL
        print(f"  User Portal  :  https://{LOCAL_IP}:{PORT}/login/user")   # <-- HTTPS absolute URL
        print()
        print(f"  Localhost    :  https://localhost:{PORT}/")
        print()
        print("  Share the Wi-Fi URLs above with teammates.")
        print("  Each device: Advanced -> Proceed (accept self-signed cert).")
    print()
    print("  Browser shows 'Not private'? That is expected (self-signed cert).")
    print("=" * 62)
    print()

    # ─────────────────────────────────────────────────────────
    # LAUNCH FLASK WITH SSL
    #
    # ssl_context=ssl_ctx:
    #   This single argument is what converts the server from
    #   plain HTTP to encrypted HTTPS. Werkzeug (Flask's dev server)
    #   reads the cert+key from ssl_ctx and wraps every TCP connection
    #   in a TLS handshake before any HTTP data is exchanged.
    #
    # WITHOUT ssl_context: http://localhost:5000  (plain text)
    # WITH    ssl_context: https://localhost:5000 (encrypted)
    # ─────────────────────────────────────────────────────────
    app.run(
        host        = HOST,
        port        = PORT,
        debug       = DEBUG,
        ssl_context = ssl_ctx
    )
