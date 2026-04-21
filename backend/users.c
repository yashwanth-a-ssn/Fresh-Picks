/*
 * users.c - Fresh Picks: User Management Logic (v4 — Binary Storage Edition)
 * ===========================================================================
 * This binary handles all admin-facing user-directory commands.
 * Called by Flask via subprocess.run() like this:
 *   ./users <action> [arg1] [arg2] ...
 *
 * ALL persistent I/O is delegated to utils.c (load/save/free SLL functions).
 * Direct fopen / fgets / strtok / fprintf on data files is STRICTLY FORBIDDEN.
 *
 * OUTPUT CONTRACT:
 *   SUCCESS header line, then one data line per user:
 *     SUCCESS|<count>
 *     <user_id>|<username>|<full_name>|<email>|<phone>|<address>|<status>
 *   OR on error:
 *     ERROR|<reason>
 *
 *   Flask reads the header status, then parses every subsequent line.
 *
 * COMMANDS (argv[1]):
 *   list_users                       — Dump every user in users.dat
 *   list_users  <filter>             — "active" | "inactive" (case-insensitive)
 *   search_users <query>             — Match user_id OR full_name (substring)
 *
 * STATUS NOTE:
 *   The v4 User struct does NOT have an explicit `status` field (see models.h).
 *   We derive status from whether the password field is non-empty — an account
 *   with a blank password is treated as "inactive" (disabled/locked).
 *   This is a safe, non-breaking convention that works with the existing binary.
 *
 * COMPILE (Windows, MSVC/MinGW):
 *   gcc -Wall -Wextra -o users users.c utils.c -lm
 *
 * Team: CodeCrafters | Project: Fresh Picks | SDP-1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "models.h"   /* Struct definitions, SLL node types, macros, utils.c API */


/* ══════════════════════════════════════════════════════════════════════
 * INTERNAL HELPER: derive_status
 * PURPOSE:  Derive a display-friendly status string from the User struct.
 *           A non-empty password field = "Active" account.
 *           An empty password field    = "Inactive" (locked/disabled).
 * PARAMS:   u — pointer to a User struct
 * RETURNS:  "Active" or "Inactive" (static string — do NOT free)
 * ══════════════════════════════════════════════════════════════════════ */
static const char* derive_status(const User* u) {
    return (u->password[0] != '\0') ? "Active" : "Inactive";
}


/* ══════════════════════════════════════════════════════════════════════
 * INTERNAL HELPER: str_to_lower
 * PURPOSE:  In-place lowercase of a string (for case-insensitive compare).
 * PARAMS:   dest — destination buffer (at least `n` bytes)
 *           src  — source string
 *           n    — max bytes to copy (including NUL terminator)
 * ══════════════════════════════════════════════════════════════════════ */
static void str_to_lower(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n - 1 && src[i] != '\0'; i++) {
        dest[i] = (char)tolower((unsigned char)src[i]);
    }
    dest[i] = '\0';
}


/* ══════════════════════════════════════════════════════════════════════
 * INTERNAL HELPER: print_user_line
 * PURPOSE:  Print one pipe-delimited user record to stdout.
 *           Schema: user_id|username|full_name|email|phone|address|status
 *           The address field may contain commas but NEVER pipes — safe.
 * PARAMS:   u — pointer to a User struct
 * ══════════════════════════════════════════════════════════════════════ */
static void print_user_line(const User* u) {
    printf("%s|%s|%s|%s|%s|%s|%s\n",
        u->user_id,
        u->username,
        u->full_name,
        u->email,
        u->phone,
        u->address,
        derive_status(u)
    );
}


/* ══════════════════════════════════════════════════════════════════════
 * COMMAND: cmd_list_users
 * PURPOSE:  Walk the user SLL and print all (or filtered) user records.
 *           Filter is optional: "active" | "inactive" | NULL (all users).
 *           Filtering is done in-memory for performance — no re-read of .dat.
 * PARAMS:   filter — "active", "inactive", or NULL
 * OUTPUT:   SUCCESS|<count>\n  followed by one data line per matching user.
 *           ERROR|<reason>     if users.dat is missing or empty.
 * ══════════════════════════════════════════════════════════════════════ */
void cmd_list_users(const char* filter) {
    /* ── Step 1: Load the entire user SLL from users.dat ─────────── */
    UserNode* head = load_user_sll();
    if (!head) {
        PRINT_ERROR("No users found or users.dat is missing");
        return;
    }

    /* ── Step 2: First pass — count matching records ─────────────── */
    int match_count = 0;
    UserNode* curr  = head;

    while (curr != NULL) {
        const char* status = derive_status(&curr->data);

        if (filter == NULL) {
            match_count++;
        } else {
            /* Case-insensitive filter comparison */
            char filter_lower[MAX_STR_LEN];
            char status_lower[MAX_STR_LEN];
            str_to_lower(filter_lower, filter, MAX_STR_LEN);
            str_to_lower(status_lower, status, MAX_STR_LEN);

            if (strcmp(filter_lower, status_lower) == 0) {
                match_count++;
            }
        }
        curr = curr->next;
    }

    /* ── Step 3: Print header with total count ───────────────────── */
    printf("SUCCESS|%d\n", match_count);

    /* ── Step 4: Second pass — print each matching record ────────── */
    curr = head;
    while (curr != NULL) {
        const char* status = derive_status(&curr->data);

        if (filter == NULL) {
            print_user_line(&curr->data);
        } else {
            char filter_lower[MAX_STR_LEN];
            char status_lower[MAX_STR_LEN];
            str_to_lower(filter_lower, filter, MAX_STR_LEN);
            str_to_lower(status_lower, status, MAX_STR_LEN);

            if (strcmp(filter_lower, status_lower) == 0) {
                print_user_line(&curr->data);
            }
        }
        curr = curr->next;
    }

    /* ── Step 5: Free SLL — memory hygiene ──────────────────────── */
    free_user_sll(head);
}


