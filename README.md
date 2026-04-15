# 🧺 Fresh Picks: The Hybrid Grocery Engine

**CodeCrafters SDP-1 Project** *Speed of C. Flexibility of Python. Elegance of VS Code Dark.*

Fresh Picks is a high-performance, intra-net hosted e-commerce platform designed for local grocery management. Unlike standard web apps, Fresh Picks offloads its core business logic and state management to compiled C binaries, utilizing custom-built data structures for maximum efficiency.

---

## 🛠️ Core Technology Stack
* **Backend:** Flask (Python 3) serving as a high-speed routing layer.
* **Logic Engine:** Compiled C binaries (`auth`, `order`, `inventory`).
* **Frontend:** Vanilla JS with Bootstrap 5, featuring a custom **VS Code-inspired** dark theme.
* **Security:** Native Intra-net hosting with **Self-Signed HTTPS/SSL** certificates.

---

## 🧠 Data Structure Architecture (The C-Engine)
Every core feature is powered by a specific data structure implementation for optimized performance:
* **Doubly Linked List (DLL):** Manages the real-time shopping cart for instantaneous item removal and updates.
* **Min-Heap:** Powers the **Admin Priority Queue**, ensuring "Morning" delivery slots are always dispatched first.
* **Circular Linked List (CLL):** Implements a round-robin delivery boy assignment system to ensure fair workload distribution.
* **Standard Queue (FIFO):** Processes incoming orders in the exact sequence they are placed.

---

## ✨ Implemented Features
* **5-Step Checkout Machine:** A seamless flow from Cart review to Delivery Slot selection and Invoice Preview.
* **JIT (Just-In-Time) Promotion:** Admin dashboard auto-promotes orders based on the current delivery slot.
* **Live PDF Engine:** Real-time generation of dark-theme receipts using `html2pdf.js`.
* **Dynamic Inventory:** Real-time stock monitoring with low-stock visual indicators.
* **Network Resilience:** Specifically configured for hosting over a local network (Wi-Fi/LAN) with encrypted traffic.

---

## 🗺️ Roadmap (Upcoming Features)
* **🚚 Delivery Boy Dashboard:** A mobile-optimized portal for agents to track assigned routes and update delivery statuses.
* **📊 Advanced Statistics:** Graphical sales analytics, inventory heatmaps, and demand forecasting.
* **🛡️ Admin Control Center:** Full CRUD management for Users, Delivery Personnel, and Shop metadata.
* **✉️ Automated Communications:** Direct-to-email PDF bills and secure **SMS OTP** authentication for logins and orders.
* **📦 Stock Alerts:** Push notifications for admins when inventory falls below a specific threshold.

---

## 🚀 Getting Started

### Prerequisites
* GCC Compiler (for C binaries)
* Python 3.x
* OpenSSL (for HTTPS certificates)

### Build Instructions
1.  **Compile the Logic Engine:** ```bash
    bash build.sh
    ```
2.  **Generate SSL Certificates (if not present):**
    ```bash
    # Follow the project documentation for self-signing instructions
    ```
3.  **Launch the HTTPS Server:**
    ```bash
    python app.py
    ```
4.  **Access on LAN:** Access via the IP provided in the terminal (e.g., `https://192.168.xx.xx:5000`).

---

## 🐞 Troubleshooting
* **Blank PDFs:** Ensure the receipt clone in JS uses hardcoded hex colors to avoid CSS variable stripping.
* **404 Errors:** Verify that the API routes in `app.py` match the `fetch` URLs in your HTML templates.
* **Syntax Errors:** Check your HTML script blocks for unclosed tags or Markdown artifacts.

---
*Developed with ❤️ by Team CodeCrafters for the SDP-1 Project.*
