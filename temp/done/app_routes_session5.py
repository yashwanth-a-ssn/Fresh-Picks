"""
app_routes_session5.py  —  Fresh Picks: Session 5 route additions
==================================================================
Copy the functions below into app.py.

NEW ROUTES:
  GET  /login/admin              → renders admin_login.html directly
  GET  /login/user               → renders user_login.html (new, Session 5)
  POST /api/get_admin_orders     → returns active orders as JSON (for admin_orders.html)
  POST /api/promote_slot_orders  → JIT auto-promotion for a given slot
  POST /api/get_user_orders      → returns this user's orders as JSON

ALSO ADD to build.sh:
  gcc -o order order.c ds_utils.c -lm

Team: CodeCrafters | SDP-1
"""

# ─────────────────────────────────────────────────────────────
# IMPORT additions (already in app.py, shown for reference):
#   from bridge import run_c_binary
# ─────────────────────────────────────────────────────────────


# =============================================================
# TASK C: SPLIT LOGIN ROUTES  (replaces the old /login?role=x)
# =============================================================

@app.route("/login/admin")
def login_admin():
    """
    PURPOSE: Serve the Admin Login page directly.
    Session 5 change: was GET /login?role=admin.
    Renders admin_login.html (existing template).
    """
    return render_template("admin_login.html")


@app.route("/login/user")
def login_user():
    """
    PURPOSE: Serve the NEW User Login page (Session 5 — Task B).
    Renders user_login.html which has the Cart-Check icon and
    the "New customer?" registration link.
    """
    return render_template("user_login.html")


# =============================================================
# TASK D SUPPORT: Get User Orders as JSON
# =============================================================

@app.route("/api/get_user_orders", methods=["POST"])
def api_get_user_orders():
    """
    PURPOSE: Return all orders for the logged-in user as JSON.
    Reads user_id from session (set at login).
    Calls C binary: ./order get_orders <user_id>

    OUTPUT FORMAT for JS (user_orders.html):
    {
      "status": "SUCCESS",
      "orders": [
        {
          "order_id":        "ORD101",
          "user_id":         "U1001",
          "total_amount":    "250.00",
          "delivery_slot":   "Morning",
          "delivery_boy_id": "D001",
          "status":          "Out for Delivery",
          "timestamp":       "2025-04-08 09:00:00",
          "items_string":    "V1001:500:40.00,VF101:50:0.00",
          "boy_name":        "Ramesh",   ← enriched by Flask
          "boy_phone":       "9876543210"
        },
        ...
      ]
    }
    """
    user_id = session.get("user_id")
    if not user_id:
        return jsonify({"status": "ERROR", "message": "Not logged in"}), 401

    # Call C binary to get orders for this user
    result = run_c_binary(["./order", "get_orders", user_id])

    if result["status"] != "SUCCESS":
        return jsonify({"status": "ERROR", "message": result["data"]}), 400

    orders = _parse_order_lines(result["raw_output"])

    # Enrich each order with delivery boy details
    boy_map = _load_delivery_boys()
    for o in orders:
        boy = boy_map.get(o["delivery_boy_id"], {})
        o["boy_name"]  = boy.get("name",  "")
        o["boy_phone"] = boy.get("phone", "")

    return jsonify({"status": "SUCCESS", "orders": orders})


# =============================================================
# TASK A SUPPORT: Get Admin Orders as JSON
# =============================================================

@app.route("/api/get_admin_orders", methods=["POST"])
def api_get_admin_orders():
    """
    PURPOSE: Return all active orders (sorted by Min-Heap priority) as JSON.
    Calls C binary: ./order admin_orders
    Only accessible to admins (session check).
    """
    if session.get("role") != "admin":
        return jsonify({"status": "ERROR", "message": "Unauthorized"}), 403

    result = run_c_binary(["./order", "admin_orders"])
    if result["status"] != "SUCCESS":
        return jsonify({"status": "ERROR", "message": result["data"]}), 400

    orders = _parse_order_lines(result["raw_output"])

    # Enrich with delivery boy info
    boy_map = _load_delivery_boys()
    for o in orders:
        boy = boy_map.get(o["delivery_boy_id"], {})
        o["boy_name"]  = boy.get("name",  "")
        o["boy_phone"] = boy.get("phone", "")

    return jsonify({"status": "SUCCESS", "orders": orders})


