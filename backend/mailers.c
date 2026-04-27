/*
 * mailer.c - Fresh Picks: Email Receipt Sender
 * =============================================
 * Standalone C binary.  Called by Flask via subprocess:
 *   ./mailer send_bill <user_id> <receipt_html>
 *
 * Uses libcurl (SMTP over TLS) to send the HTML receipt to the
 * email address stored in the user's record (loaded via utils.c).
 *
 * OUTPUT CONTRACT (identical to all other Fresh Picks binaries):
 *   SUCCESS|Bill sent successfully
 *   ERROR|<reason>
 *
 * COMPILE:
 *   gcc -Wall -Wextra -o mailer mailer.c utils.c -lm -lcurl
 *
 * PREREQUISITES:
 *   Ubuntu/Debian/WSL:  sudo apt install libcurl4-openssl-dev
 *   macOS:              brew install curl
 *
 * GMAIL SETUP:
 *   1. Enable 2-Step Verification on your Gmail account.
 *   2. Go to: Google Account → Security → App Passwords
 *   3. Generate a 16-character App Password for "Mail".
 *   4. Paste it into APP_PASSWORD below (spaces are fine).
 *   5. SENDER_EMAIL must match the Gmail account exactly.
 *
 * Team: CodeCrafters | Project: Fresh Picks | SDP-1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>
#include "models.h"   /* User struct, UserNode, load_user_sll, PRINT_ERROR, MAX_STR_LEN */


/* ══════════════════════════════════════════════════════════════════════
 * SECTION 1: CONFIGURATION MACROS
 * Replace these two values before building.
 * ══════════════════════════════════════════════════════════════════════ */
#define SENDER_EMAIL  "codecrafters658@gmail.com"   /* ← your Gmail address      */
#define APP_PASSWORD  "bvdw hcww ahsh vhev"    /* ← 16-char Gmail App Pass  */
#define SMTP_URL      "smtps://smtp.gmail.com:465"


/* ══════════════════════════════════════════════════════════════════════
 * SECTION 2: CURL UPLOAD CALLBACK
 * ══════════════════════════════════════════════════════════════════════ */

/*
 * STRUCT: UploadCtx
 * PURPOSE: Holds a pointer into our in-memory email message and
 *          tracks how many bytes remain to be sent.  libcurl calls
 *          payload_source_cb() repeatedly until size reaches 0.
 */
typedef struct {
    const char *data;   /* current read position in the message buffer */
    size_t      size;   /* bytes remaining                              */
} UploadCtx;

/*
 * FUNCTION: payload_source_cb
 * PURPOSE:  libcurl "readfunction" callback.
 *           Copies bytes from our UploadCtx into curl's send buffer.
 * PARAMS:   ptr   — destination buffer provided by libcurl
 *           size  — always 1 (element size in bytes)
 *           nmemb — max number of bytes libcurl wants this call
 *           ctx   — our UploadCtx cast to void*
 * RETURNS:  Number of bytes written into ptr (0 signals end-of-message).
 */
static size_t payload_source_cb(void *ptr, size_t size, size_t nmemb, void *ctx) {
    UploadCtx *upload = (UploadCtx *)ctx;

    if (upload->size == 0) return 0;  /* nothing left — signal EOF to curl */

    size_t copy_len = size * nmemb;
    if (copy_len > upload->size) copy_len = upload->size;

    memcpy(ptr, upload->data, copy_len);
    upload->data += copy_len;
    upload->size -= copy_len;

    return copy_len;
}

static int has_header_breaks(const char *value) {
    if (!value) return 1;
    return strchr(value, '\r') != NULL || strchr(value, '\n') != NULL;
}

static void copy_without_spaces(char *dest, size_t dest_size, const char *src) {
    size_t j = 0;

    if (!dest || dest_size == 0) return;
    dest[0] = '\0';
    if (!src) return;

    for (size_t i = 0; src[i] != '\0' && j + 1 < dest_size; i++) {
        if (!isspace((unsigned char)src[i])) {
            dest[j++] = src[i];
        }
    }
    dest[j] = '\0';
}

static void copy_header_text(char *dest, size_t dest_size, const char *src) {
    size_t j = 0;

    if (!dest || dest_size == 0) return;
    dest[0] = '\0';
    if (!src) return;

    for (size_t i = 0; src[i] != '\0' && j + 1 < dest_size; i++) {
        char ch = src[i];
        dest[j++] = (ch == '\r' || ch == '\n' || ch == '"') ? ' ' : ch;
    }
    dest[j] = '\0';
}


