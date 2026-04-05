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
 *
 * CHANGELOG (v2):
 *   - AdminCreds: Added admin_id, admin_name, email fields
 *   - User:       Added email field (between full_name and phone)
 *   - DB schema updated to match new pipe-delimited format
 */

#ifndef MODELS_H   /* "Include guard" - prevents this file from being read twice */
#define MODELS_H

/* ─────────────────────────────────────────────
   CONSTANTS - fixed values used across the app
   ───────────────────────────────────────────── */

#define MAX_STR_LEN     100   /* Maximum length for most string fields */
#define MAX_LINE_LEN    512   /* Maximum length for one line in a text file */
#define MAX_ID_LEN      20    /* Maximum length for a user ID string */
#define MAX_ADD_LEN     256   /* Maximum length for address in DB text file */

/* File paths - adjust if your folder layout changes */
#define USERS_FILE      "users.txt"
#define ADMIN_FILE      "admin_creds.txt"

/* Delimiter used in all .txt database files */
#define DELIMITER       "|"


/* ─────────────────────────────────────────────
   STRUCT: AdminCreds
   Holds all fields for one admin account loaded
   from admin_creds.txt.

   DB Format (v2):
     admin_id|username|password|admin_name|email

   CHANGE from v1: Added admin_id, admin_name, email.
   Supports multiple admin accounts (loop in login_admin).
   ───────────────────────────────────────────── */
typedef struct {
    char admin_id[MAX_ID_LEN];   /* Unique admin ID, e.g. "A1001" */
    char username[MAX_STR_LEN];  /* Admin login username, e.g. "Admin" */
    char password[MAX_STR_LEN];  /* Admin password */
    char admin_name[MAX_STR_LEN];/* Display name, e.g. "CodeCrafters" */
    char email[MAX_STR_LEN];     /* Admin email, e.g. "codecrafters@gmail.com" */
} AdminCreds;


/* ─────────────────────────────────────────────
   STRUCT: User
   Represents one row in users.txt.

   DB Format (v2):
     user_id|username|password|full_name|email|phone|address

   CHANGE from v1: Added email field (position 5, between full_name and phone).
   Address format: "Door No, Street Name, Area, PIN Code"
   ───────────────────────────────────────────── */
typedef struct {
    char user_id[MAX_ID_LEN];       /* Unique ID, e.g. "U1001"  */
    char username[MAX_STR_LEN];     /* Login username            */
    char password[MAX_STR_LEN];     /* Plain-text password (demo only) */
    char full_name[MAX_STR_LEN];    /* Display name              */
    char email[MAX_STR_LEN];        /* Email for OTP/comms — NEW in v2 */
    char phone[MAX_STR_LEN];        /* Phone number              */
    char address[MAX_ADD_LEN];      /* Door No, Street, Area, PIN */
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