/* ══════════════════════════════════════════════════════════════════════
 * COMMAND: cmd_search_users
 * PURPOSE:  Search users by user_id (exact, case-insensitive) OR
 *           by full_name (substring, case-insensitive).
 *           Filtering is done in-memory for performance.
 * PARAMS:   query — the search string (e.g. "U1003" or "Ravi")
 * OUTPUT:   SUCCESS|<count>\n  followed by one data line per match.
 *           ERROR|<reason>     if query is empty or no users exist.
 * ══════════════════════════════════════════════════════════════════════ */
void cmd_search_users(const char* query) {
    if (!query || strlen(query) == 0) {
        PRINT_ERROR("Search query cannot be empty");
        return;
    }

    /* ── Step 1: Load SLL ────────────────────────────────────────── */
    UserNode* head = load_user_sll();
    if (!head) {
        PRINT_ERROR("No users found or users.dat is missing");
        return;
    }

    /* Prepare lowercase version of query once */
    char query_lower[MAX_STR_LEN];
    str_to_lower(query_lower, query, MAX_STR_LEN);

    /* ── Step 2: First pass — count matches ──────────────────────── */
    int match_count = 0;
    UserNode* curr  = head;

    while (curr != NULL) {
        char id_lower[MAX_ID_LEN];
        char name_lower[MAX_STR_LEN];

        str_to_lower(id_lower,   curr->data.user_id,   MAX_ID_LEN);
        str_to_lower(name_lower, curr->data.full_name,  MAX_STR_LEN);

        /* Match: exact user_id OR substring in full_name */
        if (strcmp(id_lower, query_lower) == 0 ||
            strstr(name_lower, query_lower) != NULL) {
            match_count++;
        }
        curr = curr->next;
    }

    /* ── Step 3: Print header ─────────────────────────────────────── */
    printf("SUCCESS|%d\n", match_count);

    /* ── Step 4: Second pass — print matches ─────────────────────── */
    curr = head;
    while (curr != NULL) {
        char id_lower[MAX_ID_LEN];
        char name_lower[MAX_STR_LEN];

        str_to_lower(id_lower,   curr->data.user_id,   MAX_ID_LEN);
        str_to_lower(name_lower, curr->data.full_name,  MAX_STR_LEN);

        if (strcmp(id_lower, query_lower) == 0 ||
            strstr(name_lower, query_lower) != NULL) {
            print_user_line(&curr->data);
        }
        curr = curr->next;
    }

    /* ── Step 5: Free SLL ─────────────────────────────────────────── */
    free_user_sll(head);
}


/* ══════════════════════════════════════════════════════════════════════
 * COMMAND: cmd_get_user
 * PURPOSE:  Fetch a single user's full profile by user_id.
 *           Used by the "View Details" modal on the frontend.
 * PARAMS:   user_id — exact user ID string (e.g. "U1003")
 * OUTPUT:   SUCCESS|user_id|username|full_name|email|phone|address|status
 *           ERROR|<reason>  if not found.
 * ══════════════════════════════════════════════════════════════════════ */
void cmd_get_user(const char* user_id) {
    if (!user_id || strlen(user_id) == 0) {
        PRINT_ERROR("user_id is required");
        return;
    }

    UserNode* head = load_user_sll();
    if (!head) {
        PRINT_ERROR("No users found or users.dat is missing");
        return;
    }

    UserNode* curr = head;
    while (curr != NULL) {
        if (strcmp(curr->data.user_id, user_id) == 0) {
            /* Found — capture fields, free SLL, then print */
            User u = curr->data;   /* Copy struct to survive free_user_sll */
            free_user_sll(head);
            printf("SUCCESS|%s|%s|%s|%s|%s|%s|%s\n",
                u.user_id,
                u.username,
                u.full_name,
                u.email,
                u.phone,
                u.address,
                derive_status(&u)
            );
            return;
        }
        curr = curr->next;
    }

    free_user_sll(head);
    PRINT_ERROR("User not found");
}


/* ══════════════════════════════════════════════════════════════════════
 * ENTRY POINT: main
 * Dispatches to the correct command function based on argv[1].
 *
 * USAGE:
 *   ./users list_users                  → all users
 *   ./users list_users active           → only Active users
 *   ./users list_users inactive         → only Inactive users
 *   ./users search_users <query>        → search by ID or name
 *   ./users get_user <user_id>          → single user full profile
 * ══════════════════════════════════════════════════════════════════════ */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        PRINT_ERROR("Usage: ./users <action> [args...]");
        return 1;
    }

    const char* action = argv[1];

    /* ── list_users [filter] ─────────────────────────────────────── */
    if (strcmp(action, "list_users") == 0) {
        const char* filter = (argc >= 3) ? argv[2] : NULL;
        cmd_list_users(filter);

    /* ── search_users <query> ─────────────────────────────────────── */
    } else if (strcmp(action, "search_users") == 0) {
        if (argc < 3) {
            PRINT_ERROR("search_users requires a query argument");
            return 1;
        }
        cmd_search_users(argv[2]);

    /* ── get_user <user_id> ───────────────────────────────────────── */
    } else if (strcmp(action, "get_user") == 0) {
        if (argc < 3) {
            PRINT_ERROR("get_user requires a user_id argument");
            return 1;
        }
        cmd_get_user(argv[2]);

    /* ── Unknown action ───────────────────────────────────────────── */
    } else {
        PRINT_ERROR("Unknown action. Valid: list_users, search_users, get_user");
        return 1;
    }

    return 0;
}