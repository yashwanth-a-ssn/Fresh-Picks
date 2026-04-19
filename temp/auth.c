/*
 * auth.c - Fresh Picks: Authentication & Profile Logic (v4 — Binary Storage)
 * ===========================================================================
 * This binary handles all authentication and profile management commands.
 * Called by Flask via subprocess.run() like this:
 *   ./auth <action> [arg1] [arg2] ...
 *
 * ALL persistent I/O is delegated to utils.c (load/save/free SLL functions).
 * Direct fopen / fgets / strtok / fprintf on data files is STRICTLY FORBIDDEN.
 *
 * OUTPUT CONTRACT (unchanged from v3):
 *   Always "SUCCESS|<data>"  or  "ERROR|<reason>"
 *   Flask reads this output and sends it back to the frontend as JSON.
 *
 * COMMANDS (argv[1]):
 *   login_user       <username> <password>
 *   login_admin      <username> <password>
 *   register         <username> <password> <full_name> <email> <phone> <address>
 *   get_profile      <user_id>
 *   change_pass_user <user_id> <old_password> <new_password>
 *   change_pass_admin<admin_id> <old_password> <new_password>
 *   update_profile   <user_id> <field> <new_value>
 *
 * Team: CodeCrafters | Project: Fresh Picks | SDP-1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "models.h"   /* Struct definitions, SLL node types, macros, utils.c API */


/* ══════════════════════════════════════════════════════════════════════
 * FUNCTION: cmd_login_user
 * PURPOSE:  Validate user credentials against the users .dat store.
 * PARAMS:   username — the username submitted, password — the password submitted
 * OUTPUT:   SUCCESS|user_id   OR   ERROR|reason
 * SCHEMA:   SUCCESS|user_id
 * ══════════════════════════════════════════════════════════════════════ */
void cmd_login_user(const char *username, const char *password) {
    if (!username || strlen(username) == 0) { PRINT_ERROR("Username required"); return; }
    if (!password || strlen(password) == 0) { PRINT_ERROR("Password required"); return; }

    UserNode *head = load_user_sll();
    if (!head) { PRINT_ERROR("No users found"); return; }

    UserNode *curr = head;
    while (curr != NULL) {
        if (strcmp(curr->data.username, username) == 0 &&
            strcmp(curr->data.password, password) == 0) {
            char id_copy[MAX_ID_LEN];
            strncpy(id_copy, curr->data.user_id, MAX_ID_LEN - 1);
            id_copy[MAX_ID_LEN - 1] = '\0';
            free_user_sll(head);
            PRINT_SUCCESS(id_copy);
            return;
        }
        curr = curr->next;
    }

    free_user_sll(head);
    PRINT_ERROR("Invalid username or password");
}


/* ══════════════════════════════════════════════════════════════════════
 * FUNCTION: cmd_login_admin
 * PURPOSE:  Validate admin credentials against the admins .dat store.
 * PARAMS:   username — admin username, password — admin password
 * OUTPUT:   SUCCESS|admin_id|admin_name   OR   ERROR|reason
 * SCHEMA:   SUCCESS|admin_id|admin_name
 * ══════════════════════════════════════════════════════════════════════ */
void cmd_login_admin(const char *username, const char *password) {
    if (!username || strlen(username) == 0) { PRINT_ERROR("Username required"); return; }
    if (!password || strlen(password) == 0) { PRINT_ERROR("Password required"); return; }

    AdminNode *head = load_admin_sll();
    if (!head) { PRINT_ERROR("Admin database not found"); return; }

    AdminNode *curr = head;
    while (curr != NULL) {
        if (strcmp(curr->data.username, username) == 0 &&
            strcmp(curr->data.password, password) == 0) {
            /* Capture before freeing SLL */
            char id_copy[MAX_ID_LEN], name_copy[MAX_STR_LEN];
            strncpy(id_copy,   curr->data.admin_id,   MAX_ID_LEN  - 1); id_copy[MAX_ID_LEN   - 1] = '\0';
            strncpy(name_copy, curr->data.admin_name, MAX_STR_LEN - 1); name_copy[MAX_STR_LEN - 1] = '\0';
            free_admin_sll(head);
            printf("SUCCESS|%s|%s\n", id_copy, name_copy);
            return;
        }
        curr = curr->next;
    }

    free_admin_sll(head);
    PRINT_ERROR("Invalid admin credentials");
}


/* ══════════════════════════════════════════════════════════════════════
 * FUNCTION: cmd_register_user
 * PURPOSE:  Check username uniqueness, generate next user ID, append
 *           new User struct to the SLL, then persist via save_user_sll.
 * PARAMS:   username, password, full_name, email, phone, address
 * OUTPUT:   SUCCESS|new_user_id   OR   ERROR|reason
 * SCHEMA:   SUCCESS|user_id
 * ══════════════════════════════════════════════════════════════════════ */
