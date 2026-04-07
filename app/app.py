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

from flask import (
    Flask,              # Creates and initializes the web application
    render_template,    # Renders HTML templates for frontend pages
    request,            # Retrieves incoming data from client requests
    jsonify,            # Converts data into JSON HTTP responses
    session,            # Stores user session data across requests
    redirect,           # Redirects user to a different route
    url_for             # Generates dynamic URLs for application routes
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


# =============================================================
# PAGE ROUTES - These serve HTML pages
# =============================================================

@app.route("/")
def index():
    """
    PURPOSE: Serve the landing page.
    Shows two buttons: Admin Portal and User Portal.
    """
    return render_template("index.html")


@app.route("/login")
def login():
    """
    PURPOSE: Serve the shared login page.
    Reads the 'role' query parameter to know if it's admin or user login.
    Example URLs:
      /login?role=user
      /login?role=admin
    """
    # request.args is a dictionary-like object in Flask that stores:
    # Data sent via URL query parameters (GET request)
    # request.args.get("key", default_value)
    role = request.args.get("role", "user")  # Default to user if not specified
    return render_template("login.html", role=role)


@app.route("/register")
def register():
    """
    PURPOSE: Serve the registration page for new users.
    """
    return render_template("register.html")


@app.route("/user_home")
def user_home():
    """
    PURPOSE: Serve the user's home/dashboard page.
    Requires the user to be logged in (checks session).
    If not logged in, redirects to login page.
    """
    # Check if user is logged in by looking at session data
    if "user_id" not in session or session.get("role") != "user":
        return redirect(url_for("login", role="user"))

    # Pass the username to the template so we can display "Welcome, alice!"
    return render_template("user_home.html", username=session.get("username"))


@app.route("/admin_dash")
def admin_dash():
    """
    PURPOSE: Serve the admin dashboard.
    Requires admin login (checks session for role='admin').
    """
    if session.get("role") != "admin":
        return redirect(url_for("login", role="admin"))

    return render_template(
        "admin_dash.html",
        username   = session.get("username"),
        admin_name = session.get("admin_name", "Admin")  # Passed to {{ admin_name }} in template
    )


@app.route("/profile")
def profile():
    """
    PURPOSE: Serve the user profile page.
    Requires user login.
    """
    if "user_id" not in session or session.get("role") != "user":
        return redirect(url_for("login", role="user"))

    return render_template("profile.html")


@app.route("/security")
def security():
    """
    PURPOSE: Serve the Security Center page (Module 5).
    Accessible to any logged-in user or admin.
    If not logged in, redirect to home.
    """
    if "user_id" not in session:
        return redirect(url_for("index"))

    return render_template("security.html")


@app.route("/logout")
def logout():
    """
    PURPOSE: Clear the session (log out the user/admin)
    and redirect to the landing page.
    """
    session.clear()  # Remove all session data
    return redirect(url_for("index"))

@app.route("/vegetables")
def vegetables():
    """
    PURPOSE: Serve the Product Listing page.
    User must be logged in to shop. Redirect if not.
    """
    if "user_id" not in session or session.get("role") != "user":
        return redirect(url_for("login", role="user"))
    return render_template("vegetables.html")


@app.route("/cart")
def cart():
    """
    PURPOSE: Serve the Cart / Checkout / Invoice page.
    The same template handles all three states (managed by JS on the frontend).
    User must be logged in.
    """
    if "user_id" not in session or session.get("role") != "user":
        return redirect(url_for("login", role="user"))
    return render_template("cart.html")


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
    """
    PURPOSE: Handle login form submission.
    Accepts JSON with: { "username": "...", "password": "...", "role": "user"/"admin" }
    Calls the C binary to validate credentials.
    On success, stores user info in session and returns JSON.
    """
    # Step 1: Read the JSON body sent by the frontend
    data = request.get_json()
    username = data.get("username", "").strip()
    password = data.get("password", "").strip()
    role     = data.get("role", "user").strip()

    # Step 2: Basic validation - make sure fields aren't empty
    if not username or not password:
        return jsonify({"status": "ERROR", "message": "Username and password are required"})

    # Step 3: Call the correct C function based on role
    if role == "admin":
        result = run_c_binary("auth", ["login_admin", username, password])
    else:
        result = run_c_binary("auth", ["login_user", username, password])

    # Step 4: If login succeeded, save user info in session
    
    if result["status"] == "SUCCESS":
        session["role"]     = role
        session["username"] = username

        if role == "admin":
            # C now returns: SUCCESS|admin_id|admin_name
            # result["data"] is everything after "SUCCESS|", so: "admin_id|admin_name"
            admin_parts = result["data"].split("|")
            session["user_id"]    = admin_parts[0] if len(admin_parts) > 0 else "A1001"
            session["admin_name"] = admin_parts[1] if len(admin_parts) > 1 else "Admin"
        else:
            # For users, C returns: SUCCESS|user_id (unchanged)
            session["user_id"]    = result["data"]

        return jsonify({
            "status":   "SUCCESS",
            "message":  "Login successful",
            "role":     role,
            "user_id":  session["user_id"],
            "redirect": "/admin_dash" if role == "admin" else "/user_home"
        })
    else:
        return jsonify({"status": "ERROR", "message": result["data"]})


@app.route("/api/register", methods=["POST"])
def api_register():
    """
    PURPOSE: Handle user registration.
    Accepts JSON with: { username, password, full_name, email, phone, address }
    Calls the C binary to write the new user to users.txt.
    C binary enforces password complexity — error is returned from C.
    """
    # get_json() => Extract Data from a JSON File
    data      = request.get_json()
    username  = data.get("username",  "").strip()
    password  = data.get("password",  "").strip()
    full_name = data.get("full_name", "").strip()
    email     = data.get("email",     "").strip()
    phone     = data.get("phone",     "").strip()

    # Receive the 4 address sub-fields separately from the frontend form.
    # Each field is collected individually for clear display later.
    door   = data.get("door",   "").strip()  # e.g. "No 11, FF2"
    street = data.get("street", "").strip()  # e.g. "Elumalai Street"
    area   = data.get("area",   "").strip()  # e.g. "West Tambaram"
    pincode= data.get("pincode","").strip()  # e.g. "600045"

    # Concatenate into a single CSV string for storage in users.txt.
    # The pipe (|) delimiter is already used between fields, so we use
    # a comma to separate address sub-parts within the address field.
    # This matches the schema note: "store as csv within the address field."
    address = f"{door},{street},{area},{pincode}"

    # Validate all required fields are present
    if not all([username, password, full_name, email, phone, door, street, area, pincode]):
        return jsonify({"status": "ERROR", "message": "All fields are required"})

    # Call the C binary — now 8 args: register + 6 user fields
    # argv: auth register username password full_name email phone address
    result = run_c_binary("auth", ["register", username, password,
                                   full_name, email, phone, address])

    if result["status"] == "SUCCESS":
        return jsonify({
            "status":  "SUCCESS",
            "message": "Registration successful! Please log in.",
            "user_id": result["data"]
        })
    else:
        return jsonify({"status": "ERROR", "message": result["data"]})


@app.route("/api/get_profile", methods=["POST"])
def api_get_profile():
    """
    PURPOSE: Fetch the current user's profile data.
    Uses user_id stored in session - no need for frontend to send it.
    Returns profile fields as JSON including email.
    """
    # Security check: must be logged in
    if "user_id" not in session:
        return jsonify({"status": "ERROR", "message": "Not logged in"})

    user_id = session["user_id"]
    # run_c_binary(command, argument_list)
    result  = run_c_binary("auth", ["get_profile", user_id])

    if result["status"] == "SUCCESS":
        # Parse pipe-delimited data: id|username|full_name|email|phone|address
        parts = result["data"].split("|")
        return jsonify({
            "status":    "SUCCESS",
            "user_id":   parts[0] if len(parts) > 0 else "",
            "username":  parts[1] if len(parts) > 1 else "",
            "full_name": parts[2] if len(parts) > 2 else "",
            "email":     parts[3] if len(parts) > 3 else "",
            "phone":     parts[4] if len(parts) > 4 else "",
            "address":   parts[5] if len(parts) > 5 else ""
        })
    else:
        return jsonify({"status": "ERROR", "message": result["data"]})


@app.route("/api/change_password", methods=["POST"])
def api_change_password():
    """
    PURPOSE: Unified password change endpoint for BOTH users and admins.
    Reads role and user_id from the server-side session (not the client).
    Calls: ./auth change_password <role> <id> <old_pass> <new_pass>
    Accepts JSON: { "old_password": "...", "new_password": "..." }
    """
    # Security check: must be logged in (any role)
    if "user_id" not in session:
        return jsonify({"status": "ERROR", "message": "Not authorized. Please log in."})

    data         = request.get_json()
    old_password = data.get("old_password", "").strip()
    new_password = data.get("new_password", "").strip()

    if not old_password or not new_password:
        return jsonify({"status": "ERROR", "message": "Both passwords are required"})

    # Read role and ID securely from the server session
    role    = session.get("role")     # "user" or "admin"
    user_id = session.get("user_id")  # "U1001" or "A1001"

    # Route to the correct C function based on role.
    # For users:  change_pass_user  <user_id>  <old> <new>
    # For admins: change_pass_admin <admin_id> <old> <new>
    if role == "admin":
        result = run_c_binary("auth", ["change_pass_admin", user_id,
                                       old_password, new_password])
    else:
        result = run_c_binary("auth", ["change_pass_user", user_id,
                                       old_password, new_password])

    return jsonify({"status": result["status"], "message": result["data"]})


@app.route("/api/list_products", methods=["GET"])
def api_list_products():
    """
    PURPOSE: Return all products from vegetables.txt as a JSON array.
    Calls:   ./order list_products
    C OUTPUT FORMAT (one vegetable per line after SUCCESS|):
      veg_id|category|name|stock_g|price_per_1000g|tag|validity_days

    PARSING:
      Line 0 = "SUCCESS|"  (we ignore it)
      Lines 1+ = one vegetable per line, pipe-delimited
    """
    result = run_c_binary("order", ["list_products"])

    if result["status"] != "SUCCESS":
        return jsonify({"status": "ERROR", "message": result["data"]})

    # Split the raw output into individual vegetable lines.
    # The first line from C is "SUCCESS|" (just the header).
    # Each subsequent line is one vegetable.
    raw_lines = result["raw_output"].strip().split("\n")
    products  = []

    for line in raw_lines[1:]:  # Skip the "SUCCESS|" header line
        line = line.strip()
        if not line:
            continue
        parts = line.split("|")
        if len(parts) < 7:
            continue  # Skip malformed lines

        products.append({
            "veg_id":          parts[0],
            "category":        parts[1],
            "name":            parts[2],
            "stock_g":         int(parts[3]),
            "price_per_1000g": float(parts[4]),
            "tag":             parts[5],
            "validity_days":   int(parts[6])
        })

    return jsonify({"status": "SUCCESS", "products": products})


@app.route("/api/add_to_cart", methods=["POST"])
def api_add_to_cart():
    """
    PURPOSE: Add or update one item in the user's cart.
    Calls:   ./order add_to_cart <user_id> <veg_id> <qty_grams>

    HOW THE C BACKEND WORKS:
      1. Validates the quantity (> 0, multiple of 50g)
      2. Looks up the vegetable in vegetables.txt to get name + price
      3. Loads the user's cart DLL from carts/<user_id>_cart.txt
      4. Calls dll_update_or_append() — updates qty if item exists, else adds new node
      5. Saves the updated DLL back to the cart file

    REQUEST JSON: { "veg_id": "V1001", "qty_g": 500 }
    """
    if "user_id" not in session:
        return jsonify({"status": "ERROR", "message": "Not logged in"})

    data    = request.get_json()
    veg_id  = data.get("veg_id", "").strip()
    qty_g   = int(data.get("qty_g", 0))
    user_id = session["user_id"]

    if not veg_id or qty_g <= 0:
        return jsonify({"status": "ERROR", "message": "Invalid item or quantity"})

    result = run_c_binary("order", ["add_to_cart", user_id, veg_id, str(qty_g)])

    return jsonify({
        "status":  result["status"],
        "message": result["data"]
    })


@app.route("/api/view_cart", methods=["POST"])
def api_view_cart():
    """
    PURPOSE: Return all items in the user's current cart.
    Calls:   ./order view_cart <user_id>

    C OUTPUT FORMAT:
      Line 0: SUCCESS|<grand_total>
      Lines 1+: veg_id|name|qty_g|price_per_1000g|item_total|is_free

    HOW THE C BACKEND WORKS:
      1. Reads carts/<user_id>_cart.txt into a Doubly Linked List
      2. Traverses the DLL from head to tail, printing each node
      3. Also computes and prints the grand total (dll_get_total)
    """
    if "user_id" not in session:
        return jsonify({"status": "ERROR", "message": "Not logged in"})

    user_id = session["user_id"]
    result  = run_c_binary("order", ["view_cart", user_id])

    if result["status"] != "SUCCESS":
        return jsonify({"status": "ERROR", "message": result["data"]})

    # Parse the raw C output
    lines     = result["raw_output"].strip().split("\n")
    first     = lines[0]   # "SUCCESS|<total>"
    total_str = first.split("|")[1] if "|" in first else "0"

    items = []
    for line in lines[1:]:
        line = line.strip()
        if not line:
            continue
        parts = line.split("|")
        if len(parts) < 6:
            continue
        items.append({
            "veg_id":          parts[0],
            "name":            parts[1],
            "qty_g":           int(parts[2]),
            "price_per_1000g": float(parts[3]),
            "item_total":      float(parts[4]),
            "is_free":         int(parts[5])
        })

    return jsonify({
        "status": "SUCCESS",
        "total":  float(total_str),
        "items":  items
    })


@app.route("/api/remove_item", methods=["POST"])
def api_remove_item():
    """
    PURPOSE: Remove one specific item from the user's cart.
    Calls:   ./order remove_item <user_id> <veg_id>

    HOW THE C BACKEND WORKS:
      1. Loads cart DLL from file
      2. Calls dll_remove(head, veg_id)
         -> Finds the node by veg_id
         -> Re-links: prev->next = node->next, node->next->prev = node->prev
         -> Frees the node's memory
      3. Saves the updated DLL back to file

    REQUEST JSON: { "veg_id": "V1001" }
    """
    if "user_id" not in session:
        return jsonify({"status": "ERROR", "message": "Not logged in"})

    data    = request.get_json()
    veg_id  = data.get("veg_id", "").strip()
    user_id = session["user_id"]

    if not veg_id:
        return jsonify({"status": "ERROR", "message": "No item specified"})

    result = run_c_binary("order", ["remove_item", user_id, veg_id])
    return jsonify({"status": result["status"], "message": result["data"]})


@app.route("/api/checkout", methods=["POST"])
def api_checkout():
    """
    PURPOSE: Execute the full payment pipeline.
    Calls:   ./order checkout <user_id> <delivery_slot>

    THIS IS THE MOST COMPLEX ROUTE. Here's the full pipeline in C:

    Step 1: Load the cart DLL from file
    Step 2: Minimum order check (₹100)
    Step 3: STOCK RECHECK (race condition prevention)
            -> Even if stock was fine at "Add to Cart" time,
               another user may have bought the same item before payment.
               We verify CURRENT stock levels again, right before deducting.
    Step 4: Apply freebies (if total >= ₹500, add VF101 + VF102 as free nodes)
    Step 5: Deduct confirmed stock from vegetables.txt
    Step 6: Round-robin delivery boy assignment using Circular Linked List
            -> Find boy with last_assigned==1 in CLL
            -> Move to ->next node
            -> That boy gets this order
            -> Update delivery_boys.txt
    Step 7: Enqueue order to FIFO OrderQueue (simulating order processing)
    Step 8: Append order to orders.txt
    Step 9: Delete cart file (cart is now empty)
    Step 10: Print confirmation: order_id|total|slot|boy_name|boy_phone|items

    C OUTPUT: SUCCESS|<order_id>|<total>|<slot>|<boy_name>|<boy_phone>|<items>

    REQUEST JSON: { "delivery_slot": "Morning" }
    """
    if "user_id" not in session:
        return jsonify({"status": "ERROR", "message": "Not logged in"})

    data = request.get_json()
    slot = data.get("delivery_slot", "").strip()
    user_id = session["user_id"]

    valid_slots = ["Morning", "Afternoon", "Evening"]
    if slot not in valid_slots:
        return jsonify({"status": "ERROR", "message": "Invalid delivery slot"})

    result = run_c_binary("order", ["checkout", user_id, slot])

    if result["status"] != "SUCCESS":
        return jsonify({"status": "ERROR", "message": result["data"]})

    # Parse C output: SUCCESS|order_id|total|slot|boy_name|boy_phone|items
    # result["data"] is everything after the first "SUCCESS|"
    # raw_output first line: "SUCCESS|ORD101|450.00|Morning|Ramesh|9876543210|V1001:500,..."
    first_line = result["raw_output"].strip().split("\n")[0]
    parts      = first_line.split("|")

    # parts[0] = "SUCCESS"
    # parts[1] = order_id
    # parts[2] = total
    # parts[3] = slot
    # parts[4] = boy_name
    # parts[5] = boy_phone
    # parts[6] = items_string
    if len(parts) < 7:
        return jsonify({"status": "ERROR", "message": "Unexpected response from server"})

    return jsonify({
        "status":   "SUCCESS",
        "order_id": parts[1],
        "total":    float(parts[2]),
        "slot":     parts[3],
        "boy_name": parts[4],
        "boy_phone":parts[5],
        "items":    parts[6] if len(parts) > 6 else ""
    })


@app.route("/api/get_orders", methods=["GET"])
def api_get_orders():
    """
    PURPOSE: Return all past orders for the currently logged-in user.
    Calls:   ./order get_orders <user_id>
    C filters orders.txt by user_id and returns matching rows.
    """
    if "user_id" not in session:
        return jsonify({"status": "ERROR", "message": "Not logged in"})

    user_id = session["user_id"]
    result  = run_c_binary("order", ["get_orders", user_id])

    if result["status"] != "SUCCESS":
        return jsonify({"status": "ERROR", "message": result["data"]})

    lines  = result["raw_output"].strip().split("\n")
    orders = []

    for line in lines[1:]:    # Skip the "SUCCESS|" header line
        line = line.strip()
        if not line:
            continue
        parts = line.split("|")
        if len(parts) < 7:
            continue
        orders.append({
            "order_id":        parts[0],
            "user_id":         parts[1],
            "total_amount":    float(parts[2]),
            "delivery_slot":   parts[3],
            "delivery_boy_id": parts[4],
            "status":          parts[5],
            "items":           parts[6]
        })

    return jsonify({"status": "SUCCESS", "orders": orders})


@app.route("/api/admin_orders", methods=["GET"])
def api_admin_orders():
    """
    PURPOSE: Return ALL orders sorted by delivery slot priority (Morning first).
    Calls:   ./order admin_orders
    ONLY accessible to admin role.

    HOW THE C BACKEND WORKS:
      1. Reads ALL orders from orders.txt into a Min-Heap
         (Morning=Priority 1, Afternoon=2, Evening=3)
      2. Repeatedly calls heap_extract_min() to print orders
         in ascending priority order (most urgent first)
      3. Flask returns the sorted list as a JSON array

    C OUTPUT:
      Line 0: SUCCESS|<count>
      Lines 1+: one order per line, pipe-delimited, sorted by priority
    """
    if session.get("role") != "admin":
        return jsonify({"status": "ERROR", "message": "Admin access required"})

    result = run_c_binary("order", ["admin_orders"])

    if result["status"] != "SUCCESS":
        return jsonify({"status": "ERROR", "message": result["data"]})

    lines  = result["raw_output"].strip().split("\n")
    orders = []

    for line in lines[1:]:    # Skip "SUCCESS|<count>" header
        line = line.strip()
        if not line:
            continue
        parts = line.split("|")
        if len(parts) < 7:
            continue
        orders.append({
            "order_id":        parts[0],
            "user_id":         parts[1],
            "total_amount":    float(parts[2]),
            "delivery_slot":   parts[3],
            "delivery_boy_id": parts[4],
            "status":          parts[5],
            "items":           parts[6]
        })

    return jsonify({"status": "SUCCESS", "orders": orders})


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

    # ─────────────────────────────────────────────────────────
    # START-UP BANNER
    # Clearly shows which mode is active and connection details.
    # ─────────────────────────────────────────────────────────
    print()
    print("=" * 56)
    print("  FreshPicks - Secure Server (HTTPS)")
    print("  Team: CodeCrafters | SDP-1")
    print("=" * 56)

    if HOST == "127.0.0.1":
        print("  Mode :  VERSION A - Local Testing Only")
        print(f"  URL  :  https://localhost:{PORT}")
        print(f"  URL  :  https://127.0.0.1:{PORT}")
        print("  Note :  Not visible to other devices on the network.")
    else:
        print("  Mode :  VERSION B - Demo / Wi-Fi Mode (INADDR_ANY)")
        print(f"  Local:  https://localhost:{PORT}")
        print(f"  Wi-Fi:  https://{get_local_ip()}:{PORT}")

    print()
    print("  Browser shows a warning? That is normal.")
    print(f"  Click: Advanced -> Proceed to <address> (unsafe)")
    print("=" * 56)
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
