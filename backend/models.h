/*
 * models.h - Fresh Picks: Data Model Definitions
 * ================================================
 * This header file defines the data structures (structs) used throughout
 * the entire C backend. Think of these as "blueprints" for our data.
 *
 * HOW TO USE: Include this file in any .c file that needs these structs.
 *   Example: #include "models.h"
 *
 * Team: CodeCrafters | Project: Fresh Picks | SDP-1
 */

#ifndef MODELS_H   /* "Include guard" - prevents this file from being read twice */
#define MODELS_H

/* ─────────────────────────────────────────────
   CONSTANTS - fixed values used across the app
   ───────────────────────────────────────────── */

#define MAX_STR_LEN     100   /* Maximum length for most string fields */
#define MAX_LINE_LEN    512   /* Maximum length for one line in a text file */
#define MAX_ID_LEN      20    /* Maximum length for a user ID string */

/* File paths - adjust if your folder layout changes */
#define USERS_FILE      "users.txt"
#define ADMIN_FILE      "admin_creds.txt"

/* Delimiter used in all .txt database files */
#define DELIMITER       "|"


/* ─────────────────────────────────────────────
   STRUCT: AdminCreds
   Holds the admin username and password loaded
   from admin_creds.txt
   ───────────────────────────────────────────── */
typedef struct {
    char username[MAX_STR_LEN];  /* Admin username, e.g. "Admin" */
    char password[MAX_STR_LEN];  /* Admin password, e.g. "Admin" */
} AdminCreds;


/* ─────────────────────────────────────────────
   STRUCT: User
   Represents one row in users.txt
   Format: user_id|username|password|full_name|phone|address
   ───────────────────────────────────────────── */
typedef struct {
    char user_id[MAX_ID_LEN];       /* Unique ID, e.g. "U001" */
    char username[MAX_STR_LEN];     /* Login username */
    char password[MAX_STR_LEN];     /* Plain-text password (demo only) */
    char full_name[MAX_STR_LEN];    /* Display name */
    char phone[MAX_STR_LEN];        /* Phone number */
    char address[MAX_STR_LEN];      /* Home address */
} User;


/* ─────────────────────────────────────────────
   OUTPUT FORMAT MACROS
   Always print results in this format so Flask
   can easily split on '|' and read the result.
   ───────────────────────────────────────────── */

/* Print a success message with extra data */
#define PRINT_SUCCESS(data)  printf("SUCCESS|%s\n", data)

/* Print an error message with a reason */
#define PRINT_ERROR(reason)  printf("ERROR|%s\n", reason)


#endif /* End of include guard */