void cmd_register_user(const char *username, const char *password,
                       const char *full_name,  const char *email,
                       const char *phone,      const char *address) {
    if (!username  || strlen(username)  == 0) { PRINT_ERROR("Username required");  return; }
    if (!password  || strlen(password)  == 0) { PRINT_ERROR("Password required");  return; }
    if (!full_name || strlen(full_name) == 0) { PRINT_ERROR("Full name required"); return; }
    if (!email     || strlen(email)     == 0) { PRINT_ERROR("Email required");     return; }
    if (!phone     || strlen(phone)     == 0) { PRINT_ERROR("Phone required");     return; }
    if (!address   || strlen(address)   == 0) { PRINT_ERROR("Address required");   return; }

    /* Load existing users — needed for uniqueness check and ID generation */
    UserNode *head = load_user_sll();

    /* Check for duplicate username */
    UserNode *curr = head;
    while (curr != NULL) {
        if (strcmp(curr->data.username, username) == 0) {
            free_user_sll(head);
            PRINT_ERROR("Username already exists");
            return;
        }
        curr = curr->next;
    }

    /* Generate next user ID: U1001, U1002, … */
    int count = sll_count_users(head);
    int next_num = 1001 + count;
    char new_id[MAX_ID_LEN];
    snprintf(new_id, MAX_ID_LEN, "U%d", next_num);

    /* Build the new User struct */
    User new_user;
    memset(&new_user, 0, sizeof(User));
    strncpy(new_user.user_id,   new_id,    MAX_ID_LEN  - 1);
    strncpy(new_user.username,  username,  MAX_STR_LEN - 1);
    strncpy(new_user.password,  password,  MAX_STR_LEN - 1);
    strncpy(new_user.full_name, full_name, MAX_STR_LEN - 1);
    strncpy(new_user.email,     email,     MAX_STR_LEN - 1);
    strncpy(new_user.phone,     MAX_STR_LEN - 1 < 20 ? new_user.phone : new_user.phone,
            MAX_STR_LEN - 1);
    strncpy(new_user.phone,     phone,     MAX_STR_LEN - 1);
    strncpy(new_user.address,   address,   MAX_ADDR_LEN - 1);

    /* Append new node to the tail of the SLL */
    UserNode *new_node = (UserNode *)malloc(sizeof(UserNode));
    if (!new_node) {
        free_user_sll(head);
        PRINT_ERROR("Memory allocation failed");
        return;
    }
    new_node->data = new_user;
    new_node->next = NULL;

    if (!head) {
        head = new_node;
    } else {
        UserNode *tail = head;
        while (tail->next) tail = tail->next;
        tail->next = new_node;
    }

    save_user_sll(head);
    free_user_sll(head);
    PRINT_SUCCESS(new_id);
}


/* ══════════════════════════════════════════════════════════════════════
 * FUNCTION: cmd_get_profile
 * PURPOSE:  Find user by ID and return their info (password excluded).
 * PARAMS:   user_id — the ID to look up
 * OUTPUT:   SUCCESS|user_id|username|full_name|email|phone|address
 *           OR  ERROR|reason
 * SCHEMA:   SUCCESS|user_id|username|full_name|email|phone|address
 * ══════════════════════════════════════════════════════════════════════ */
void cmd_get_profile(const char *user_id) {
    if (!user_id || strlen(user_id) == 0) { PRINT_ERROR("User ID required"); return; }

    UserNode *head = load_user_sll();
    if (!head) { PRINT_ERROR("No users found"); return; }

    UserNode *curr = head;
    while (curr != NULL) {
        if (strcmp(curr->data.user_id, user_id) == 0) {
            /* Capture all fields before freeing */
            char uid[MAX_ID_LEN], uname[MAX_STR_LEN], fname[MAX_STR_LEN];
            char em[MAX_STR_LEN], ph[MAX_STR_LEN], addr[MAX_ADDR_LEN];
            strncpy(uid,   curr->data.user_id,   MAX_ID_LEN   - 1); uid[MAX_ID_LEN   - 1] = '\0';
            strncpy(uname, curr->data.username,  MAX_STR_LEN  - 1); uname[MAX_STR_LEN - 1] = '\0';
            strncpy(fname, curr->data.full_name, MAX_STR_LEN  - 1); fname[MAX_STR_LEN - 1] = '\0';
            strncpy(em,    curr->data.email,     MAX_STR_LEN  - 1); em[MAX_STR_LEN    - 1] = '\0';
            strncpy(ph,    curr->data.phone,     MAX_STR_LEN  - 1); ph[MAX_STR_LEN    - 1] = '\0';
            strncpy(addr,  curr->data.address,   MAX_ADDR_LEN - 1); addr[MAX_ADDR_LEN - 1] = '\0';
            free_user_sll(head);
            printf("SUCCESS|%s|%s|%s|%s|%s|%s\n", uid, uname, fname, em, ph, addr);
            return;
        }
        curr = curr->next;
    }

    free_user_sll(head);
    PRINT_ERROR("User not found");
}


