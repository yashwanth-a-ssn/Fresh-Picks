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
    sprintf(out_id, "U%03d", count + 1);
}

/* ─────────────────────────────────────────────────────────────
   FUNCTION: login_user
   PURPOSE:  Validate user credentials against users.txt
   PARAMS:   username, password - what the user typed
   OUTPUT:   SUCCESS|user_id  OR  ERROR|message
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

        char *t_id    = strtok(temp, DELIMITER); /* user_id  */
        char *t_uname = strtok(NULL, DELIMITER); /* username */
        char *t_pass  = strtok(NULL, DELIMITER); /* password */

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
   OUTPUT:   SUCCESS|admin  OR  ERROR|message
   ──────────────────────────────────────────────────────────── */
void login_admin(const char *username, const char *password) {
    FILE *fp = fopen(ADMIN_FILE, "r");
    if (fp == NULL) { PRINT_ERROR("Admin database not found"); return; }

    char line[MAX_LINE_LEN];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        PRINT_ERROR("Admin database is empty");
        return;
    }
    fclose(fp);
    trim_newline(line);

    char temp[MAX_LINE_LEN];
    strcpy(temp, line);

    char *stored_user = strtok(temp, DELIMITER); /* username field */
    char *stored_pass = strtok(NULL, DELIMITER); /* password field */

    if (!stored_user || !stored_pass) { PRINT_ERROR("Admin database format error"); return; }

    if (strcmp(stored_user, username) == 0 && strcmp(stored_pass, password) == 0) {
        PRINT_SUCCESS("admin");
    } else {
        PRINT_ERROR("Invalid admin credentials");
    }
}

/* ─────────────────────────────────────────────────────────────
   FUNCTION: register_user
   PURPOSE:  Add a new user to users.txt.
             First checks username is unique, then appends.
   PARAMS:   username, password, full_name, phone, address
   OUTPUT:   SUCCESS|new_user_id  OR  ERROR|message
   ──────────────────────────────────────────────────────────── */
void register_user(const char *username, const char *password,
                   const char *full_name, const char *phone,
                   const char *address) {

    /* Step 1: Check for duplicate username */
    FILE *fp = fopen(USERS_FILE, "r");
    char line[MAX_LINE_LEN];

    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp) != NULL) {
            trim_newline(line);
            if (strlen(line) < 3) continue;

            char temp[MAX_LINE_LEN];
            strcpy(temp, line);
            strtok(temp, DELIMITER);                     /* skip user_id */
            char *t_u = strtok(NULL, DELIMITER);         /* get username */

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

    /* Step 3: Append to users.txt */
    fp = fopen(USERS_FILE, "a"); /* Append mode */
    if (fp == NULL) { PRINT_ERROR("Could not write to database"); return; }

    fprintf(fp, "%s|%s|%s|%s|%s|%s\n",
            new_id, username, password, full_name, phone, address);
    fclose(fp);

    PRINT_SUCCESS(new_id); /* Return new user's ID */
}

/* ─────────────────────────────────────────────────────────────
   FUNCTION: get_profile
   PURPOSE:  Find user by ID and return their info (no password).
   PARAMS:   user_id - the ID to search for
   OUTPUT:   SUCCESS|id|username|full_name|phone|address
             OR  ERROR|message
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

        char *t_id   = strtok(temp, DELIMITER);
        char *t_u    = strtok(NULL, DELIMITER);
        char *t_pass = strtok(NULL, DELIMITER); /* Extracted but not returned */
        char *t_name = strtok(NULL, DELIMITER);
        char *t_ph   = strtok(NULL, DELIMITER);
        char *t_addr = strtok(NULL, DELIMITER);

        (void)t_pass; /* We have it but intentionally skip it for security */

        if (!t_id) continue;

        if (strcmp(t_id, user_id) == 0) {
            fclose(fp);
            printf("SUCCESS|%s|%s|%s|%s|%s\n",
                   t_id,
                   t_u    ? t_u    : "",
                   t_name ? t_name : "",
                   t_ph   ? t_ph   : "",
                   t_addr ? t_addr : "");
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

        char *t_id   = strtok(temp, DELIMITER);
        char *t_u    = strtok(NULL, DELIMITER);
        char *t_pass = strtok(NULL, DELIMITER);
        char *t_name = strtok(NULL, DELIMITER);
        char *t_ph   = strtok(NULL, DELIMITER);
        char *t_addr = strtok(NULL, DELIMITER);

        if (!t_id) continue;

        if (strcmp(t_id, user_id) == 0) {
            if (!t_pass || strcmp(t_pass, old_password) != 0) {
                PRINT_ERROR("Old password is incorrect");
                return;
            }
            found = 1;
            /* Rebuild this line with new password */
            sprintf(lines[i], "%s|%s|%s|%s|%s|%s",
                    t_id,
                    t_u    ? t_u    : "",
                    new_password,
                    t_name ? t_name : "",
                    t_ph   ? t_ph   : "",
                    t_addr ? t_addr : "");
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
   PARAMS:   old_password, new_password
   OUTPUT:   SUCCESS|Admin password changed  OR  ERROR|message
   ──────────────────────────────────────────────────────────── */
void change_pass_admin(const char *old_password, const char *new_password) {
    FILE *fp = fopen(ADMIN_FILE, "r");
    if (fp == NULL) { PRINT_ERROR("Admin database not found"); return; }

    char line[MAX_LINE_LEN];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        PRINT_ERROR("Admin database is empty");
        return;
    }
    fclose(fp);
    trim_newline(line);

    char temp[MAX_LINE_LEN];
    strcpy(temp, line);

    char *stored_user = strtok(temp, DELIMITER);
    char *stored_pass = strtok(NULL, DELIMITER);

    if (!stored_user || !stored_pass) { PRINT_ERROR("Admin database format error"); return; }

    if (strcmp(stored_pass, old_password) != 0) {
        PRINT_ERROR("Old password is incorrect");
        return;
    }

    /* Overwrite file with new password */
    fp = fopen(ADMIN_FILE, "w");
    if (fp == NULL) { PRINT_ERROR("Could not update admin database"); return; }

    fprintf(fp, "%s|%s\n", stored_user, new_password);
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

    if      (strcmp(action, "login_user") == 0)       { if (argc<4) {PRINT_ERROR("Missing args");return 1;} login_user(argv[2], argv[3]); }
    else if (strcmp(action, "login_admin") == 0)      { if (argc<4) {PRINT_ERROR("Missing args");return 1;} login_admin(argv[2], argv[3]); }
    else if (strcmp(action, "register") == 0)         { if (argc<7) {PRINT_ERROR("Missing args");return 1;} register_user(argv[2], argv[3], argv[4], argv[5], argv[6]); }
    else if (strcmp(action, "get_profile") == 0)      { if (argc<3) {PRINT_ERROR("Missing args");return 1;} get_profile(argv[2]); }
    else if (strcmp(action, "change_pass_user") == 0) { if (argc<5) {PRINT_ERROR("Missing args");return 1;} change_pass_user(argv[2], argv[3], argv[4]); }
    else if (strcmp(action, "change_pass_admin") == 0){ if (argc<4) {PRINT_ERROR("Missing args");return 1;} change_pass_admin(argv[2], argv[3]); }
    else { PRINT_ERROR("Unknown action"); return 1; }

    return 0;
}
