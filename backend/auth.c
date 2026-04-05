/*
 * auth.c - Fresh Picks: Authentication & Profile Logic
 * =====================================================
 * This is the main C backend file. It handles:
 *   1. User Login           (login_user)
 *   2. Admin Login          (login_admin)
 *   3. User Registration    (register)
 *   4. Fetch User Profile   (get_profile)
 *   5. Change User Password (change_pass_user)
 *   6. Change Admin Password(change_pass_admin)
 *
 * HOW IT WORKS:
 *   Flask calls this compiled binary via subprocess:
 *     ./auth <action> [arg1] [arg2] ...
 *
 *   ALL output goes to stdout in format:
 *     SUCCESS|<data>   or   ERROR|<reason>
 *   Flask reads this output and sends it back as JSON.
 *
 * DB SCHEMA — users.txt (7 fields):
 *   user_id|username|password|full_name|email|phone|address
 *   Field indices (0-based after strtok):
 *     [0] user_id   [1] username  [2] password
 *     [3] full_name [4] email     [5] phone    [6] address
 *
 * DB SCHEMA — admin_creds.txt (5 fields):
 *   admin_id|username|password|admin_name|email
 *
 * Team: CodeCrafters | Project: Fresh Picks | SDP-1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "models.h"

/* ─────────────────────────────────────────────────────────────
   HELPER: trim_newline
   PURPOSE: Remove the '\n' from end of string.
            fgets() keeps the newline; this cleans it up.
   PARAMS:  str - the string to modify in-place
   ──────────────────────────────────────────────────────────── */
void trim_newline(char *str) {
    int len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0'; /* Replace newline with null terminator */
    }
}

/* ─────────────────────────────────────────────────────────────
   HELPER: generate_user_id
   PURPOSE: Count existing users and generate next ID.
            Example: if 2 users exist, returns "U003"
   PARAMS:  out_id - output buffer to store the new ID
   ──────────────────────────────────────────────────────────── */
void generate_user_id(char *out_id) {
    FILE *fp = fopen(USERS_FILE, "r");
    int count = 0;
    char line[MAX_LINE_LEN];

    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp) != NULL) {
            if (strlen(line) > 1) count++; /* Count non-empty lines */
        }
        fclose(fp);
    }

    /* Zero-pad to 3 digits: U001, U002, ... */
    // sprintf() => writes the fromatted data into a string ptr
    // out_id => string ptr
    sprintf(out_id, "U%03d", count + 1);
}

/* ─────────────────────────────────────────────────────────────
   FUNCTION: login_user
   PURPOSE:  Validate user credentials against users.txt
   PARAMS:   username, password - what the user typed
   OUTPUT:   SUCCESS|user_id  OR  ERROR|message

   SCHEMA:   user_id|username|password|full_name|email|phone|address
             We only need fields [0]=user_id, [1]=username, [2]=password
             to authenticate. The rest are skipped here.
   ──────────────────────────────────────────────────────────── */
void login_user(const char *username, const char *password) {
    FILE *fp = fopen(USERS_FILE, "r");
    if (fp == NULL) { PRINT_ERROR("Database file not found"); return; }

    char line[MAX_LINE_LEN];

    while (fgets(line, sizeof(line), fp) != NULL) {
        trim_newline(line);
        if (strlen(line) < 3) continue; /* Skip blank lines */

        char temp[MAX_LINE_LEN];
        strcpy(temp, line); /* strtok modifies string, use a copy */

        // In the first call of strtok pass the str
        // Pass NULL for consecutive calls, it will fetch automatically.
        char *t_id    = strtok(temp, DELIMITER); /* field [0]: user_id  */
        char *t_uname = strtok(NULL, DELIMITER); /* field [1]: username */
        char *t_pass  = strtok(NULL, DELIMITER); /* field [2]: password */

        if (!t_id || !t_uname || !t_pass) continue; /* Malformed line */

        if (strcmp(t_uname, username) == 0 && strcmp(t_pass, password) == 0) {
            fclose(fp);
            PRINT_SUCCESS(t_id); /* Return the user's ID to Flask */
            return;
        }
    }

    fclose(fp);
    PRINT_ERROR("Invalid username or password");
}