/* ══════════════════════════════════════════════════════════════════════
 * FUNCTION: cmd_change_pass_user
 * PURPOSE:  Verify old password for a user, then update to new password
 *           in the SLL and persist.
 * PARAMS:   user_id — target user, old_password — must match stored value,
 *           new_password — replacement
 * OUTPUT:   SUCCESS|Password changed successfully   OR   ERROR|reason
 * SCHEMA:   SUCCESS|message
 * ══════════════════════════════════════════════════════════════════════ */
void cmd_change_pass_user(const char *user_id, const char *old_password,
                          const char *new_password) {
    if (!user_id      || strlen(user_id)      == 0) { PRINT_ERROR("User ID required");       return; }
    if (!old_password || strlen(old_password) == 0) { PRINT_ERROR("Old password required");  return; }
    if (!new_password || strlen(new_password) == 0) { PRINT_ERROR("New password required");  return; }

    UserNode *head = load_user_sll();
    if (!head) { PRINT_ERROR("No users found"); return; }

    UserNode *curr = head;
    int found = 0;
    while (curr != NULL) {
        if (strcmp(curr->data.user_id, user_id) == 0) {
            if (strcmp(curr->data.password, old_password) != 0) {
                free_user_sll(head);
                PRINT_ERROR("Old password is incorrect");
                return;
            }
            strncpy(curr->data.password, new_password, MAX_STR_LEN - 1);
            curr->data.password[MAX_STR_LEN - 1] = '\0';
            found = 1;
            break;
        }
        curr = curr->next;
    }

    if (!found) {
        free_user_sll(head);
        PRINT_ERROR("User not found");
        return;
    }

    save_user_sll(head);
    free_user_sll(head);
    PRINT_SUCCESS("Password changed successfully");
}


/* ══════════════════════════════════════════════════════════════════════
 * FUNCTION: cmd_change_pass_admin
 * PURPOSE:  Verify old password for an admin, then update to new password
 *           in the SLL and persist.
 * PARAMS:   admin_id — target admin, old_password — must match stored value,
 *           new_password — replacement
 * OUTPUT:   SUCCESS|Admin password changed successfully   OR   ERROR|reason
 * SCHEMA:   SUCCESS|message
 * ══════════════════════════════════════════════════════════════════════ */
void cmd_change_pass_admin(const char *admin_id, const char *old_password,
                           const char *new_password) {
    if (!admin_id     || strlen(admin_id)     == 0) { PRINT_ERROR("Admin ID required");      return; }
    if (!old_password || strlen(old_password) == 0) { PRINT_ERROR("Old password required");  return; }
    if (!new_password || strlen(new_password) == 0) { PRINT_ERROR("New password required");  return; }

    AdminNode *head = load_admin_sll();
    if (!head) { PRINT_ERROR("Admin database not found"); return; }

    AdminNode *curr = head;
    int found = 0;
    while (curr != NULL) {
        if (strcmp(curr->data.admin_id, admin_id) == 0) {
            if (strcmp(curr->data.password, old_password) != 0) {
                free_admin_sll(head);
                PRINT_ERROR("Old password is incorrect");
                return;
            }
            strncpy(curr->data.password, new_password, MAX_STR_LEN - 1);
            curr->data.password[MAX_STR_LEN - 1] = '\0';
            found = 1;
            break;
        }
        curr = curr->next;
    }

    if (!found) {
        free_admin_sll(head);
        PRINT_ERROR("Admin not found");
        return;
    }

    save_admin_sll(head);
    free_admin_sll(head);
    PRINT_SUCCESS("Admin password changed successfully");
}


/* ══════════════════════════════════════════════════════════════════════
 * FUNCTION: cmd_update_profile
 * PURPOSE:  Update a single profile field (full_name, email, phone, or
 *           address) for a given user_id, then persist.
 * PARAMS:   user_id — target user, field — one of: full_name / email /
 *           phone / address, new_value — replacement value
 * OUTPUT:   SUCCESS|Profile updated   OR   ERROR|reason
 * SCHEMA:   SUCCESS|message
 * ══════════════════════════════════════════════════════════════════════ */