/* ══════════════════════════════════════════════════════════════════════
 * SECTION 3: send_bill
 * ══════════════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: send_bill
 * PURPOSE:  Look up the user's email via the SLL loaded by utils.c,
 *           build a MIME/HTML email message in memory, and deliver it
 *           through Gmail's SMTPS endpoint using libcurl.
 *
 * PARAMS:
 *   user_id      — e.g. "U1001"  (matched against users.dat)
 *   bill_details — complete HTML string produced by receipt.js
 *
 * OUTPUT (stdout — parsed by bridge.py):
 *   SUCCESS|Bill sent successfully
 *   ERROR|<reason>
 *
 * DATA FLOW:
 *   1. load_user_sll()  →  find user  →  copy email + name
 *   2. free_user_sll()
 *   3. snprintf() assembles the raw MIME message into a heap buffer
 *   4. curl_easy_perform() delivers it over SMTPS
 *   5. printf() reports result
 *   6. All resources freed
 */
void send_bill(const char *user_id, const char *bill_details) {

    /* ── Guard: reject empty arguments immediately ───────────────────── */
    if (!user_id || strlen(user_id) == 0) {
        PRINT_ERROR("send_bill: user_id is empty");
        return;
    }
    if (!bill_details || strlen(bill_details) == 0) {
        PRINT_ERROR("send_bill: bill_details (HTML) is empty");
        return;
    }

    /* ── Step 1: Resolve user_id → email via the SLL ─────────────────── */
    UserNode *head = load_user_sll();
    if (!head) {
        PRINT_ERROR("send_bill: no users found in database");
        return;
    }

    char recipient_email[MAX_STR_LEN] = "";
    char recipient_name [MAX_STR_LEN] = "";

    UserNode *curr = head;
    while (curr != NULL) {
        if (strcmp(curr->data.user_id, user_id) == 0) {
            strncpy(recipient_email, curr->data.email,     MAX_STR_LEN - 1);
            strncpy(recipient_name,  curr->data.full_name, MAX_STR_LEN - 1);
            recipient_email[MAX_STR_LEN - 1] = '\0';
            recipient_name [MAX_STR_LEN - 1] = '\0';
            break;
        }
        curr = curr->next;
    }
    free_user_sll(head);   /* always free regardless of match result */

    if (strlen(recipient_email) == 0) {
        PRINT_ERROR("send_bill: user not found");
        return;
    }
    if (!strchr(recipient_email, '@') || has_header_breaks(recipient_email)) {
        PRINT_ERROR("send_bill: invalid recipient email");
        return;
    }

    char smtp_password[128];
    char safe_recipient_name[MAX_STR_LEN];
    char recipient_mailbox[MAX_STR_LEN + 4];
    char sender_mailbox[MAX_STR_LEN + 4];

    copy_without_spaces(smtp_password, sizeof(smtp_password), APP_PASSWORD);
    copy_header_text(safe_recipient_name, sizeof(safe_recipient_name), recipient_name);

    if (strlen(smtp_password) == 0) {
        PRINT_ERROR("send_bill: sender app password is not configured");
        return;
    }
    if (has_header_breaks(SENDER_EMAIL) || !strchr(SENDER_EMAIL, '@')) {
        PRINT_ERROR("send_bill: sender email is not configured correctly");
        return;
    }

    snprintf(recipient_mailbox, sizeof(recipient_mailbox), "<%s>", recipient_email);
    snprintf(sender_mailbox, sizeof(sender_mailbox), "<%s>", SENDER_EMAIL);

    /* ── Step 2: Build the raw MIME message in a heap buffer ─────────── */
    /*
     * RFC 5322 email format:
     *   <headers>          — one per line, "Key: Value\r\n"
     *   <blank line>       — mandatory separator between headers and body
     *   <body>             — our HTML string
     *
     * Buffer size: fixed header overhead (~512 B) + HTML body length.
     */
    size_t html_len    = strlen(bill_details);
    size_t buf_size    = html_len + 1024;   /* 1 KB headroom for headers */
    char  *message     = (char *)malloc(buf_size);

    if (!message) {
        PRINT_ERROR("send_bill: malloc failed — out of memory");
        return;
    }

    snprintf(message, buf_size,
        /* ── RFC 5322 headers ── */
        "To: %s <%s>\r\n"
        "From: FreshPicks <" SENDER_EMAIL ">\r\n"
        "Subject: Your FreshPicks Order Receipt\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Transfer-Encoding: 8bit\r\n"
        "\r\n"          /* ← blank line: end of headers, start of body */
        "%s\r\n",       /* ← HTML receipt body                         */
        safe_recipient_name,
        recipient_email,
        bill_details
    );

    /* ── Step 3: Initialise libcurl ──────────────────────────────────── */
    CURLcode init_res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (init_res != CURLE_OK) {
        free(message);
        printf("ERROR|send_bill: curl_global_init failed: %s\n", curl_easy_strerror(init_res));
        return;
    }

    CURL *curl = curl_easy_init();

    if (!curl) {
        free(message);
        curl_global_cleanup();
        PRINT_ERROR("send_bill: curl_easy_init() failed");
        return;
    }

    /* Upload context — curl will pull bytes from here via the callback */
    UploadCtx upload_ctx = { message, strlen(message) };

    struct curl_slist *recipients = NULL;
    char curl_error[CURL_ERROR_SIZE] = "";

    /* ── Step 4: Configure the SMTP transfer ─────────────────────────── */

    /* Gmail SMTPS endpoint — implicit TLS, port 465 */
    curl_easy_setopt(curl, CURLOPT_URL, SMTP_URL);

    /* Authenticate with Gmail App Password */
    curl_easy_setopt(curl, CURLOPT_USERNAME, SENDER_EMAIL);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, smtp_password);

    /* Envelope FROM (SMTP protocol level — separate from the header above) */
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, sender_mailbox);

    /* Envelope RCPT TO */
    recipients = curl_slist_append(recipients, recipient_mailbox);
    if (!recipients) {
        curl_easy_cleanup(curl);
        free(message);
        curl_global_cleanup();
        PRINT_ERROR("send_bill: failed to allocate recipient list");
        return;
    }
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

    /* Wire up the read callback so curl pulls the message from memory */
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source_cb);
    curl_easy_setopt(curl, CURLOPT_READDATA,     &upload_ctx);
    curl_easy_setopt(curl, CURLOPT_UPLOAD,       1L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER,  curl_error);

    /* Enforce SSL certificate verification (keep at 1L in production) */
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    /* ── Step 5: Send ────────────────────────────────────────────────── */
    CURLcode res = curl_easy_perform(curl);

    /* ── Step 6: Report result to Flask via stdout ───────────────────── */
    if (res == CURLE_OK) {
        printf("SUCCESS|Bill sent successfully\n");
    } else {
        const char *detail = curl_error[0] ? curl_error : curl_easy_strerror(res);
        printf("ERROR|send_bill: %s\n", detail);
    }

    /* ── Step 7: Free everything ─────────────────────────────────────── */
    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);
    free(message);
    curl_global_cleanup();
}


/* ══════════════════════════════════════════════════════════════════════
 * SECTION 4: main — argument dispatch
 * ══════════════════════════════════════════════════════════════════════ */

/*
 * FUNCTION: main
 * PURPOSE:  Entry point. Parses argv and dispatches to send_bill().
 *           Follows the same argv pattern as auth.c / delivery.c.
 *
 * USAGE:
 *   ./mailer send_bill <user_id> <receipt_html>
 *
 * OUTPUT:
 *   SUCCESS|Bill sent successfully
 *   ERROR|<reason>
 */
int main(int argc, char *argv[]) {

    if (argc < 2) {
        PRINT_ERROR("Usage: ./mailer <action> [args...]");
        return 1;
    }

    const char *action = argv[1];

    if (strcmp(action, "send_bill") == 0) {
        /* Expects: ./mailer send_bill <user_id> <receipt_html> */
        if (argc < 4) {
            PRINT_ERROR("Usage: ./mailer send_bill <user_id> <receipt_html>");
            return 1;
        }
        send_bill(argv[2], argv[3]);

    } else {
        /* Unknown action — report cleanly so Flask can surface the error */
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Unknown action: %s", action);
        PRINT_ERROR(err_msg);
        return 1;
    }

    return 0;
}