/* ─────────────────────────────────────────────────────────────
   FUNCTION: login_admin
   PURPOSE:  Validate admin credentials against admin_creds.txt
   PARAMS:   username, password - what the admin typed
   OUTPUT:   SUCCESS|admin_id|admin_name  OR  ERROR|message

   SCHEMA:   admin_id|username|password|admin_name|email
             We read all 5 fields so we can return admin_name
             to Flask, which stores it in the session.
   ──────────────────────────────────────────────────────────── */
void login_admin(const char *username, const char *password) {
    FILE *fp = fopen(ADMIN_FILE, "r");
    if (fp == NULL) { PRINT_ERROR("Admin database not found"); return; }

    char line[MAX_LINE_LEN];
    char temp[MAX_LINE_LEN];

    /*
     * WHY loop instead of reading just one line?
     * The schema note says admin_creds.txt supports multiple admin accounts.
     * We loop through every row to find the matching username+password pair.
     */
    while (fgets(line, sizeof(line), fp) != NULL) {
        trim_newline(line);
        if (strlen(line) < 3) continue; /* Skip blank/empty lines */

        strcpy(temp, line); /* strtok will destroy temp; line stays intact */

        /* Parse all 5 fields: admin_id|username|password|admin_name|email */
        char *t_admin_id   = strtok(temp, DELIMITER); /* field [0]: admin_id   */
        char *t_user       = strtok(NULL, DELIMITER); /* field [1]: username   */
        char *t_pass       = strtok(NULL, DELIMITER); /* field [2]: password   */
        char *t_admin_name = strtok(NULL, DELIMITER); /* field [3]: admin_name */
        /* field [4] email not needed for login, intentionally skipped */

        if (!t_admin_id || !t_user || !t_pass) continue; /* Malformed row */

        if (strcmp(t_user, username) == 0 && strcmp(t_pass, password) == 0) {
            fclose(fp);
            /*
             * Return admin_id AND admin_name pipe-separated.
             * Flask will split on '|' to extract both fields and
             * store them in the session for the dashboard greeting.
             * Format: SUCCESS|admin_id|admin_name
             */
            printf("SUCCESS|%s|%s\n",
                   t_admin_id,
                   t_admin_name ? t_admin_name : "Admin");
            return;
        }
    }

    fclose(fp);
    PRINT_ERROR("Invalid admin credentials");
}

/* ─────────────────────────────────────────────────────────────
   FUNCTION: register_user
   PURPOSE:  Add a new user to users.txt.
             First checks username is unique, then appends.
   PARAMS:   username, password, full_name, email, phone, address
   OUTPUT:   SUCCESS|new_user_id  OR  ERROR|message

   SCHEMA:   user_id|username|password|full_name|email|phone|address
             We write all 7 fields. Previously only 6 were written
             (email was missing), causing a segfault / field misalignment
             when get_profile tried to read field [4] (email) and got
             phone data instead. This is the root cause of the bug.
   ──────────────────────────────────────────────────────────── */
