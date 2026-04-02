"""
app.py - Fresh Picks: Main Flask Application
=============================================
This is the central Flask server. It:
  1. Serves HTML pages (templates)
  2. Exposes API routes that the frontend JavaScript calls
  3. Uses bridge.py to communicate with C binaries
  4. Manages user sessions (login state)

ROUTES:
  GET  /                      -> Landing page (index.html)
  GET  /login                 -> Login page
  GET  /register              -> Register page
  POST /api/login             -> Validate login via C binary
  POST /api/register          -> Register new user via C binary
  GET  /user_home             -> User dashboard (requires login)
  GET  /admin_dash            -> Admin dashboard (requires admin login)
  GET  /profile               -> User profile page
  POST /api/get_profile       -> Fetch profile data via C binary
  POST /api/change_pass_user  -> Change user password via C binary
  POST /api/change_pass_admin -> Change admin password via C binary
  GET  /logout                -> Clear session and redirect to home

HOW TO RUN:
  cd Fresh-Picks/app
  python app.py

Team: CodeCrafters | Project: Fresh Picks | SDP-1
"""

from flask import (
    Flask, render_template, request,
    jsonify, session, redirect, url_for
)
from bridge import run_c_binary  # Our reusable C-caller function

# ─────────────────────────────────────────────────────────────
# Flask App Setup
# ─────────────────────────────────────────────────────────────
app = Flask(
    __name__,
    template_folder="../templates",   # HTML files are in /templates/
    static_folder="../static"         # CSS/JS files are in /static/
)

# Secret key for session encryption.
# In production, use a long random string from os.urandom(24).
app.secret_key = "fresh_picks_secret_codecrafters_2024"


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

    return render_template("admin_dash.html", username=session.get("username"))


@app.route("/profile")
def profile():
    """
    PURPOSE: Serve the user profile page.
    Requires user login.
    """
    if "user_id" not in session or session.get("role") != "user":
        return redirect(url_for("login", role="user"))

    return render_template("profile.html")


@app.route("/logout")
def logout():
    """
    PURPOSE: Clear the session (log out the user/admin)
    and redirect to the landing page.
    """
    session.clear()  # Remove all session data
    return redirect(url_for("index"))


# =============================================================
# API ROUTES - These return JSON, called by JavaScript fetch()
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

    # Step 2: Basic validation — make sure fields aren't empty
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
            session["user_id"] = "admin"
        else:
            # The C binary returns the user_id as 'data'
            session["user_id"] = result["data"]

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
    Accepts JSON with: { username, password, full_name, phone, address }
    Calls the C binary to write the new user to users.txt.
    """
    data      = request.get_json()
    username  = data.get("username", "").strip()
    password  = data.get("password", "").strip()
    full_name = data.get("full_name", "").strip()
    phone     = data.get("phone", "").strip()
    address   = data.get("address", "").strip()

    # Validate all fields are present
    if not all([username, password, full_name, phone, address]):
        return jsonify({"status": "ERROR", "message": "All fields are required"})

    # Call the C binary to register
    result = run_c_binary("auth", ["register", username, password, full_name, phone, address])

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
    Uses user_id stored in session — no need for frontend to send it.
    Returns profile fields as JSON.
    """
    # Security check: must be logged in
    if "user_id" not in session:
        return jsonify({"status": "ERROR", "message": "Not logged in"})

    user_id = session["user_id"]
    result  = run_c_binary("auth", ["get_profile", user_id])

    if result["status"] == "SUCCESS":
        # Parse the pipe-delimited data: id|username|full_name|phone|address
        parts = result["data"].split("|")
        return jsonify({
            "status":    "SUCCESS",
            "user_id":   parts[0] if len(parts) > 0 else "",
            "username":  parts[1] if len(parts) > 1 else "",
            "full_name": parts[2] if len(parts) > 2 else "",
            "phone":     parts[3] if len(parts) > 3 else "",
            "address":   parts[4] if len(parts) > 4 else ""
        })
    else:
        return jsonify({"status": "ERROR", "message": result["data"]})


@app.route("/api/change_pass_user", methods=["POST"])
def api_change_pass_user():
    """
    PURPOSE: Change the logged-in user's password.
    Accepts JSON: { "old_password": "...", "new_password": "..." }
    Uses user_id from session.
    """
    if "user_id" not in session or session.get("role") != "user":
        return jsonify({"status": "ERROR", "message": "Not authorized"})

    data         = request.get_json()
    old_password = data.get("old_password", "").strip()
    new_password = data.get("new_password", "").strip()

    if not old_password or not new_password:
        return jsonify({"status": "ERROR", "message": "Both passwords are required"})

    user_id = session["user_id"]
    result  = run_c_binary("auth", ["change_pass_user", user_id, old_password, new_password])

    return jsonify({"status": result["status"], "message": result["data"]})


@app.route("/api/change_pass_admin", methods=["POST"])
def api_change_pass_admin():
    """
    PURPOSE: Change the admin's password.
    Accepts JSON: { "old_password": "...", "new_password": "..." }
    """
    if session.get("role") != "admin":
        return jsonify({"status": "ERROR", "message": "Not authorized"})

    data         = request.get_json()
    old_password = data.get("old_password", "").strip()
    new_password = data.get("new_password", "").strip()

    if not old_password or not new_password:
        return jsonify({"status": "ERROR", "message": "Both passwords are required"})

    result = run_c_binary("auth", ["change_pass_admin", old_password, new_password])
    return jsonify({"status": result["status"], "message": result["data"]})


# =============================================================
# Start the server
# =============================================================
if __name__ == "__main__":
    # debug=True  -> auto-reloads on code changes, shows errors in browser
    # port=5000   -> access at http://localhost:5000
    print("=" * 50)
    print("  Fresh Picks Server Starting...")
    print("  Visit: http://localhost:5000")
    print("=" * 50)
    app.run(debug=True, port=5000)