# =============================================================
# TASK A: JIT SLOT PROMOTION
# =============================================================

@app.route("/api/promote_slot_orders", methods=["POST"])
def api_promote_slot_orders():
    """
    PURPOSE: Auto-promote all "Order Placed" orders for the given slot
             to "Out for Delivery".

    CALLED BY: admin_orders.html JS on page-load when a slot window is open.

    BODY:    { "slot": "Afternoon" }
    RETURNS: { "status": "SUCCESS", "promoted": 3 }

    HOW:
      1. Read all active orders (admin_orders C command).
      2. Filter: status == "Order Placed" AND delivery_slot == requested slot.
      3. For each match, call: ./order update_order_status <id> "Out for Delivery"
    """
    if session.get("role") != "admin":
        return jsonify({"status": "ERROR", "message": "Unauthorized"}), 403

    data = request.get_json(silent=True) or {}
    slot = data.get("slot", "").strip()

    if not slot:
        return jsonify({"status": "ERROR", "message": "slot is required"}), 400

    # Fetch all active orders
    result = run_c_binary(["./order", "admin_orders"])
    if result["status"] != "SUCCESS":
        return jsonify({"status": "ERROR", "message": result["data"]}), 400

    orders = _parse_order_lines(result["raw_output"])

    # Filter: "Order Placed" + matching slot
    to_promote = [
        o for o in orders
        if o["status"] == "Order Placed" and o["delivery_slot"] == slot
    ]

    # Promote each one
    promoted_count = 0
    for o in to_promote:
        upd = run_c_binary(
            ["./order", "update_order_status", o["order_id"], "Out for Delivery"]
        )
        if upd["status"] == "SUCCESS":
            promoted_count += 1

    return jsonify({"status": "SUCCESS", "promoted": promoted_count})


# =============================================================
# PRIVATE HELPERS
# =============================================================

def _parse_order_lines(raw_output):
    """
    PURPOSE: Parse the raw multi-line output from the C admin_orders /
             get_orders commands into a list of dicts.

    C output format (after the SUCCESS|count header line):
      order_id|user_id|total|slot|boy_id|status|timestamp|items_string

    RETURNS: list of order dicts
    """
    orders = []
    lines  = raw_output.strip().split("\n")

    for line in lines:
        # Skip the first "SUCCESS|N" header line and any blanks
        if not line or line.startswith("SUCCESS") or line.startswith("ERROR"):
            continue

        parts = line.split("|")
        if len(parts) < 8:
            continue  # Malformed line — skip silently

        orders.append({
            "order_id":        parts[0],
            "user_id":         parts[1],
            "total_amount":    parts[2],
            "delivery_slot":   parts[3],
            "delivery_boy_id": parts[4],
            "status":          parts[5],
            "timestamp":       parts[6],
            "items_string":    parts[7] if len(parts) > 7 else ""
        })

    return orders


def _load_delivery_boys():
    """
    PURPOSE: Read delivery_boys.txt and return a dict keyed by boy_id.
             Used to enrich orders with the delivery agent's name + phone.

    DB format (models.h DeliveryBoy):
      boy_id|name|phone|vehicle_no|is_active|last_assigned

    RETURNS: { "D001": { "name": "Ramesh", "phone": "9876543210" }, ... }
    """
    boy_map = {}
    try:
        with open("delivery_boys.txt", "r") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                parts = line.split("|")
                if len(parts) < 3:
                    continue
                boy_map[parts[0]] = {
                    "name":  parts[1],
                    "phone": parts[2]
                }
    except FileNotFoundError:
        pass  # File not yet created — return empty map
    return boy_map