void register_user(const char *username, const char *password,
                   const char *full_name, const char *email,
                   const char *phone, const char *address) {

    /* Step 1: Check for duplicate username */
    FILE *fp = fopen(USERS_FILE, "r");
    char line[MAX_LINE_LEN];

    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp) != NULL) {
            trim_newline(line);
            if (strlen(line) < 3) continue;

            char temp[MAX_LINE_LEN];
            strcpy(temp, line);
            strtok(temp, DELIMITER);                     /* skip user_id  [0] */
            char *t_u = strtok(NULL, DELIMITER);         /* get username  [1] */

            if (t_u && strcmp(t_u, username) == 0) {
                fclose(fp);
                PRINT_ERROR("Username already exists");
                return;
            }
        }
        fclose(fp);
    }

    /* Step 2: Generate unique ID */
    char new_id[MAX_ID_LEN];
    generate_user_id(new_id);

    /* Step 3: Append to users.txt with ALL 7 fields
     *
     * WHY 7 fields?
     *   The schema is: user_id|username|password|full_name|email|phone|address
     *   If we write fewer fields, every subsequent strtok call in get_profile
     *   will be off by one — e.g., phone data lands in the email slot.
     *   That misalignment is what caused the segmentation fault.
     */
    fp = fopen(USERS_FILE, "a"); /* Append mode */
    if (fp == NULL) { PRINT_ERROR("Could not write to database"); return; }

    fprintf(fp, "%s|%s|%s|%s|%s|%s|%s\n",
            new_id,    /* [0] user_id   */
            username,  /* [1] username  */
            password,  /* [2] password  */
            full_name, /* [3] full_name */
            email,     /* [4] email     */
            phone,     /* [5] phone     */
            address);  /* [6] address   */
    fclose(fp);

    PRINT_SUCCESS(new_id); /* Return new user's ID */
}

/* ─────────────────────────────────────────────────────────────
   FUNCTION: get_profile
   PURPOSE:  Find user by ID and return their info (no password).
   PARAMS:   user_id - the ID to search for
   OUTPUT:   SUCCESS|user_id|username|full_name|email|phone|address
             OR  ERROR|message

   SCHEMA:   user_id|username|password|full_name|email|phone|address
             We tokenize all 7 fields and skip [2] (password) for security.
             The remaining 6 fields are returned to Flask as pipe-delimited data.
   ──────────────────────────────────────────────────────────── */
void get_profile(const char *user_id) {
    FILE *fp = fopen(USERS_FILE, "r");
    if (fp == NULL) { PRINT_ERROR("Database file not found"); return; }

    char line[MAX_LINE_LEN];

    while (fgets(line, sizeof(line), fp) != NULL) {
        trim_newline(line);
        if (strlen(line) < 3) continue;

        char temp[MAX_LINE_LEN];
        strcpy(temp, line);

        /*
         * Tokenize all 7 fields in order.
         * This must exactly match the write order in register_user().
         * If register_user() ever writes more/fewer fields, update here too.
         */
        char *t_id    = strtok(temp, DELIMITER); /* [0] user_id   */
        char *t_u     = strtok(NULL, DELIMITER); /* [1] username  */
        char *t_pass  = strtok(NULL, DELIMITER); /* [2] password  — extracted but NOT returned */
        char *t_name  = strtok(NULL, DELIMITER); /* [3] full_name */
        char *t_email = strtok(NULL, DELIMITER); /* [4] email     */
        char *t_ph    = strtok(NULL, DELIMITER); /* [5] phone     */
        char *t_addr  = strtok(NULL, DELIMITER); /* [6] address   */

        /*
         * (void)t_pass: We intentionally discard the password.
         * Casting to void suppresses the "unused variable" compiler warning
         * while making it clear this is a deliberate security decision.
         */
        (void)t_pass;

        if (!t_id) continue;

        if (strcmp(t_id, user_id) == 0) {
            fclose(fp);
            /*
             * Return 6 fields (no password).
             * Flask's api_get_profile splits on '|' to build the JSON response.
             * Order must match what Flask expects:
             *   parts[0]=user_id, [1]=username, [2]=full_name,
             *   [3]=email, [4]=phone, [5]=address
             */
            printf("SUCCESS|%s|%s|%s|%s|%s|%s\n",
                   t_id,
                   t_u     ? t_u     : "",
                   t_name  ? t_name  : "",
                   t_email ? t_email : "",
                   t_ph    ? t_ph    : "",
                   t_addr  ? t_addr  : "");
            return;
        }
    }

    fclose(fp);
    PRINT_ERROR("User not found");
}