void cmd_update_profile(const char *user_id, const char *field,
                        const char *new_value) {
    if (!user_id   || strlen(user_id)   == 0) { PRINT_ERROR("User ID required");   return; }
    if (!field     || strlen(field)     == 0) { PRINT_ERROR("Field required");      return; }
    if (!new_value || strlen(new_value) == 0) { PRINT_ERROR("New value required");  return; }

    /* Validate field name before loading */
    int valid_field = (strcmp(field, "full_name") == 0 ||
                       strcmp(field, "email")     == 0 ||
                       strcmp(field, "phone")     == 0 ||
                       strcmp(field, "address")   == 0);
    if (!valid_field) { PRINT_ERROR("Unknown field"); return; }

    UserNode *head = load_user_sll();
    if (!head) { PRINT_ERROR("No users found"); return; }

    UserNode *curr = head;
    int found = 0;
    while (curr != NULL) {
        if (strcmp(curr->data.user_id, user_id) == 0) {
            if (strcmp(field, "full_name") == 0)
                strncpy(curr->data.full_name, new_value, MAX_STR_LEN  - 1);
            else if (strcmp(field, "email") == 0)
                strncpy(curr->data.email,     new_value, MAX_STR_LEN  - 1);
            else if (strcmp(field, "phone") == 0)
                strncpy(curr->data.phone,     new_value, MAX_STR_LEN  - 1);
            else /* address */
                strncpy(curr->data.address,   new_value, MAX_ADDR_LEN - 1);
            found = 1;
            break;
        }
        curr = curr->next;
    }

    if (!found) {
        free_user_sll(head);
        PRINT_ERROR("User not found");
        return;
    }

    save_user_sll(head);
    free_user_sll(head);
    PRINT_SUCCESS("Profile updated");
}


/* ══════════════════════════════════════════════════════════════════════
 * MAIN — Command Dispatcher
 * PURPOSE:  Parse argv[1] and route to the appropriate cmd_* function.
 *           Guard clause on argc before any dispatch.
 * ══════════════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        PRINT_ERROR("No action specified. Usage: ./auth <action> [args...]");
        return 1;
    }

    const char *action = argv[1];

    if (strcmp(action, "login_user") == 0) {
        /* argv: auth login_user <username> <password>   argc >= 4 */
        if (argc < 4) { PRINT_ERROR("Usage: login_user <username> <password>"); return 1; }
        cmd_login_user(argv[2], argv[3]);

    } else if (strcmp(action, "login_admin") == 0) {
        /* argv: auth login_admin <username> <password>  argc >= 4 */
        if (argc < 4) { PRINT_ERROR("Usage: login_admin <username> <password>"); return 1; }
        cmd_login_admin(argv[2], argv[3]);

    } else if (strcmp(action, "register") == 0) {
        /* argv: auth register <username> <password> <full_name> <email> <phone> <address>
         * idx:   [0]   [1]      [2]        [3]        [4]         [5]    [6]     [7]
         * argc >= 8 */
        if (argc < 8) { PRINT_ERROR("Usage: register <username> <password> <full_name> <email> <phone> <address>"); return 1; }
        cmd_register_user(argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]);

    } else if (strcmp(action, "get_profile") == 0) {
        /* argv: auth get_profile <user_id>   argc >= 3 */
        if (argc < 3) { PRINT_ERROR("Usage: get_profile <user_id>"); return 1; }
        cmd_get_profile(argv[2]);

    } else if (strcmp(action, "change_pass_user") == 0) {
        /* argv: auth change_pass_user <user_id> <old_pass> <new_pass>   argc >= 5 */
        if (argc < 5) { PRINT_ERROR("Usage: change_pass_user <user_id> <old_pass> <new_pass>"); return 1; }
        cmd_change_pass_user(argv[2], argv[3], argv[4]);

    } else if (strcmp(action, "change_pass_admin") == 0) {
        /* argv: auth change_pass_admin <admin_id> <old_pass> <new_pass>   argc >= 5 */
        if (argc < 5) { PRINT_ERROR("Usage: change_pass_admin <admin_id> <old_pass> <new_pass>"); return 1; }
        cmd_change_pass_admin(argv[2], argv[3], argv[4]);

    } else if (strcmp(action, "update_profile") == 0) {
        /* argv: auth update_profile <user_id> <field> <new_value>   argc >= 5 */
        if (argc < 5) { PRINT_ERROR("Usage: update_profile <user_id> <field> <new_value>"); return 1; }
        cmd_update_profile(argv[2], argv[3], argv[4]);

    } else {
        char err[MAX_STR_LEN];
        snprintf(err, sizeof(err), "Unknown action: %s", action);
        PRINT_ERROR(err);
        return 1;
    }

    return 0;
}