/* ─────────────────────────────────────────────────────────────
   FUNCTION: change_pass_user
   PURPOSE:  Verify old password, then rewrite users.txt with
             the updated password for that user_id.
   PARAMS:   user_id, old_password, new_password
   OUTPUT:   SUCCESS|Password changed  OR  ERROR|message

   SCHEMA:   user_id|username|password|full_name|email|phone|address
             We tokenize all 7 fields, verify old password in [2],
             then rebuild the line with new_password in slot [2].
             All 7 fields must be written back or data will be lost.
   ──────────────────────────────────────────────────────────── */
void change_pass_user(const char *user_id, const char *old_password,
                      const char *new_password) {
    FILE *fp = fopen(USERS_FILE, "r");
    if (fp == NULL) { PRINT_ERROR("Database file not found"); return; }

    /* Step 1: Load all lines into memory */
    char lines[100][MAX_LINE_LEN];
    int count = 0;
    int found = 0;
    char line[MAX_LINE_LEN];

    while (fgets(line, sizeof(line), fp) != NULL) {
        trim_newline(line);
        if (strlen(line) > 1) {
            strcpy(lines[count++], line);
        }
    }
    fclose(fp);

    /* Step 2: Find target user and update password */
    for (int i = 0; i < count; i++) {
        char temp[MAX_LINE_LEN];
        strcpy(temp, lines[i]);

        /* Tokenize all 7 fields */
        char *t_id    = strtok(temp, DELIMITER); /* [0] user_id   */
        char *t_u     = strtok(NULL, DELIMITER); /* [1] username  */
        char *t_pass  = strtok(NULL, DELIMITER); /* [2] password  */
        char *t_name  = strtok(NULL, DELIMITER); /* [3] full_name */
        char *t_email = strtok(NULL, DELIMITER); /* [4] email     */
        char *t_ph    = strtok(NULL, DELIMITER); /* [5] phone     */
        char *t_addr  = strtok(NULL, DELIMITER); /* [6] address   */

        if (!t_id) continue;

        if (strcmp(t_id, user_id) == 0) {
            if (!t_pass || strcmp(t_pass, old_password) != 0) {
                PRINT_ERROR("Old password is incorrect");
                return;
            }
            found = 1;
            /*
             * Rebuild this line with new_password replacing [2].
             * ALL 7 fields must be written back.
             * Omitting email (field [4]) here would corrupt the DB.
             */
            sprintf(lines[i], "%s|%s|%s|%s|%s|%s|%s",
                    t_id,
                    t_u     ? t_u     : "",
                    new_password,
                    t_name  ? t_name  : "",
                    t_email ? t_email : "",
                    t_ph    ? t_ph    : "",
                    t_addr  ? t_addr  : "");
            break;
        }
    }

    if (!found) { PRINT_ERROR("User not found"); return; }

    /* Step 3: Rewrite entire file */
    fp = fopen(USERS_FILE, "w");
    if (fp == NULL) { PRINT_ERROR("Could not update database"); return; }

    for (int i = 0; i < count; i++) {
        fprintf(fp, "%s\n", lines[i]);
    }
    fclose(fp);

    PRINT_SUCCESS("Password changed successfully");
}

/* ─────────────────────────────────────────────────────────────
   FUNCTION: change_pass_admin
   PURPOSE:  Verify old admin password and rewrite admin_creds.txt
   PARAMS:   admin_id, old_password, new_password
   OUTPUT:   SUCCESS|Admin password changed  OR  ERROR|message

   SCHEMA:   admin_id|username|password|admin_name|email
             We load all rows, find the matching admin_id, verify
             old password, then rewrite all 5 fields with the new one.
   ──────────────────────────────────────────────────────────── */
void change_pass_admin(const char *admin_id, const char *old_password,
                       const char *new_password) {
    FILE *fp = fopen(ADMIN_FILE, "r");
    if (fp == NULL) { PRINT_ERROR("Admin database not found"); return; }

    /* Step 1: Load all admin rows into memory */
    char lines[20][MAX_LINE_LEN]; /* Admins are few; 20 is a safe upper bound */
    int count = 0;
    int found = 0;
    char line[MAX_LINE_LEN];

    while (fgets(line, sizeof(line), fp) != NULL) {
        trim_newline(line);
        if (strlen(line) > 1) {
            strcpy(lines[count++], line);
        }
    }
    fclose(fp);

    /* Step 2: Find matching admin_id and verify old password */
    for (int i = 0; i < count; i++) {
        char temp[MAX_LINE_LEN];
        strcpy(temp, lines[i]);

        /* Parse all 5 fields */
        char *t_aid   = strtok(temp, DELIMITER); /* [0] admin_id   */
        char *t_u     = strtok(NULL, DELIMITER); /* [1] username   */
        char *t_pass  = strtok(NULL, DELIMITER); /* [2] password   */
        char *t_name  = strtok(NULL, DELIMITER); /* [3] admin_name */
        char *t_email = strtok(NULL, DELIMITER); /* [4] email      */

        if (!t_aid) continue;

        if (strcmp(t_aid, admin_id) == 0) {
            if (!t_pass || strcmp(t_pass, old_password) != 0) {
                PRINT_ERROR("Old password is incorrect");
                return;
            }
            found = 1;
            /* Rebuild with new password; preserve all 5 fields */
            sprintf(lines[i], "%s|%s|%s|%s|%s",
                    t_aid,
                    t_u     ? t_u     : "",
                    new_password,
                    t_name  ? t_name  : "",
                    t_email ? t_email : "");
            break;
        }
    }

    if (!found) { PRINT_ERROR("Admin not found"); return; }

    /* Step 3: Rewrite entire admin file */
    fp = fopen(ADMIN_FILE, "w");
    if (fp == NULL) { PRINT_ERROR("Could not update admin database"); return; }

    for (int i = 0; i < count; i++) {
        fprintf(fp, "%s\n", lines[i]);
    }
    fclose(fp);

    PRINT_SUCCESS("Admin password changed successfully");
}

/* ─────────────────────────────────────────────────────────────
   MAIN - Entry Point
   PURPOSE: Reads argv to determine action, then calls the
            appropriate function above.
   ──────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        PRINT_ERROR("No action specified");
        return 1;
    }

    char *action = argv[1]; /* The action string, e.g. "login_user" */

    if (strcmp(action, "login_user") == 0) {
        if (argc < 4) { PRINT_ERROR("Missing args"); return 1; }
        login_user(argv[2], argv[3]);
    }
    else if (strcmp(action, "login_admin") == 0) {
        if (argc < 4) { PRINT_ERROR("Missing args"); return 1; }
        login_admin(argv[2], argv[3]);
    }
    else if (strcmp(action, "register") == 0) {
        /*
         * argv layout: auth register username password full_name email phone address
         * Index:         [0]   [1]     [2]      [3]     [4]       [5]   [6]   [7]
         * We now require 8 args (argc >= 8) because email is field [5].
         */
        if (argc < 8) { PRINT_ERROR("Missing args"); return 1; }
        register_user(argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]);
    }
    else if (strcmp(action, "get_profile") == 0) {
        if (argc < 3) { PRINT_ERROR("Missing args"); return 1; }
        get_profile(argv[2]);
    }
    else if (strcmp(action, "change_pass_user") == 0) {
        if (argc < 5) { PRINT_ERROR("Missing args"); return 1; }
        change_pass_user(argv[2], argv[3], argv[4]);
    }
    else if (strcmp(action, "change_pass_admin") == 0) {
        /*
         * argv layout: auth change_pass_admin admin_id old_pass new_pass
         * Index:         [0]   [1]             [2]      [3]      [4]
         */
        if (argc < 5) { PRINT_ERROR("Missing args"); return 1; }
        change_pass_admin(argv[2], argv[3], argv[4]);
    }
    else {
        PRINT_ERROR("Unknown action");
        return 1;
    }

    return 0;
}
