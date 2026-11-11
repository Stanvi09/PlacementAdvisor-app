#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdint.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

/* TCC's windows.h does not always pull in basetsd.h, so UINT_PTR may be
   undefined. Fall back to the standard uintptr_t from <stdint.h>. */
#ifndef UINT_PTR
  #ifdef _WIN64
    typedef unsigned __int64 UINT_PTR;
  #else
    typedef uintptr_t UINT_PTR;
  #endif
#endif

typedef UINT_PTR SOCKET;
#define INVALID_SOCKET (SOCKET)(~0)
#define SOCKET_ERROR (-1)
#define AF_INET 2
#define SOCK_STREAM 1
//#define INADDR_ANY 0

/*struct in_addr {
    unsigned long s_addr;
};
struct sockaddr_in {
    short sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};
struct sockaddr {
    unsigned short sa_family;
    char sa_data[14];
};*/

/*typedef struct WSAData {
    WORD wVersion;
    WORD wHighVersion;
    char szDescription[257];
    char szSystemStatus[129];
    unsigned short iMaxSockets;
    unsigned short iMaxUdpDg;
    char* lpVendorInfo;
} WSADATA;*/

typedef int (WINAPI *WSAStartup_t)(WORD, WSADATA*);
typedef int (WINAPI *WSACleanup_t)(void);
typedef int (WINAPI *WSAGetLastError_t)(void);
typedef SOCKET (WINAPI *socket_t)(int, int, int);
typedef unsigned short (WINAPI *htons_t)(unsigned short);
typedef int (WINAPI *bind_t)(SOCKET, const struct sockaddr*, int);
typedef int (WINAPI *listen_t)(SOCKET, int);
typedef SOCKET (WINAPI *accept_t)(SOCKET, struct sockaddr*, int*);
typedef int (WINAPI *recv_t)(SOCKET, char*, int, int);
typedef int (WINAPI *send_t)(SOCKET, const char*, int, int);
typedef int (WINAPI *closesocket_t)(SOCKET);

WSAStartup_t my_WSAStartup;
WSACleanup_t my_WSACleanup;
WSAGetLastError_t my_WSAGetLastError;
socket_t my_socket;
htons_t my_htons;
bind_t my_bind;
listen_t my_listen;
accept_t my_accept;
recv_t my_recv;
send_t my_send;
closesocket_t my_closesocket;

void load_winsock() {
    HMODULE h = LoadLibraryA("ws2_32.dll");
    my_WSAStartup = (WSAStartup_t)GetProcAddress(h, "WSAStartup");
    my_WSACleanup = (WSACleanup_t)GetProcAddress(h, "WSACleanup");
    my_WSAGetLastError = (WSAGetLastError_t)GetProcAddress(h, "WSAGetLastError");
    my_socket = (socket_t)GetProcAddress(h, "socket");
    my_htons = (htons_t)GetProcAddress(h, "htons");
    my_bind = (bind_t)GetProcAddress(h, "bind");
    my_listen = (listen_t)GetProcAddress(h, "listen");
    my_accept = (accept_t)GetProcAddress(h, "accept");
    my_recv = (recv_t)GetProcAddress(h, "recv");
    my_send = (send_t)GetProcAddress(h, "send");
    my_closesocket = (closesocket_t)GetProcAddress(h, "closesocket");
}

#define PORT 8080
#define BUFFER_SIZE 8192

#define MAX_NAME        50
#define MAX_SKILLS      10
#define MAX_SKILL_LEN   30
#define MAX_COMPANIES    5
#define NUM_COMPANIES   12

/* ═══════════════════════════════════════════════════════════════
   STRUCTURE DEFINITIONS & DATABASES
   ═══════════════════════════════════════════════════════════════ */
typedef struct {
    char   name[MAX_NAME];
    char   branch[MAX_NAME];
    float  cgpa;
    int    backlogs;
    int    num_skills;
    char   skills[MAX_SKILLS][MAX_SKILL_LEN];
    int    projects;
    int    internships;
    int    leetcode_problems;
    char   preferred_domain[MAX_NAME];
} Student;

typedef struct {
    char  name[MAX_NAME];
    char  domain[MAX_NAME];
    float min_cgpa;
    int   allow_backlogs;
    int   base_salary;
    char  required_skill[MAX_SKILL_LEN];
    char  tier[10];
} Company;

typedef struct {
    char  company_name[MAX_NAME];
    int   match_score;
    int   estimated_salary;
    char  recommendation[20];
    char  tier[10];
} Prediction;

Company companies[NUM_COMPANIES] = {
    {"Google",         "Tech",       8.5f, 0, 45, "DSA",    "A"},
    {"Microsoft",      "Tech",       8.0f, 0, 40, "DSA",    "A"},
    {"Amazon",         "Tech",       7.5f, 0, 35, "DSA",    "A"},
    {"Infosys",        "Tech",       6.0f, 1,  8, "Java",   "C"},
    {"TCS",            "Tech",       6.0f, 1,  7, "C",      "C"},
    {"Wipro",          "Tech",       6.0f, 1,  7, "Python", "C"},
    {"Goldman Sachs",  "Finance",    8.0f, 0, 30, "Python", "A"},
    {"JP Morgan",      "Finance",    7.5f, 0, 25, "SQL",    "A"},
    {"Deloitte",       "Consulting", 7.0f, 1, 12, "Excel",  "B"},
    {"Accenture",      "Consulting", 6.5f, 1, 10, "Java",   "B"},
    {"ISRO",           "Core",       7.5f, 0,  9, "C",      "A"},
    {"L&T Technology", "Core",       7.0f, 1, 10, "MATLAB", "B"}
};

/* ═══════════════════════════════════════════════════════════════
   PREDICTION ENGINE LOGIC
   ═══════════════════════════════════════════════════════════════ */
int has_skill(const Student *s, const char *skill) {
    for (int i = 0; i < s->num_skills; i++) {
        char a[MAX_SKILL_LEN], b[MAX_SKILL_LEN];
        strncpy(a, s->skills[i], MAX_SKILL_LEN - 1); a[MAX_SKILL_LEN - 1] = '\0';
        strncpy(b, skill,        MAX_SKILL_LEN - 1); b[MAX_SKILL_LEN - 1] = '\0';
        for (char *p = a; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
        for (char *p = b; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
        if (strstr(a, b) != NULL) return 1;
    }
    return 0;
}

int compute_match_score(const Student *s, const Company *c) {
    int score = 0;
    if (s->cgpa >= c->min_cgpa) {
        float extra = s->cgpa - c->min_cgpa;
        score += 25 + (int)(extra * 3);
        if (score > 40) score = 40;
    } else {
        float diff = c->min_cgpa - s->cgpa;
        if (diff < 0.5f) score += 10;
    }
    if (s->backlogs == 0)                           score += 10;
    else if (c->allow_backlogs && s->backlogs <= 2) score += 5;
    if (has_skill(s, c->required_skill))            score += 15;
    score += (s->num_skills >= 4) ? 10 : (s->num_skills >= 2) ? 6 : 3;
    score += (s->projects    >= 3) ? 10 : (s->projects    >= 1) ? 6 : 0;
    score += (s->internships >= 2) ? 10 : (s->internships == 1) ? 6 : 0;
    if (strcmp(c->domain, "Tech") == 0 && strcmp(c->tier, "A") == 0) {
        score += (s->leetcode_problems >= 200) ? 5 :
                 (s->leetcode_problems >= 100) ? 3 : 1;
    }
    if (score > 100) score = 100;
    return score;
}

int estimate_salary(const Student *s, const Company *c, int score) {
    float base       = (float)c->base_salary;
    float multiplier = 1.0f + (score - 50) * 0.005f;
    if      (s->cgpa >= 9.0f) multiplier += 0.10f;
    else if (s->cgpa >= 8.5f) multiplier += 0.07f;
    else if (s->cgpa >= 8.0f) multiplier += 0.04f;
    multiplier += s->internships * 0.03f;
    float estimated = base * multiplier;
    float cap        = base * 1.30f;
    if (estimated > cap)          estimated = cap;
    if (estimated < base * 0.70f) estimated = base * 0.70f;
    return (int)roundf(estimated);
}

int run_predictions(const Student *s, Prediction results[], int max_results) {
    Prediction all[NUM_COMPANIES];
    for (int i = 0; i < NUM_COMPANIES; i++) {
        int sc = compute_match_score(s, &companies[i]);
        all[i].match_score      = sc;
        all[i].estimated_salary = estimate_salary(s, &companies[i], sc);
        strncpy(all[i].company_name, companies[i].name, MAX_NAME - 1);
        all[i].company_name[MAX_NAME - 1] = '\0';
        strncpy(all[i].tier, companies[i].tier, 9);
        all[i].tier[9] = '\0';
        if      (sc >= 70) strcpy(all[i].recommendation, "APPLY NOW");
        else if (sc >= 50) strcpy(all[i].recommendation, "MAYBE");
        else               strcpy(all[i].recommendation, "SKIP");
    }
    for (int i = 0; i < NUM_COMPANIES - 1; i++)
        for (int j = 0; j < NUM_COMPANIES - 1 - i; j++)
            if (all[j].match_score < all[j+1].match_score) {
                Prediction tmp = all[j]; all[j] = all[j+1]; all[j+1] = tmp;
            }
    int count = 0;
    for (int i = 0; i < NUM_COMPANIES && count < max_results; i++) {
        for (int k = 0; k < NUM_COMPANIES; k++) {
            if (strcmp(all[i].company_name, companies[k].name) == 0) {
                if (strcmp(companies[k].domain, s->preferred_domain) == 0)
                    results[count++] = all[i];
                break;
            }
        }
    }
    for (int i = 0; i < NUM_COMPANIES && count < max_results; i++) {
        int already = 0;
        for (int j = 0; j < count; j++)
            if (strcmp(results[j].company_name, all[i].company_name) == 0)
                { already = 1; break; }
        if (!already) results[count++] = all[i];
    }
    return count;
}

/* ═══════════════════════════════════════════════════════════════
   HTTP SERVER CAP-SAFE UTILITIES
   ═══════════════════════════════════════════════════════════════ */

void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit((unsigned char)a) && isxdigit((unsigned char)b))) {
            
            if (a >= 'a' && a <= 'f') a = a - 'a' + 10;
            else if (a >= 'A' && a <= 'F') a = a - 'A' + 10;
            else if (a >= '0' && a <= '9') a = a - '0';

            if (b >= 'a' && b <= 'f') b = b - 'a' + 10;
            else if (b >= 'A' && b <= 'F') b = b - 'A' + 10;
            else if (b >= '0' && b <= '9') b = b - '0';

            *dst++ = (char)(16 * a + b);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

void extract_value(const char* body, const char* key, char* output, int max_len) {
    output[0] = '\0';
    char search_key[100];
    snprintf(search_key, sizeof(search_key), "%s=", key);
    
    const char* start = strstr(body, search_key);
    if (start) {
        start += strlen(search_key);
        const char* end = strchr(start, '&');
        int len = end ? (end - start) : strlen(start);
        
        if (len >= 500) len = 499;
        
        char temp[500];
        strncpy(temp, start, len);
        temp[len] = '\0';
        
        char decoded[500];
        url_decode(decoded, temp);
        
        strncpy(output, decoded, max_len - 1);
        output[max_len - 1] = '\0';
    }
}

/* Escapes <, >, & and " so text typed into a resume field (skills,
   achievements, etc.) can never break the resume's HTML structure. */
static void html_escape(const char* src, char* dst, size_t max_len) {
    size_t i = 0;
    while (*src && i < max_len - 1) {
        if (*src == '<' && i + 4 < max_len)      { memcpy(dst + i, "&lt;", 4);   i += 4; src++; }
        else if (*src == '>' && i + 4 < max_len) { memcpy(dst + i, "&gt;", 4);   i += 4; src++; }
        else if (*src == '&' && i + 5 < max_len) { memcpy(dst + i, "&amp;", 5);  i += 5; src++; }
        else if (*src == '"' && i + 6 < max_len) { memcpy(dst + i, "&quot;", 6); i += 6; src++; }
        else { dst[i++] = *src++; }
    }
    dst[i] = '\0';
}

/* Splits a delimiter-separated string (e.g. "C, Python, SQL") into trimmed,
   HTML-escaped <li>...</li> fragments, ignoring empty entries. Used to turn
   raw comma/semicolon-separated form fields into proper bullet lists for
   the resume output instead of dumping raw text. Returns the number of
   items rendered. */
int render_list_items(const char* raw, char delim, char* out, size_t out_size) {
    out[0] = '\0';
    if (!raw || !raw[0]) return 0;

    size_t used = 0;
    int count = 0;
    const char* p = raw;

    while (*p) {
        char item[220];
        int len = 0;
        while (*p && *p != delim && len < (int)sizeof(item) - 1) {
            item[len++] = *p++;
        }
        item[len] = '\0';
        if (*p == delim) p++;

        char* start = item;
        while (*start == ' ' || *start == '\t') start++;
        char* end = start + strlen(start) - 1;
        while (end > start && (*end == ' ' || *end == '\t')) *end-- = '\0';

        if (*start) {
            char escaped[300];
            html_escape(start, escaped, sizeof(escaped));

            char li[350];
            snprintf(li, sizeof(li), "<li>%s</li>", escaped);
            size_t l = strlen(li);
            if (used + l < out_size - 1) {
                strcat(out, li);
                used += l;
                count++;
            }
        }
    }
    return count;
}

/* ═══════════════════════════════════════════════════════════════
   PREMIUM STYLE SHEET & MODERN LAYOUT ENGINE
   ═══════════════════════════════════════════════════════════════ */

const char* page_styles = 
"* { box-sizing: border-box; margin: 0; padding: 0; }\n"
":root {\n"
"  --bg: #12131a;\n"
"  --surface-1: #191b23;\n"
"  --surface-2: rgba(255,255,255,0.05);\n"
"  --surface-3: rgba(255,255,255,0.08);\n"
"  --surface-4: rgba(255,255,255,0.12);\n"
"  --border: rgba(255,255,255,0.09);\n"
"  --border-strong: rgba(255,255,255,0.16);\n"
"  --text: #eef0f3;\n"
"  --text-muted: #98a0ab;\n"
"  --text-faint: #676f7a;\n"
"  --accent: #cc9a3d;\n"
"  --accent-strong: #e6b455;\n"
"  --accent-soft: rgba(204,154,61,0.15);\n"
"  --accent-ink: #241d0d;\n"
"  --accent-2: #4fa39d;\n"
"  --accent-2-soft: rgba(79,163,157,0.15);\n"
"  --success: #34d399;\n"
"  --success-soft: rgba(52,211,153,0.14);\n"
"  --warning: #fbbf24;\n"
"  --warning-soft: rgba(251,191,36,0.14);\n"
"  --danger: #f7797f;\n"
"  --danger-soft: rgba(247,121,127,0.14);\n"
"  --shadow: 0 24px 48px rgba(0,0,0,0.45);\n"
"  --shadow-sm: 0 10px 26px rgba(0,0,0,0.3);\n"
"  --input-bg: rgba(0,0,0,0.28);\n"
"  --code-bg: rgba(0,0,0,0.3);\n"
"}\n"
":root[data-theme='light'] {\n"
"  --bg: #f3f1eb;\n"
"  --surface-1: #ffffff;\n"
"  --surface-2: rgba(30,25,15,0.04);\n"
"  --surface-3: rgba(30,25,15,0.06);\n"
"  --surface-4: rgba(30,25,15,0.1);\n"
"  --border: rgba(30,25,15,0.1);\n"
"  --border-strong: rgba(30,25,15,0.18);\n"
"  --text: #201d17;\n"
"  --text-muted: #6b6355;\n"
"  --text-faint: #92897a;\n"
"  --accent: #a2711f;\n"
"  --accent-strong: #8a5f19;\n"
"  --accent-soft: rgba(162,113,31,0.12);\n"
"  --accent-ink: #fffaf0;\n"
"  --accent-2: #1c716c;\n"
"  --accent-2-soft: rgba(28,113,108,0.12);\n"
"  --success: #16874c;\n"
"  --success-soft: rgba(22,135,76,0.12);\n"
"  --warning: #a45b09;\n"
"  --warning-soft: rgba(164,91,9,0.12);\n"
"  --danger: #c8353d;\n"
"  --danger-soft: rgba(200,53,61,0.12);\n"
"  --shadow: 0 24px 48px rgba(32,24,10,0.14);\n"
"  --shadow-sm: 0 10px 24px rgba(32,24,10,0.08);\n"
"  --input-bg: #ffffff;\n"
"  --code-bg: rgba(30,25,15,0.045);\n"
"}\n"
"html { color-scheme: dark; }\n"
":root[data-theme='light'] html, html[data-theme='light'] { color-scheme: light; }\n"
"body { font-family: 'Outfit', sans-serif; background: var(--bg); color: var(--text); min-height: 100vh; display: flex; flex-direction: column; align-items: center; padding-bottom: 60px; transition: background 0.25s ease, color 0.25s ease; }\n"
"h1, h2, .nav-brand, .compare-title, .backlog-card h3, .gap-card h4 { font-family: 'Space Grotesk', 'Outfit', sans-serif; }\n"
".navbar { width: 100%; max-width: 1140px; display: flex; justify-content: space-between; align-items: center; padding: 16px 26px; margin-top: 20px; background: var(--surface-1); border: 1px solid var(--border); border-radius: 14px; box-shadow: var(--shadow-sm); gap: 16px; }\n"
".nav-brand { display: flex; align-items: center; gap: 9px; font-size: 1.2em; font-weight: 700; color: var(--text); text-decoration: none; letter-spacing: -0.01em; transition: opacity 0.2s; white-space: nowrap; flex-shrink: 0; }\n"
".nav-brand:hover { opacity: 0.8; }\n"
".nav-brand .mark { display: inline-flex; width: 26px; height: 26px; border-radius: 7px; background: var(--accent); color: var(--accent-ink); align-items: center; justify-content: center; font-size: 0.62em; font-weight: 800; letter-spacing: 0; flex-shrink: 0; }\n"
".nav-right { display: flex; align-items: center; min-width: 0; flex: 1; justify-content: flex-end; }\n"
".nav-links { display: flex; gap: 2px; flex-wrap: nowrap; overflow-x: auto; scrollbar-width: none; max-width: 100%; }\n"
".nav-links::-webkit-scrollbar { display: none; }\n"
".nav-links a { color: var(--text-muted); text-decoration: none; font-weight: 500; font-size: 0.82em; padding: 7px 10px; border-radius: 8px; transition: all 0.2s; white-space: nowrap; flex-shrink: 0; }\n"
".nav-links a:hover { color: var(--text); background: var(--surface-2); }\n"
".nav-links a.active { color: var(--accent); background: var(--accent-soft); }\n"
".theme-toggle { position: fixed; top: 20px; right: 24px; z-index: 100; display: flex; align-items: center; justify-content: center; width: 40px; height: 40px; border-radius: 10px; border: 1px solid var(--border); background: var(--surface-1); color: var(--text-muted); cursor: pointer; transition: all 0.2s; box-shadow: var(--shadow-sm); }\n"
".theme-toggle:hover { background: var(--surface-3); color: var(--text); border-color: var(--border-strong); }\n"
".theme-toggle svg { width: 17px; height: 17px; }\n"
".theme-toggle .icon-sun { display: none; }\n"
".theme-toggle .icon-moon { display: block; }\n"
":root[data-theme='light'] .theme-toggle .icon-sun { display: block; }\n"
":root[data-theme='light'] .theme-toggle .icon-moon { display: none; }\n"
".container { background: var(--surface-1); border: 1px solid var(--border); border-radius: 16px; padding: 40px; box-shadow: var(--shadow); max-width: 940px; width: 95%; margin: 24px auto; }\n"
"h1 { text-align: left; font-weight: 700; color: var(--text); margin-bottom: 8px; font-size: 1.9em; letter-spacing: -0.015em; }\n"
".subtitle { text-align: left; color: var(--text-muted); margin-bottom: 30px; font-size: 1em; max-width: 65ch; }\n"
"h2 { color: var(--text); border-bottom: 1px solid var(--border); padding-bottom: 12px; margin-top: 34px; margin-bottom: 20px; font-size: 1.28em; font-weight: 600; display: flex; align-items: center; gap: 9px; }\n"
"h2::before { content: ''; width: 4px; height: 16px; background: var(--accent); border-radius: 2px; display: inline-block; }\n"
".grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }\n"
"@media (max-width: 768px) { .grid { grid-template-columns: 1fr; } .navbar { flex-direction: column; gap: 12px; padding: 16px; } .nav-links { justify-content: flex-start; width: 100%; } .theme-toggle { top: 14px; right: 14px; width: 36px; height: 36px; } }\n"
".form-group { margin-bottom: 18px; display: flex; flex-direction: column; }\n"
"label { margin-bottom: 7px; font-weight: 500; font-size: 0.82em; letter-spacing: 0.4px; color: var(--text-muted); text-transform: uppercase; }\n"
"input, select { background: var(--input-bg); border: 1px solid var(--border-strong); color: var(--text); padding: 11px 14px; border-radius: 9px; font-family: 'Outfit', sans-serif; font-size: 1em; outline: none; transition: all 0.2s; }\n"
"input:focus, select:focus { border-color: var(--accent); box-shadow: 0 0 0 3px var(--accent-soft); }\n"
"input[type='checkbox'] { width: 19px; height: 19px; accent-color: var(--accent); cursor: pointer; }\n"
".checkbox-group { display: flex; align-items: center; gap: 12px; margin-bottom: 15px; }\n"
".checkbox-group label { margin-bottom: 0; text-transform: none; font-size: 0.95em; color: var(--text); cursor: pointer; }\n"
".btn { background: var(--accent); border: none; color: var(--accent-ink); font-weight: 600; padding: 13px 22px; border-radius: 9px; cursor: pointer; transition: all 0.2s ease; width: 100%; margin-top: 14px; font-size: 1em; letter-spacing: 0.2px; display: inline-block; text-align: center; text-decoration: none; }\n"
".btn:hover { background: var(--accent-strong); transform: translateY(-1px); box-shadow: var(--shadow-sm); }\n"
".btn-secondary { background: var(--surface-2); border: 1px solid var(--border-strong); color: var(--text-muted); }\n"
".btn-secondary:hover { background: var(--surface-3); color: var(--text); box-shadow: none; }\n"
".results-table { width: 100%; border-collapse: collapse; margin-top: 20px; background: var(--surface-2); border-radius: 10px; overflow: hidden; border: 1px solid var(--border); }\n"
".results-table th, .results-table td { padding: 14px 16px; text-align: left; border-bottom: 1px solid var(--border); }\n"
".results-table th { background: var(--surface-3); font-weight: 600; color: var(--accent); font-size: 0.85em; text-transform: uppercase; letter-spacing: 0.5px; font-family: 'JetBrains Mono', monospace; }\n"
".results-table tr:hover { background: var(--surface-2); }\n"
".badge { padding: 5px 11px; border-radius: 7px; font-size: 0.76em; font-weight: 700; display: inline-block; letter-spacing: 0.3px; font-family: 'JetBrains Mono', monospace; }\n"
".badge-apply { background: var(--success-soft); color: var(--success); border: 1px solid var(--success-soft); }\n"
".badge-maybe { background: var(--warning-soft); color: var(--warning); border: 1px solid var(--warning-soft); }\n"
".badge-skip { background: var(--danger-soft); color: var(--danger); border: 1px solid var(--danger-soft); }\n"
".badge-tier { background: var(--accent-2-soft); color: var(--accent-2); border: 1px solid var(--accent-2-soft); }\n"
".progress-bar { width: 100%; background: var(--surface-2); border-radius: 9px; overflow: hidden; margin: 15px 0; border: 1px solid var(--border); padding: 3px; }\n"
".progress-fill { height: 24px; border-radius: 6px; transition: width 1s cubic-bezier(0.4, 0, 0.2, 1); display: flex; align-items: center; justify-content: flex-end; padding-right: 12px; font-size: 0.82em; font-weight: bold; color: #fff; text-shadow: 0 1px 3px rgba(0,0,0,0.5); font-family: 'JetBrains Mono', monospace; }\n"
".verdict-box { margin-top: 15px; font-size: 1.02em; padding: 12px 16px; border-radius: 8px; background: var(--surface-2); border-left: 3px solid var(--text-faint); }\n"
".verdict-excellent { border-left-color: var(--success); color: var(--success); }\n"
".verdict-good { border-left-color: var(--warning); color: var(--warning); }\n"
".verdict-average { border-left-color: var(--warning); color: var(--warning); }\n"
".verdict-needs-work { border-left-color: var(--danger); color: var(--danger); }\n"
".bio-box { background: var(--surface-2); padding: 22px; border-left: 3px solid var(--accent); border-radius: 10px; margin-top: 15px; line-height: 1.7; font-size: 1.02em; color: var(--text); }\n"
".compare-card { background: var(--surface-2); border: 1px solid var(--border); border-radius: 12px; padding: 20px; text-align: center; }\n"
".compare-title { font-size: 1.2em; font-weight: 700; color: var(--accent); margin-bottom: 10px; }\n"
".score-circle-container { display: flex; justify-content: center; align-items: center; margin: 30px 0; }\n"
".score-circle { width: 136px; height: 136px; border-radius: 50%; border: 5px solid var(--surface-3); display: flex; flex-direction: column; justify-content: center; align-items: center; background: var(--surface-2); }\n"
".score-num { font-size: 2.3em; font-weight: 700; color: var(--accent); font-family: 'JetBrains Mono', monospace; }\n"
".score-label { font-size: 0.78em; color: var(--text-muted); text-transform: uppercase; letter-spacing: 1px; }\n"
".checklist { list-style: none; margin-top: 20px; }\n"
".checklist-item { display: flex; align-items: flex-start; gap: 12px; padding: 12px; border-radius: 8px; margin-bottom: 8px; background: var(--surface-2); }\n"
".checklist-item.success { border-left: 3px solid var(--success); }\n"
".checklist-item.warning { border-left: 3px solid var(--warning); }\n"
".icon-check { color: var(--success); font-weight: bold; }\n"
".icon-warn { color: var(--warning); font-weight: bold; }\n"
".gap-card { background: var(--surface-2); border: 1px solid var(--border); border-radius: 12px; padding: 18px 20px; margin-bottom: 14px; }\n"
".gap-card h4 { color: var(--accent); margin-bottom: 8px; font-size: 1em; }\n"
".gap-card ul { list-style: none; padding: 0; }\n"
".gap-card ul li { padding: 5px 0; color: var(--text-muted); font-size: 0.93em; }\n"
".gap-card ul li::before { content: '\\2192  '; color: var(--danger); }\n"
".gap-card ul li.has-skill::before { content: '\\2713  '; color: var(--success); }\n"
".interview-q { background: var(--surface-2); border-left: 3px solid var(--accent-2); border-radius: 0 10px 10px 0; padding: 14px 18px; margin-bottom: 12px; }\n"
".interview-q .q-num { font-size: 0.76em; color: var(--accent-2); font-weight: 700; letter-spacing: 1px; margin-bottom: 4px; font-family: 'JetBrains Mono', monospace; }\n"
".interview-q .q-text { color: var(--text); font-size: 1em; }\n"
".interview-q .q-hint { color: var(--text-faint); font-size: 0.85em; margin-top: 6px; font-style: italic; }\n"
".sim-result-row { display: flex; align-items: center; gap: 14px; margin-bottom: 10px; padding: 10px 14px; border-radius: 8px; background: var(--surface-2); }\n"
".sim-company-name { width: 150px; font-weight: 600; color: var(--text); }\n"
".sim-bar-wrap { flex: 1; background: var(--surface-2); border-radius: 6px; overflow: hidden; height: 20px; border: 1px solid var(--border); }\n"
".sim-bar-fill { height: 20px; border-radius: 5px; display: flex; align-items: center; justify-content: flex-end; padding-right: 8px; font-size: 0.76em; font-weight: bold; color: #fff; font-family: 'JetBrains Mono', monospace; }\n"
".sim-status { width: 90px; text-align: right; font-size: 0.8em; font-weight: 600; }\n"
".backlog-card { border: 1px solid var(--border); border-radius: 12px; padding: 20px; background: var(--surface-2); margin-bottom: 16px; }\n"
".backlog-card h3 { color: var(--accent); margin-bottom: 6px; }\n"
".backlog-card .bc-meta { font-size: 0.84em; color: var(--text-muted); margin-bottom: 10px; font-family: 'JetBrains Mono', monospace; }\n"
".backlog-card .bc-tip { background: var(--accent-2-soft); border-left: 3px solid var(--accent-2); border-radius: 0 8px 8px 0; padding: 10px 14px; font-size: 0.9em; color: var(--accent-2); margin-top: 10px; }\n"
".resume-gen-box { background: #ffffff; border-radius: 12px; padding: 44px 48px; font-family: 'Georgia', 'Times New Roman', serif; font-size: 0.95em; line-height: 1.6; color: #1f2937; box-shadow: 0 20px 50px rgba(0,0,0,0.35); }\n"
".resume-gen-box .rg-name { font-size: 1.9em; font-weight: 700; color: #111827; text-align: center; display: block; margin-bottom: 4px; font-family: 'Outfit', sans-serif; letter-spacing: 0.5px; }\n"
".resume-gen-box .rg-contact { text-align: center; color: #4b5563; display: block; margin-bottom: 20px; font-family: 'Outfit', sans-serif; font-size: 0.85em; }\n"
".resume-gen-box .rg-section { color: #111827; font-weight: 700; border-bottom: 2px solid #111827; padding-bottom: 3px; margin: 18px 0 8px; display: block; font-family: 'Outfit', sans-serif; text-transform: uppercase; letter-spacing: 1.2px; font-size: 0.8em; }\n"
".resume-gen-box .rg-objective { color: #374151; margin: 0 0 4px 0; }\n"
".resume-gen-box .rg-edu-row { display: flex; justify-content: space-between; flex-wrap: wrap; gap: 4px 16px; color: #1f2937; margin-bottom: 2px; }\n"
".resume-gen-box .rg-edu-row strong { color: #111827; }\n"
".resume-gen-box ul.rg-list { list-style: none; margin: 0 0 4px 0; padding: 0; }\n"
".resume-gen-box ul.rg-list li { position: relative; padding-left: 16px; margin-bottom: 4px; color: #1f2937; }\n"
".resume-gen-box ul.rg-list li::before { content: '\\2022'; position: absolute; left: 0; color: #6366f1; font-weight: bold; }\n"
".resume-gen-box .rg-empty { color: #9ca3af; font-style: italic; }\n"
"@media print { .navbar, .btn, form, .subtitle, .tip-box-print-hide, .theme-toggle { display: none !important; } body { background: #fff !important; padding: 0 !important; } .container { box-shadow: none !important; border: none !important; padding: 0 !important; max-width: 100% !important; } .resume-gen-box { box-shadow: none !important; padding: 0 !important; } }\n"
".review-card { background: var(--surface-2); border: 1px solid var(--border); border-radius: 12px; padding: 22px; margin-bottom: 16px; }\n"
".review-card .rc-header { display: flex; align-items: center; gap: 14px; margin-bottom: 12px; }\n"
".review-card .rc-avatar { width: 42px; height: 42px; border-radius: 50%; background: var(--accent); display: flex; align-items: center; justify-content: center; font-weight: 700; font-size: 1.05em; color: var(--accent-ink); flex-shrink: 0; }\n"
".review-card .rc-meta h4 { color: var(--text); font-size: 0.97em; margin-bottom: 2px; }\n"
".review-card .rc-meta span { color: var(--text-faint); font-size: 0.82em; }\n"
".review-card .rc-stars { color: var(--warning); letter-spacing: 2px; font-size: 0.9em; }\n"
".review-card .rc-body { color: var(--text-muted); font-size: 0.93em; line-height: 1.65; }\n"
".review-card .rc-tag { display: inline-block; background: var(--accent-2-soft); border: 1px solid var(--accent-2-soft); color: var(--accent-2); border-radius: 20px; padding: 3px 10px; font-size: 0.75em; margin-top: 10px; margin-right: 6px; }\n"
".review-form textarea { background: var(--input-bg); border: 1px solid var(--border-strong); color: var(--text); padding: 12px 16px; border-radius: 9px; font-family: 'Outfit', sans-serif; font-size: 0.95em; width: 100%; min-height: 100px; outline: none; resize: vertical; transition: all 0.2s; }\n"
".review-form textarea:focus { border-color: var(--accent); box-shadow: 0 0 0 3px var(--accent-soft); }\n";

void send_response(SOCKET client_socket, const char* status, const char* content_type, const char* body) {
    char header[1024];
    snprintf(header, sizeof(header), 
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n", 
             status, content_type, strlen(body));
    my_send(client_socket, header, strlen(header), 0);
    my_send(client_socket, body, strlen(body), 0);
}

void serve_page(SOCKET client, const char* active_tab, const char* body_content) {
    char* full_html = malloc(65536);
    if (!full_html) {
        send_response(client, "500 Internal Server Error", "text/plain", "Out of memory.");
        return;
    }
    
    snprintf(full_html, 65536,
        "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<meta name='description' content='AI Placement Advisor - Predict your campus placement eligibility, score your resume, compare recruiters, and get a personalized study roadmap.'>"
        "<link rel='icon' type='image/svg+xml' href=\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><text y='.9em' font-size='90'>🎯</text></svg>\">"
        "<title>AI Placement Advisor - Smart Career Assistant</title>"
        "<link rel='preconnect' href='https://fonts.googleapis.com'>"
        "<link href='https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;500;600;700&family=Space+Grotesk:wght@500;600;700&family=JetBrains+Mono:wght@500;600&display=swap' rel='stylesheet'>"
        "<script>(function(){try{var t=localStorage.getItem('pd-theme');if(!t){t=window.matchMedia('(prefers-color-scheme: light)').matches?'light':'dark';}document.documentElement.setAttribute('data-theme',t);}catch(e){}})();function toggleTheme(){var h=document.documentElement,c=h.getAttribute('data-theme')==='light'?'dark':'light';h.setAttribute('data-theme',c);try{localStorage.setItem('pd-theme',c);}catch(e){}}</script>"
        "<style>%s</style></head><body>"
        "<button type='button' class='theme-toggle' onclick='toggleTheme()' aria-label='Toggle color theme' title='Toggle color theme'>"
        "<svg class='icon-sun' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round'><circle cx='12' cy='12' r='4'></circle><path d='M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4'></path></svg>"
        "<svg class='icon-moon' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><path d='M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z'></path></svg>"
        "</button>"
        "<div class='navbar'>"
        "<a href='/' class='nav-brand'>🎯 AI Placement Advisor</a>"
        "<div class='nav-right'>"
        "<div class='nav-links'>"
        "<a href='/' class='%s'>Predictor</a>"
        "<a href='/resume' class='%s'>Resume Scorer</a>"
        "<a href='/compare' class='%s'>Compare</a>"
        "<a href='/roadmap' class='%s'>Roadmap</a>"
        "<a href='/interview' class='%s'>Interview</a>"
        "<a href='/simulator' class='%s'>CGPA Sim</a>"
        "<a href='/backlogs' class='%s'>Backlogs</a>"
        "<a href='/resume-gen' class='%s'>Resume Builder</a>"
        "<a href='/reviews' class='%s'>Reviews</a>"
        "</div>"
        "</div>"
        "</div>"
        "<div class='container'>%s</div>"
        "</body></html>",
        page_styles,
        strcmp(active_tab, "predictor") == 0 ? "active" : "",
        strcmp(active_tab, "resume") == 0 ? "active" : "",
        strcmp(active_tab, "compare") == 0 ? "active" : "",
        strcmp(active_tab, "roadmap") == 0 ? "active" : "",
        strcmp(active_tab, "interview") == 0 ? "active" : "",
        strcmp(active_tab, "simulator") == 0 ? "active" : "",
        strcmp(active_tab, "backlogs") == 0 ? "active" : "",
        strcmp(active_tab, "resumegen") == 0 ? "active" : "",
        strcmp(active_tab, "reviews") == 0 ? "active" : "",
        body_content
    );
    
    send_response(client, "200 OK", "text/html", full_html);
    free(full_html);
}

/* ═══════════════════════════════════════════════════════════════
   ROUTE HANDLERS
   ═══════════════════════════════════════════════════════════════ */

void handle_get_home(SOCKET client) {
    char body[5120];
    snprintf(body, sizeof(body),
        "<h1>Placement Eligibility Predictor</h1>"
        "<p class='subtitle'>Analyze your academic profile and skills to see matching campus placement opportunities</p>"
        "<form method='POST' action='/predict'>"
        "<h2>Academic &amp; Coding Profile</h2>"
        "<div class='grid'>"
        "<div class='form-group'><label for='name'>Full Name</label><input type='text' id='name' name='name' placeholder='e.g. John Doe' autocomplete='name' required></div>"
        "<div class='form-group'><label for='branch'>Branch</label><input type='text' id='branch' name='branch' placeholder='e.g. Computer Science' required></div>"
        "<div class='form-group'><label for='cgpa'>CGPA (0.0 - 10.0)</label><input type='number' id='cgpa' step='0.01' min='0.0' max='10.0' name='cgpa' placeholder='e.g. 8.45' required></div>"
        "<div class='form-group'><label for='backlogs'>Active Backlogs</label><input type='number' id='backlogs' min='0' name='backlogs' value='0' required></div>"
        "<div class='form-group'><label for='projects'>Hands-on Projects</label><input type='number' id='projects' min='0' name='projects' value='0' required></div>"
        "<div class='form-group'><label for='internships'>Completed Internships</label><input type='number' id='internships' min='0' name='internships' value='0' required></div>"
        "<div class='form-group'><label for='leetcode'>LeetCode Solved</label><input type='number' id='leetcode' min='0' name='leetcode' value='0' required></div>"
        "<div class='form-group'><label for='domain'>Preferred Domain</label>"
        "<select id='domain' name='domain'>"
        "<option value='Tech'>Tech (Software, Data, Cloud)</option>"
        "<option value='Finance'>Finance (Quant, Investment)</option>"
        "<option value='Consulting'>Consulting (Strategy, Advisory)</option>"
        "<option value='Core'>Core (Hardware, Civil, Mech, ISRO)</option>"
        "</select>"
        "</div>"
        "</div>"
        "<h2>Skills Catalog</h2>"
        "<div class='form-group'><label>Comma Separated Skills</label><input type='text' name='skills' placeholder='e.g. C, Python, SQL, DSA' required></div>"
        "<button type='submit' class='btn'>Generate Placement Predictions</button>"
        "</form>"
    );
    serve_page(client, "predictor", body);
}

void handle_post_predict(SOCKET client, const char* body) {
    Student s;
    memset(&s, 0, sizeof(Student));
    char temp[200];
    
    extract_value(body, "name", s.name, sizeof(s.name));
    extract_value(body, "branch", s.branch, sizeof(s.branch));
    
    extract_value(body, "cgpa", temp, sizeof(temp));
    s.cgpa = (float)atof(temp);
    
    extract_value(body, "backlogs", temp, sizeof(temp));
    s.backlogs = atoi(temp);
    
    extract_value(body, "projects", temp, sizeof(temp));
    s.projects = atoi(temp);
    
    extract_value(body, "internships", temp, sizeof(temp));
    s.internships = atoi(temp);
    
    extract_value(body, "leetcode", temp, sizeof(temp));
    s.leetcode_problems = atoi(temp);
    
    extract_value(body, "domain", s.preferred_domain, sizeof(s.preferred_domain));
    
    char raw_skills[500];
    extract_value(body, "skills", raw_skills, sizeof(raw_skills));
    
    char *token = strtok(raw_skills, ",");
    s.num_skills = 0;
    while (token != NULL && s.num_skills < MAX_SKILLS) {
        while (isspace((unsigned char)*token)) token++;
        int len = strlen(token);
        while (len > 0 && isspace((unsigned char)token[len - 1])) {
            token[len - 1] = '\0';
            len--;
        }
        if (strlen(token) > 0) {
            strncpy(s.skills[s.num_skills], token, MAX_SKILL_LEN - 1);
            s.skills[s.num_skills][MAX_SKILL_LEN - 1] = '\0';
            s.num_skills++;
        }
        token = strtok(NULL, ",");
    }

    float score = 0;
    score += (s.cgpa / 10.0f) * 35;
    score += (s.backlogs == 0 ? 1.0f : s.backlogs <= 2 ? 0.4f : 0.1f) * 15;
    score += (s.projects >= 2 ? 1.0f : s.projects * 0.5f) * 20;
    score += (s.internships >= 1 ? 1.0f : 0.0f) * 15;
    int leet = s.leetcode_problems;
    score += (leet >= 200 ? 1.0f : leet >= 100 ? 0.7f : leet >= 50 ? 0.4f : 0.1f) * 15;
    if (score > 100) score = 100;
    int placement_prob = (int)score;

    Prediction top_companies[MAX_COMPANIES];
    int n = run_predictions(&s, top_companies, MAX_COMPANIES);

    char* content = malloc(24576);
    if (!content) {
        send_response(client, "500 Internal Server Error", "text/plain", "Out of memory.");
        return;
    }
    
    char fill_color[16];
    if (placement_prob >= 75) strcpy(fill_color, "#10b981");
    else if (placement_prob >= 50) strcpy(fill_color, "#f59e0b");
    else strcpy(fill_color, "#ef4444");

    const char* verdict_class;
    const char* verdict_text;
    if (placement_prob >= 80) {
        verdict_class = "verdict-excellent";
        verdict_text = "<strong>EXCELLENT:</strong> You have an exceptional profile! Keep practicing interview questions and you will ace your placements easily.";
    } else if (placement_prob >= 60) {
        verdict_class = "verdict-good";
        verdict_text = "<strong>GOOD:</strong> You have high readiness. Focus on refining your projects and practicing advanced DSA topics.";
    } else if (placement_prob >= 40) {
        verdict_class = "verdict-average";
        verdict_text = "<strong>AVERAGE:</strong> Decent readiness, but you need to eliminate backlogs (if any), build 1-2 stronger projects, and practice DSA coding.";
    } else {
        verdict_class = "verdict-needs-work";
        verdict_text = "<strong>NEEDS WORK:</strong> Your placement index is low. Start upskilling immediately, complete at least one web/core project, and work hard on basics.";
    }

    int written = snprintf(content, 24576,
        "<h1>Placement Diagnostics Report</h1>"
        "<p class='subtitle'>Tailored prediction and readiness diagnostics for %s</p>"
        "<h2>Overall Placement Probability</h2>"
        "<div class='progress-bar'>"
        "<div class='progress-fill' style='width: %d%%; background: %s;'>%d%%</div>"
        "</div>"
        "<div class='verdict-box %s'>%s</div>"
        "<h2>Best Recruiter Matching</h2>"
        "<table class='results-table'>"
        "<tr><th>Company</th><th>Tier</th><th>Match Score</th><th>Est. Salary</th><th>Recommendation</th></tr>",
        s.name, placement_prob, fill_color, placement_prob, verdict_class, verdict_text
    );
    
    for (int i = 0; i < n; i++) {
        char row[512];
        const char* badge_class;
        if (strcmp(top_companies[i].recommendation, "APPLY NOW") == 0) badge_class = "badge-apply";
        else if (strcmp(top_companies[i].recommendation, "MAYBE") == 0) badge_class = "badge-maybe";
        else badge_class = "badge-skip";
        
        snprintf(row, sizeof(row),
            "<tr>"
            "<td><strong>%s</strong></td>"
            "<td><span class='badge badge-tier'>Tier %s</span></td>"
            "<td>%d%%</td>"
            "<td>%d LPA</td>"
            "<td><span class='badge %s'>%s</span></td>"
            "</tr>",
            top_companies[i].company_name, top_companies[i].tier, top_companies[i].match_score,
            top_companies[i].estimated_salary, badge_class, top_companies[i].recommendation
        );
        
        if (written + strlen(row) < 24000) {
            strcat(content, row);
            written += strlen(row);
        }
    }
    
    /* ── Skill Gap Analysis ── */
    char gap_section[4096];
    int gw = snprintf(gap_section, sizeof(gap_section),
        "</table>"
        "<h2>Skill Gap Analysis</h2>"
        "<p class='subtitle' style='text-align:left; margin-bottom:16px;'>What you are missing for each company you matched with</p>"
    );
    for (int i = 0; i < n && gw < (int)sizeof(gap_section) - 512; i++) {
        Company *c = NULL;
        for (int k = 0; k < NUM_COMPANIES; k++)
            if (strcmp(companies[k].name, top_companies[i].company_name) == 0)
                { c = &companies[k]; break; }
        if (!c) continue;

        int cgpa_ok   = (s.cgpa >= c->min_cgpa);
        int skill_ok  = has_skill(&s, c->required_skill);
        int backlog_ok = (s.backlogs == 0 || (c->allow_backlogs && s.backlogs <= 2));
        int leet_ok   = !(strcmp(c->domain,"Tech")==0 && strcmp(c->tier,"A")==0 && s.leetcode_problems < 100);

        char card[512];
        snprintf(card, sizeof(card),
            "<div class='gap-card'>"
            "<h4>%s &nbsp;<span class='badge badge-tier'>Tier %s</span></h4>"
            "<ul>"
            "<li class='%s'>CGPA %.1f required — you have %.2f</li>"
            "<li class='%s'>Key skill required: %s</li>"
            "<li class='%s'>Backlogs: %s</li>"
            "%s"
            "</ul>"
            "</div>",
            c->name, c->tier,
            cgpa_ok ? "has-skill" : "", c->min_cgpa, s.cgpa,
            skill_ok ? "has-skill" : "", c->required_skill,
            backlog_ok ? "has-skill" : "",
            s.backlogs == 0 ? "No active backlogs — clean record" : (c->allow_backlogs ? "Backlogs allowed (max 2) — you qualify" : "Backlogs NOT allowed here — clear them first"),
            (!leet_ok) ? "<li>LeetCode: need 100+ problems for Tier-A Tech (you have fewer)</li>" : ""
        );
        if (gw + (int)strlen(card) < (int)sizeof(gap_section) - 10) {
            strcat(gap_section, card);
            gw += strlen(card);
        }
    }
    if (written + gw < 23000) {
        strcat(content, gap_section);
        written += gw;
    }

    char bio_section[2048];
    snprintf(bio_section, sizeof(bio_section),
        "<h2>AI-Generated LinkedIn Bio</h2>"
        "<div class='bio-box'>"
        "Aspiring %s professional with a CGPA of %.2f from the %s department. "
        "%s%s%s%s"
        "Proficient in %s. "
        "%s"
        "Passionate about solving complex problems and eager to contribute to the %s industry!"
        "</div>"
        "<div style='margin-top: 30px; display: flex; gap: 15px;'>"
        "<a href='/' class='btn' style='margin-top:0; flex: 1;'>Evaluate Another Profile</a>"
        "<a href='/roadmap' class='btn btn-secondary' style='margin-top:0; flex: 1;'>Get Study Roadmap</a>"
        "</div>",
        s.preferred_domain, s.cgpa, s.branch,
        s.projects > 0 ? "Hands-on experience in building " : "",
        s.projects > 0 ? (s.projects > 1 ? "multiple high-impact projects " : "a technical project ") : "",
        s.internships > 0 ? "and completed " : "",
        s.internships > 0 ? (s.internships > 1 ? "industry internships. " : "an industry internship. ") : "",
        s.num_skills > 0 ? s.skills[0] : "Software Engineering",
        (strcmp(s.preferred_domain, "Tech") == 0 && s.leetcode_problems > 0) ? "Active coding practitioner on LeetCode. " : "",
        s.preferred_domain
    );
    
    if (written + strlen(bio_section) < 24000) {
        strcat(content, bio_section);
    }
    
    serve_page(client, "predictor", content);
    free(content);
}

void handle_get_resume(SOCKET client) {
    char body[4096];
    snprintf(body, sizeof(body),
        "<h1>Resume Performance Scorer</h1>"
        "<p class='subtitle'>Analyze your resume against recruiting standards and get concrete optimization steps</p>"
        "<form method='POST' action='/resume'>"
        "<h2>Resume Checklist</h2>"
        "<div class='form-group checkbox-group'>"
        "<input type='checkbox' id='github' name='github' value='1'>"
        "<label for='github'>Active GitHub profile with visible public repositories</label>"
        "</div>"
        "<div class='form-group checkbox-group'>"
        "<input type='checkbox' id='linkedin' name='linkedin' value='1'>"
        "<label for='linkedin'>Professional LinkedIn profile with profile picture and detailed bio</label>"
        "</div>"
        "<div class='form-group checkbox-group'>"
        "<input type='checkbox' id='project' name='project' value='1'>"
        "<label for='project'>At least one live deployed project link on your resume (e.g. Netlify, Vercel, Heroku)</label>"
        "</div>"
        "<div class='form-group checkbox-group'>"
        "<input type='checkbox' id='cp' name='cp' value='1'>"
        "<label for='cp'>Competitive coding profile (e.g. LeetCode, Codeforces, HackerRank)</label>"
        "</div>"
        "<div class='form-group checkbox-group'>"
        "<input type='checkbox' id='onepage' name='onepage' value='1'>"
        "<label for='onepage'>Resume fits strictly on a single page (highly recommended for graduates)</label>"
        "</div>"
        "<h2>Additional Qualifications</h2>"
        "<div class='form-group'>"
        "<label>Number of Professional Certifications</label>"
        "<input type='number' min='0' name='certs' value='0' style='max-width: 200px;' required>"
        "</div>"
        "<button type='submit' class='btn'>Run Resume Diagnostic</button>"
        "</form>"
    );
    serve_page(client, "resume", body);
}

void handle_post_resume(SOCKET client, const char* body) {
    int has_github = 0, has_linkedin = 0, has_deployed = 0, has_cp = 0, is_onepage = 0;
    int certs = 0;
    char temp[50];
    
    extract_value(body, "github", temp, sizeof(temp));
    if (strcmp(temp, "1") == 0) has_github = 1;
    
    extract_value(body, "linkedin", temp, sizeof(temp));
    if (strcmp(temp, "1") == 0) has_linkedin = 1;
    
    extract_value(body, "project", temp, sizeof(temp));
    if (strcmp(temp, "1") == 0) has_deployed = 1;
    
    extract_value(body, "cp", temp, sizeof(temp));
    if (strcmp(temp, "1") == 0) has_cp = 1;
    
    extract_value(body, "onepage", temp, sizeof(temp));
    if (strcmp(temp, "1") == 0) is_onepage = 1;
    
    extract_value(body, "certs", temp, sizeof(temp));
    certs = atoi(temp);

    int score = 0;
    if (has_github)   score += 15;
    if (has_linkedin) score += 10;
    if (has_deployed) score += 25;
    if (has_cp)       score += 15;
    if (is_onepage)   score += 15;
    score += (certs >= 2) ? 20 : (certs == 1) ? 10 : 0;
    
    char* content = malloc(8192);
    if (!content) {
        send_response(client, "500 Internal Server Error", "text/plain", "Out of memory.");
        return;
    }

    const char* verdict;
    const char* fill_color;
    if (score >= 80) {
        verdict = "EXCELLENT RESUME STRUCTURE!";
        fill_color = "#10b981";
    } else if (score >= 60) {
        verdict = "GOOD RESUME STRUCTURE!";
        fill_color = "#f59e0b";
    } else {
        verdict = "RESUME NEEDS WORK!";
        fill_color = "#ef4444";
    }

    snprintf(content, 8192,
        "<h1>Resume Evaluation Score</h1>"
        "<p class='subtitle'>Detailed diagnostic checklist and feedback on your resume</p>"
        "<div class='score-circle-container'>"
        "<div class='score-circle' style='box-shadow: 0 0 30px rgba(139,92,246,0.2); border-color: %s;'>"
        "<span class='score-num' style='color: %s;'>%d</span>"
        "<span class='score-label'>Score</span>"
        "</div>"
        "</div>"
        "<h3 style='text-align: center; margin-bottom: 25px; color: %s;'>%s</h3>"
        "<h2>Itemized Evaluation Checklist</h2>"
        "<ul class='checklist'>"
        
        "<li class='checklist-item %s'>"
        "<span class='%s'>%s</span>"
        "<div><strong>GitHub Profile Integration</strong><br>"
        "%s</div>"
        "</li>"
        
        "<li class='checklist-item %s'>"
        "<span class='%s'>%s</span>"
        "<div><strong>LinkedIn Professional Visibility</strong><br>"
        "%s</div>"
        "</li>"
        
        "<li class='checklist-item %s'>"
        "<span class='%s'>%s</span>"
        "<div><strong>Live Technical Project Links</strong><br>"
        "%s</div>"
        "</li>"
        
        "<li class='checklist-item %s'>"
        "<span class='%s'>%s</span>"
        "<div><strong>Coding/Competitive Profile</strong><br>"
        "%s</div>"
        "</li>"
        
        "<li class='checklist-item %s'>"
        "<span class='%s'>%s</span>"
        "<div><strong>One-Page Resume Compactness</strong><br>"
        "%s</div>"
        "</li>"
        
        "<li class='checklist-item %s'>"
        "<span class='%s'>%s</span>"
        "<div><strong>Professional Certifications</strong><br>"
        "You have added %d certification(s). %s</div>"
        "</li>"
        
        "</ul>"
        "<div style='margin-top: 30px; display: flex; gap: 15px;'>"
        "<a href='/resume' class='btn' style='margin-top: 0; flex: 1;'>Recheck Resume</a>"
        "<a href='/' class='btn btn-secondary' style='margin-top: 0; flex: 1;'>Evaluate Placements</a>"
        "</div>",
        fill_color, fill_color, score, fill_color, verdict,
        
        has_github ? "success" : "warning",
        has_github ? "icon-check" : "icon-warn",
        has_github ? "✓" : "⚠",
        has_github ? "Excellent! Recruiters can see your code repositories directly, demonstrating transparency." : "Add a link to your active GitHub account. Showcase real code to establish baseline programming skills.",
        
        has_linkedin ? "success" : "warning",
        has_linkedin ? "icon-check" : "icon-warn",
        has_linkedin ? "✓" : "⚠",
        has_linkedin ? "Perfect! Professional social profiles make finding and screening you easy." : "Establish and list a formal LinkedIn profile. Networking and visibility are key channels for high tier jobs.",
        
        has_deployed ? "success" : "warning",
        has_deployed ? "icon-check" : "icon-warn",
        has_deployed ? "✓" : "⚠",
        has_deployed ? "Exceptional! Deployed projects show you can ship fully operational programs." : "Host and link your project online! A live app carries 5x the weight of static codebase listings.",
        
        has_cp ? "success" : "warning",
        has_cp ? "icon-check" : "icon-warn",
        has_cp ? "✓" : "⚠",
        has_cp ? "Great job! Strong algorithmic presence guarantees clearing coding screen rounds." : "Build a profile on platforms like LeetCode or HackerRank. Essential to prove problem-solving capabilities.",
        
        is_onepage ? "success" : "warning",
        is_onepage ? "icon-check" : "icon-warn",
        is_onepage ? "✓" : "⚠",
        is_onepage ? "Excellent. Fits well within a standard 6-second quick recruiter scan." : "Trim details to keep resume to 1 page. Bullet-points are much easier to scan than walls of text.",
        
        certs >= 1 ? "success" : "warning",
        certs >= 1 ? "icon-check" : "icon-warn",
        certs >= 1 ? "✓" : "⚠",
        certs,
        certs >= 2 ? "Outstanding credentials! Your specialization and continuous learning are well validated." :
        certs == 1 ? "Good effort. Add one more professional domain certification (AWS, Oracle, Cisco) to stand out." :
                     "Acquire professional domain certifications. Validates structural skill competencies."
    );

    serve_page(client, "resume", content);
    free(content);
}

void handle_get_compare(SOCKET client) {
    char body[4096];
    int written = snprintf(body, sizeof(body),
        "<h1>Recruiter Comparison</h1>"
        "<p class='subtitle'>Compare two campus recruiters side-by-side to understand criteria and choose your target companies</p>"
        "<form method='POST' action='/compare'>"
        "<h2>Select Recruiting Companies</h2>"
        "<div class='grid'>"
        "<div class='form-group'>"
        "<label>First Company</label>"
        "<select name='c1'>"
    );
    
    for (int i = 0; i < NUM_COMPANIES; i++) {
        char opt[128];
        snprintf(opt, sizeof(opt), "<option value='%s'>%s (%s)</option>", companies[i].name, companies[i].name, companies[i].domain);
        strcat(body, opt);
        written += strlen(opt);
    }
    
    char middle[] = 
        "</select>"
        "</div>"
        "<div class='form-group'>"
        "<label>Second Company</label>"
        "<select name='c2'>";
    strcat(body, middle);
    written += strlen(middle);

    for (int i = 0; i < NUM_COMPANIES; i++) {
        char opt[128];
        snprintf(opt, sizeof(opt), "<option value='%s' %s>%s (%s)</option>", 
                 companies[i].name, (i == 1 ? "selected" : ""), companies[i].name, companies[i].domain);
        strcat(body, opt);
        written += strlen(opt);
    }

    char end[] =
        "</select>"
        "</div>"
        "</div>"
        "<button type='submit' class='btn'>Run Comparative Analysis</button>"
        "</form>";
    strcat(body, end);

    serve_page(client, "compare", body);
}

void handle_post_compare(SOCKET client, const char* body) {
    char c1_name[MAX_NAME] = {0};
    char c2_name[MAX_NAME] = {0};
    
    extract_value(body, "c1", c1_name, sizeof(c1_name));
    extract_value(body, "c2", c2_name, sizeof(c2_name));

    Company *comp1 = NULL;
    Company *comp2 = NULL;
    
    for (int i = 0; i < NUM_COMPANIES; i++) {
        if (strcmp(companies[i].name, c1_name) == 0) comp1 = &companies[i];
        if (strcmp(companies[i].name, c2_name) == 0) comp2 = &companies[i];
    }

    char* content = malloc(8192);
    if (!content) {
        send_response(client, "500 Internal Server Error", "text/plain", "Out of memory.");
        return;
    }

    if (!comp1 || !comp2) {
        snprintf(content, 8192, "<h1>Error</h1><p>Invalid company choices.</p><a href='/compare' class='btn'>Go Back</a>");
        serve_page(client, "compare", content);
        free(content);
        return;
    }

    snprintf(content, 8192,
        "<h1>Recruiter Comparison</h1>"
        "<p class='subtitle'>Side-by-side parameter analysis between %s and %s</p>"
        "<div class='grid' style='margin-bottom: 30px;'>"
        "<div class='compare-card'>"
        "<div class='compare-title'>%s</div>"
        "<span class='badge badge-tier'>Tier %s Recruiter</span>"
        "</div>"
        "<div class='compare-card'>"
        "<div class='compare-title'>%s</div>"
        "<span class='badge badge-tier'>Tier %s Recruiter</span>"
        "</div>"
        "</div>"
        
        "<table class='results-table'>"
        "<tr><th>Recruiter Criterion</th><th>%s</th><th>%s</th></tr>"
        "<tr><td><strong>Domain Industry</strong></td><td>%s</td><td>%s</td></tr>"
        "<tr><td><strong>Minimum CGPA Cutoff</strong></td><td>%.2f / 10.0</td><td>%.2f / 10.0</td></tr>"
        "<tr><td><strong>CTC Salary Package</strong></td><td style='color:#34d399;'><strong>%d LPA</strong></td><td style='color:#34d399;'><strong>%d LPA</strong></td></tr>"
        "<tr><td><strong>Key Required Skill</strong></td><td><span class='badge badge-tier'>%s</span></td><td><span class='badge badge-tier'>%s</span></td></tr>"
        "<tr><td><strong>Backlogs Allowed?</strong></td><td>%s</td><td>%s</td></tr>"
        "</table>"
        
        "<div style='margin-top: 30px; display: flex; gap: 15px;'>"
        "<a href='/compare' class='btn' style='margin-top:0; flex: 1;'>Compare Other Teams</a>"
        "<a href='/' class='btn btn-secondary' style='margin-top:0; flex: 1;'>Check Readiness</a>"
        "</div>",
        comp1->name, comp2->name,
        comp1->name, comp1->tier,
        comp2->name, comp2->tier,
        comp1->name, comp2->name,
        comp1->domain, comp2->domain,
        comp1->min_cgpa, comp2->min_cgpa,
        comp1->base_salary, comp2->base_salary,
        comp1->required_skill, comp2->required_skill,
        comp1->allow_backlogs ? "Yes (Max 2)" : "Strictly No", comp2->allow_backlogs ? "Yes (Max 2)" : "Strictly No"
    );

    serve_page(client, "compare", content);
    free(content);
}

void handle_get_roadmap(SOCKET client) {
    char body[4096];
    snprintf(body, sizeof(body),
        "<h1>Placement Study Roadmap</h1>"
        "<p class='subtitle'>Generate a week-by-week curriculum customized to your target domain and current skill level</p>"
        "<form method='POST' action='/roadmap'>"
        "<h2>Preparation Strategy Parameters</h2>"
        "<div class='grid'>"
        "<div class='form-group'>"
        "<label>Target Career Domain</label>"
        "<select name='domain'>"
        "<option value='Tech'>Tech (Software, Data, Cloud)</option>"
        "<option value='Finance'>Finance (Quant, Investment)</option>"
        "<option value='Consulting'>Consulting (Strategy, Advisory)</option>"
        "<option value='Core'>Core Engineering (Electronics, Systems)</option>"
        "</select>"
        "</div>"
        "<div class='form-group'>"
        "<label>Your Current Level</label>"
        "<select name='level'>"
        "<option value='Beginner'>Beginner (Just starting coding / prep)</option>"
        "<option value='Intermediate'>Intermediate (Knows basics, needs practice)</option>"
        "<option value='Advanced'>Advanced (Strong foundation, targeting Tier A)</option>"
        "</select>"
        "</div>"
        "</div>"
        "<button type='submit' class='btn'>Build Week-by-Week Roadmap</button>"
        "</form>"
    );
    serve_page(client, "roadmap", body);
}

void handle_post_roadmap(SOCKET client, const char* body) {
    char domain[MAX_NAME] = {0};
    char level[MAX_NAME] = {0};
    
    extract_value(body, "domain", domain, sizeof(domain));
    extract_value(body, "level", level, sizeof(level));

    char* content = malloc(8192);
    if (!content) {
        send_response(client, "500 Internal Server Error", "text/plain", "Out of memory.");
        return;
    }

    char weeks[6][256];
    
    if (strcmp(domain, "Tech") == 0) {
        if (strcmp(level, "Beginner") == 0) {
            strcpy(weeks[0], "Master C/C++ Basics (Variables, Loops, Functions, Arrays)");
            strcpy(weeks[1], "Understand Pointers, Structures, and Dynamic Memory (malloc/free)");
            strcpy(weeks[2], "Basic Data Structures (Singly Linked Lists, Stacks, Queues)");
            strcpy(weeks[3], "Searching & Sorting Algorithms (Bubble, Selection, Binary Search)");
            strcpy(weeks[4], "Time & Space Complexity analysis (Big-O notation, basics)");
            strcpy(weeks[5], "Practice 30 easy coding questions on LeetCode/HackerRank");
        } else if (strcmp(level, "Intermediate") == 0) {
            strcpy(weeks[0], "Advanced Structures (Recursion, Trees, Binary Search Tree concepts)");
            strcpy(weeks[1], "Master HashMaps, HashSets, and two-pointer coding techniques");
            strcpy(weeks[2], "Solve 40 classic interview questions (Strings, Matrix, Linked List)");
            strcpy(weeks[3], "Object Oriented Programming (Inheritance, Polymorphism, Virtual Fn)");
            strcpy(weeks[4], "Database Management (SQL queries, Joins, Indexing, Normalization)");
            strcpy(weeks[5], "Build a simple web/C project and upload to GitHub");
        } else {
            strcpy(weeks[0], "Advanced Algorithms (Dynamic Programming, Graph DFS/BFS)");
            strcpy(weeks[1], "System Design Basics (Scalability, Load Balancers, Microservices)");
            strcpy(weeks[2], "Solve 60 Medium/Hard problems on LeetCode");
            strcpy(weeks[3], "Mock Interviews on Pramp & Resume polishing");
            strcpy(weeks[4], "Operating Systems concepts (Threads, Virtual Memory, Deadlocks)");
            strcpy(weeks[5], "Behavioral Interview Prep (STAR framework, team leading skills)");
        }
    } else if (strcmp(domain, "Finance") == 0) {
        if (strcmp(level, "Beginner") == 0) {
            strcpy(weeks[0], "Learn basic Financial Terms (Balance sheet, P&L, Cash Flow)");
            strcpy(weeks[1], "Master Microsoft Excel basics (VLOOKUP, Pivot Tables, shortcuts)");
            strcpy(weeks[2], "Basic Probability & Statistics (Mean, Median, Standard Deviation)");
            strcpy(weeks[3], "Introduction to Stock Markets & Corporate Finance basics");
            strcpy(weeks[4], "Learn Python scripting basics for financial calculators");
            strcpy(weeks[5], "Read standard business news (Financial Times, Moneycontrol)");
        } else {
            strcpy(weeks[0], "Valuation methodologies (DCF, Comparable Company Analysis)");
            strcpy(weeks[1], "Advanced Statistics (Hypothesis testing, Regression, correlation)");
            strcpy(weeks[2], "Financial Modeling (Building forecast models in Excel)");
            strcpy(weeks[3], "Quantitative Finance (Time Series analysis, portfolio variance)");
            strcpy(weeks[4], "Python for Data Analysis (Pandas, NumPy, Matplotlib data frames)");
            strcpy(weeks[5], "Practice case studies on company financial statement evaluation");
        }
    } else if (strcmp(domain, "Consulting") == 0) {
        strcpy(weeks[0], "Learn structural thinking (MECE frameworks, Issue Trees)");
        strcpy(weeks[1], "Master Profitability Frameworks & Market Entry business cases");
        strcpy(weeks[2], "Practice Guesstimates (e.g. Estimate size of market for X)");
        strcpy(weeks[3], "Business case studies (Read Victor Cheng - Look Over Shoulder)");
        strcpy(weeks[4], "Advanced slide design (PowerPoint layout, storyboarding ideas)");
        strcpy(weeks[5], "Do 10 live mock case interviews with case partners");
    } else { 
        strcpy(weeks[0], "Review basic Engineering Mechanics & Thermodynamics");
        strcpy(weeks[1], "Advanced Circuit Analysis / Material Science specifications");
        strcpy(weeks[2], "Hands-on simulation tools (MATLAB, Simulink, AutoCAD basics)");
        strcpy(weeks[3], "Embedded Systems basics (Microcontrollers 8051, Arduino, I/O)");
        strcpy(weeks[4], "Practice C/C++ micro-code for low level register operations");
        strcpy(weeks[5], "Review core hardware project architecture for interview pitch");
    }

    snprintf(content, 8192,
        "<h1>Personalized Study Roadmap</h1>"
        "<p class='subtitle'>Weekly learning curriculum for %s (%s Track)</p>"
        
        "<table class='results-table'>"
        "<tr><th style='width: 15%%;'>Timeline</th><th>Study Targets & Actionable Items</th></tr>"
        "<tr><td><strong>Week 1</strong></td><td>%s</td></tr>"
        "<tr><td><strong>Week 2</strong></td><td>%s</td></tr>"
        "<tr><td><strong>Week 3</strong></td><td>%s</td></tr>"
        "<tr><td><strong>Week 4</strong></td><td>%s</td></tr>"
        "<tr><td><strong>Week 5</strong></td><td>%s</td></tr>"
        "<tr><td><strong>Week 6</strong></td><td>%s</td></tr>"
        "</table>"
        
        "<div style='margin-top: 30px; display: flex; gap: 15px;'>"
        "<a href='/roadmap' class='btn' style='margin-top:0; flex: 1;'>Adjust Plan Parameters</a>"
        "<a href='/' class='btn btn-secondary' style='margin-top:0; flex: 1;'>Go to Main Hub</a>"
        "</div>",
        domain, level,
        weeks[0], weeks[1], weeks[2], weeks[3], weeks[4], weeks[5]
    );

    serve_page(client, "roadmap", content);
    free(content);
}

/* ═══════════════════════════════════════════════════════════════
   FEATURE: MOCK INTERVIEW Q&A  (/interview)
   ═══════════════════════════════════════════════════════════════ */

void handle_get_interview(SOCKET client) {
    char body[5120];
    int w = snprintf(body, sizeof(body),
        "<h1>Mock Interview Prep</h1>"
        "<p class='subtitle'>Select a company and level to get targeted interview questions with answer hints</p>"
        "<form method='POST' action='/interview'>"
        "<h2>Interview Configuration</h2>"
        "<div class='grid'>"
        "<div class='form-group'><label>Target Company</label><select name='company'>"
    );
    for (int i = 0; i < NUM_COMPANIES && w < 4800; i++) {
        char opt[100];
        snprintf(opt, sizeof(opt), "<option value='%s'>%s (%s)</option>", companies[i].name, companies[i].name, companies[i].domain);
        strcat(body, opt); w += strlen(opt);
    }
    char rest[] =
        "</select></div>"
        "<div class='form-group'><label>Interview Round</label><select name='round'>"
        "<option value='aptitude'>Aptitude &amp; Reasoning</option>"
        "<option value='technical'>Technical / Coding</option>"
        "<option value='hr'>HR &amp; Behavioral</option>"
        "</select></div>"
        "<div class='form-group'><label>Your Branch</label><select name='branch'>"
        "<option value='CSE'>Computer Science / IT</option>"
        "<option value='ECE'>Electronics &amp; Communication</option>"
        "<option value='EEE'>Electrical &amp; Electronics</option>"
        "<option value='MECH'>Mechanical</option>"
        "<option value='CIVIL'>Civil</option>"
        "<option value='CHEM'>Chemical</option>"
        "<option value='OTHER'>Other</option>"
        "</select></div>"
        "</div>"
        "<button type='submit' class='btn'>Generate Interview Questions</button>"
        "</form>";
    strcat(body, rest);
    serve_page(client, "interview", body);
}

void handle_post_interview(SOCKET client, const char* body) {
    char company[MAX_NAME] = {0};
    char round[MAX_NAME]   = {0};
    char branch[MAX_NAME]  = {0};
    extract_value(body, "company", company, sizeof(company));
    extract_value(body, "round",   round,   sizeof(round));
    extract_value(body, "branch",  branch,  sizeof(branch));

    /* Find domain for this company */
    char domain[MAX_NAME] = "Tech";
    for (int i = 0; i < NUM_COMPANIES; i++)
        if (strcmp(companies[i].name, company) == 0)
            { strncpy(domain, companies[i].domain, sizeof(domain)-1); break; }

    /* Question banks: 10 questions per round×domain combination */
    typedef struct { const char *q; const char *hint; } QA;

    QA aptitude_q[10] = {
        {"A train 150m long passes a pole in 15 seconds. What is the speed?",          "Speed = Distance / Time. Convert to km/h by × 18/5."},
        {"Find the next term: 2, 6, 12, 20, 30, __",                                  "Differences are 4, 6, 8, 10 — increasing by 2."},
        {"If 8 workers finish a job in 12 days, how long for 6 workers?",              "Use work = workers × days = constant."},
        {"A shopkeeper marks up 25% then gives 10% discount. Net profit %?",           "Apply markup then discount on cost price sequentially."},
        {"In a class, 40% are girls. 60% of boys and 80% of girls passed. Overall %?", "Weighted average: 0.6×0.6 + 0.4×0.8."},
        {"Clock shows 3:15. Angle between hour and minute hand?",                      "Minute at 90°, hour at 97.5°. Difference = 7.5°."},
        {"P is 30% more efficient than Q. Together they finish in 23 days. P alone?",  "Let Q = x days, P = x/1.3. Sum their rates."},
        {"A bag has 4 red and 5 blue balls. Two are drawn. P(both red)?",              "Probability = C(4,2)/C(9,2) = 6/36."},
        {"Find HCF of 180 and 252.",                                                   "Euclidean algorithm: 252 = 1×180 + 72; 180 = 2×72 + 36; HCF=36."},
        {"Simple interest on Rs 8000 at 5% for 3 years?",                             "SI = P×R×T/100 = 8000×5×3/100."}
    };

    QA tech_tech_q[10] = {
        {"Reverse a linked list in-place. Write the C function.",                       "Use three pointers: prev, curr, next. Iterate and rewire."},
        {"What is the time complexity of QuickSort in worst and average case?",         "Worst O(n²) with sorted input; average O(n log n) with random pivot."},
        {"Explain how a Hash Map handles collisions.",                                  "Chaining (linked list per bucket) or open addressing (linear probing)."},
        {"Find two numbers in an array that sum to a target — optimally.",              "Use a hash set: for each element x, check if (target−x) exists."},
        {"What is a deadlock? Give the four necessary conditions.",                     "Mutual exclusion, hold-and-wait, no preemption, circular wait."},
        {"Difference between stack and heap memory?",                                  "Stack: automatic, LIFO, function scope. Heap: manual/GC, dynamic lifetime."},
        {"Write SQL to find the 2nd highest salary from an Employees table.",           "SELECT MAX(salary) FROM Employees WHERE salary < (SELECT MAX(salary) ...)."},
        {"What does the 'volatile' keyword do in C?",                                  "Tells compiler not to cache the variable — re-read from memory each time."},
        {"Explain Binary Search Tree insertion and search complexity.",                 "Average O(log n); worst O(n) for unbalanced (degenerate) trees."},
        {"Given a string, check if it is a palindrome ignoring spaces and case.",       "Two-pointer from both ends, skip non-alphanumeric, compare lowercased."}
    };

    QA tech_finance_q[10] = {
        {"What is the difference between market cap and enterprise value?",             "EV = market cap + debt − cash. Includes debt obligations."},
        {"Explain NPV and IRR — when would you prefer one over the other?",            "NPV gives absolute value; IRR is a rate. Use NPV when comparing scales."},
        {"If a stock's P/E is 25 and EPS is Rs 40, what is the share price?",         "Price = P/E × EPS = 25 × 40 = Rs 1000."},
        {"Write a Python snippet to compute compound interest.",                        "A = P * (1 + r/n)**(n*t). Use math.pow or ** operator."},
        {"What does a yield curve inversion signal?",                                  "Short-term rates > long-term rates; historically precedes recession."},
        {"Difference between forwards and futures?",                                   "Forwards: OTC, customizable, no daily settlement. Futures: exchange, standardized, MTM daily."},
        {"What is Value at Risk (VaR)?",                                               "Maximum expected loss over given time horizon at a confidence level (e.g. 95%)."},
        {"Given monthly returns data, calculate annualized Sharpe Ratio in Python.",   "Sharpe = (mean_return − risk_free) / std_return × sqrt(12)."},
        {"What is the Capital Asset Pricing Model (CAPM)?",                            "E(R) = Rf + β(Rm − Rf). Beta measures systematic risk."},
        {"Explain Black-Scholes inputs for option pricing.",                           "Stock price, strike, time to expiry, volatility, risk-free rate."}
    };

    QA tech_consulting_q[10] = {
        {"A retailer's profits have dropped 20%. How would you diagnose this?",        "MECE split: revenue or cost side? Then drill: volume, price, mix, fixed/variable."},
        {"Estimate the number of ATMs in India.",                                      "Segment: urban/rural population, banking penetration, transactions/ATM."},
        {"How would you advise a traditional bank entering fintech?",                  "Porter's Five Forces + digital capability gap analysis + build/buy/partner."},
        {"A client wants to enter the EV market. Structure your approach.",            "Market sizing → competitive landscape → value chain → go-to-market → financials."},
        {"What Excel functions are essential for financial modeling?",                  "VLOOKUP/INDEX-MATCH, SUMIFS, NPV/IRR, IF nesting, pivot tables, data validation."},
        {"Your client's market share fell 5% in 6 months. What would you investigate?","Hypotheses: pricing, product, distribution, competitor move, customer churn."},
        {"How do you prioritize multiple workstreams under a tight deadline?",          "Impact-effort matrix. Delegate low-impact; escalate blockers; communicate trade-offs."},
        {"Crack a guesstimate: How many cups of chai are sold in Mumbai per day?",     "Population → % chai drinkers → avg cups/day → multiply."},
        {"Explain a profitability framework.",                                          "Profit = Revenue − Cost. Revenue = price × volume. Cost = fixed + variable."},
        {"What is the 80/20 principle and how is it used in consulting?",              "80% of effects come from 20% of causes. Focus analysis on high-impact levers."}
    };

    QA tech_core_q[10] = {
        {"Explain the difference between microcontroller and microprocessor.",          "MCU has CPU+memory+I/O on-chip. MPU needs external peripherals."},
        {"What is PWM and where is it used?",                                          "Pulse Width Modulation: varying duty cycle controls motor speed, LED brightness."},
        {"Derive Thevenin's theorem step by step.",                                    "Remove load, find Voc across terminals, then find Rth with sources zeroed."},
        {"What is the significance of a Bode plot?",                                  "Shows gain and phase vs frequency; used for stability and bandwidth analysis."},
        {"Explain memory-mapped I/O vs port-mapped I/O.",                             "Memory-mapped: I/O shares address space with RAM. Port-mapped: separate address space."},
        {"What is the difference between RISC and CISC architectures?",               "RISC: simple instructions, many registers, pipelining. CISC: complex instructions, fewer regs."},
        {"How does a PID controller work? What do P, I, and D each do?",             "P: proportional to error. I: eliminates steady-state. D: damps oscillations."},
        {"What causes harmonics in a power system and how are they mitigated?",       "Non-linear loads (inverters, VFDs). Mitigation: filters, active harmonic correction."},
        {"Explain the function of an interrupt in an embedded system.",               "Interrupts pause main execution to handle time-critical events asynchronously."},
        {"Write C code to blink an LED on port P1.0 of 8051 at 1Hz.",               "Toggle P1_0 bit with a delay loop. Timer/counter method is more precise."}
    };

    QA hr_q[10] = {
        {"Tell me about yourself.",                                                     "Structure: present role/study → past achievements → why this company."},
        {"Why do you want to join this company specifically?",                         "Research: company values, recent projects, culture. Align with your goals."},
        {"Describe a time you handled a conflict in a team.",                          "STAR: Situation, Task, Action you took, Result. Show empathy and resolution."},
        {"What is your greatest professional weakness?",                               "Name a real weakness; show concrete steps you are taking to fix it."},
        {"Where do you see yourself in 5 years?",                                     "Show ambition aligned with the company's growth path, not just personal goals."},
        {"Tell me about your most challenging project.",                               "Focus on your specific contribution, obstacles overcome, lessons learned."},
        {"How do you manage tight deadlines with multiple deliverables?",              "Prioritization, communication with stakeholders, time-boxing, delegation."},
        {"Have you ever failed at something? What did you learn?",                    "Be honest. Show self-awareness and specific changes in behavior after failure."},
        {"Why should we hire you over other candidates?",                             "Specific skills + cultural fit + concrete examples of impact. No generic answers."},
        {"Do you have any questions for us?",                                         "Always ask 2-3 thoughtful questions: team culture, success metrics, onboarding."}
    };

    QA *questions;
    if (strcmp(round, "aptitude") == 0) {
        questions = aptitude_q;
    } else if (strcmp(round, "hr") == 0) {
        questions = hr_q;
    } else {
        /* technical — role-specific domains override branch (a Finance or
           Consulting interview tests role knowledge regardless of your
           branch); otherwise pick the bank that matches YOUR branch, not
           just the target company's domain, so a CSE student and a
           Mechanical student targeting the same company get different,
           relevant technical questions. */
        if (strcmp(domain, "Finance") == 0)         questions = tech_finance_q;
        else if (strcmp(domain, "Consulting") == 0) questions = tech_consulting_q;
        else if (strcmp(branch, "CSE") == 0)        questions = tech_tech_q;
        else if (branch[0] == '\0')                 questions = (strcmp(domain, "Core") == 0) ? tech_core_q : tech_tech_q;
        else                                        questions = tech_core_q;
    }

    char *content = malloc(16384);
    if (!content) { send_response(client, "500 Internal Server Error", "text/plain", "Out of memory."); return; }

    const char *round_label = strcmp(round,"aptitude")?"":0;
    if      (strcmp(round,"aptitude")==0) round_label = "Aptitude & Reasoning";
    else if (strcmp(round,"technical")==0) round_label = "Technical";
    else                                   round_label = "HR & Behavioral";

    int w2 = snprintf(content, 16384,
        "<h1>Mock Interview Questions</h1>"
        "<p class='subtitle'>%s round for <strong>%s</strong> — study the hints before your interview</p>",
        round_label, company
    );

    for (int i = 0; i < 10 && w2 < 15800; i++) {
        char block[512];
        snprintf(block, sizeof(block),
            "<div class='interview-q'>"
            "<div class='q-num'>QUESTION %d / 10</div>"
            "<div class='q-text'>%s</div>"
            "<div class='q-hint'>💡 Hint: %s</div>"
            "</div>",
            i+1, questions[i].q, questions[i].hint
        );
        strcat(content, block);
        w2 += strlen(block);
    }

    char footer[512];
    snprintf(footer, sizeof(footer),
        "<div style='margin-top: 30px; display: flex; gap: 15px;'>"
        "<a href='/interview' class='btn' style='margin-top:0; flex:1;'>Try Another Round</a>"
        "<a href='/roadmap' class='btn btn-secondary' style='margin-top:0; flex:1;'>Study Roadmap</a>"
        "</div>"
    );
    strcat(content, footer);

    serve_page(client, "interview", content);
    free(content);
}

/* ═══════════════════════════════════════════════════════════════
   FEATURE: CGPA IMPROVEMENT SIMULATOR  (/simulator)
   ═══════════════════════════════════════════════════════════════ */

void handle_get_simulator(SOCKET client) {
    char body[5120];
    int w = snprintf(body, sizeof(body),
        "<h1>CGPA Improvement Simulator</h1>"
        "<p class='subtitle'>Enter your current profile and simulate how improving your CGPA unlocks new companies</p>"
        "<form method='POST' action='/simulator'>"
        "<h2>Current Profile</h2>"
        "<div class='grid'>"
        "<div class='form-group'><label>Current CGPA</label><input type='number' step='0.01' min='0' max='10' name='current_cgpa' placeholder='e.g. 7.20' required></div>"
        "<div class='form-group'><label>Target CGPA</label><input type='number' step='0.01' min='0' max='10' name='target_cgpa' placeholder='e.g. 8.50' required></div>"
        "<div class='form-group'><label>Active Backlogs</label><input type='number' min='0' name='backlogs' value='0' required></div>"
        "<div class='form-group'><label>Number of Skills</label><input type='number' min='0' max='10' name='num_skills' value='3' required></div>"
        "<div class='form-group'><label>Projects Done</label><input type='number' min='0' name='projects' value='1' required></div>"
        "<div class='form-group'><label>Internships</label><input type='number' min='0' name='internships' value='0' required></div>"
        "<div class='form-group'><label>LeetCode Solved</label><input type='number' min='0' name='leetcode' value='0'></div>"
        "<div class='form-group'><label>Preferred Domain</label><select name='domain'>"
        "<option value='Tech'>Tech</option>"
        "<option value='Finance'>Finance</option>"
        "<option value='Consulting'>Consulting</option>"
        "<option value='Core'>Core</option>"
        "</select></div>"
        "</div>"
        "<button type='submit' class='btn'>Simulate CGPA Impact</button>"
        "</form>"
    );
    serve_page(client, "simulator", body);
}

void handle_post_simulator(SOCKET client, const char* body) {
    char temp[64];
    float current_cgpa, target_cgpa;
    Student s_now, s_target;
    memset(&s_now, 0, sizeof(Student));
    memset(&s_target, 0, sizeof(Student));

    extract_value(body, "current_cgpa", temp, sizeof(temp)); current_cgpa = (float)atof(temp);
    extract_value(body, "target_cgpa",  temp, sizeof(temp)); target_cgpa  = (float)atof(temp);
    extract_value(body, "backlogs",     temp, sizeof(temp)); s_now.backlogs = s_target.backlogs = atoi(temp);
    extract_value(body, "num_skills",   temp, sizeof(temp)); s_now.num_skills = s_target.num_skills = atoi(temp);
    extract_value(body, "projects",     temp, sizeof(temp)); s_now.projects   = s_target.projects   = atoi(temp);
    extract_value(body, "internships",  temp, sizeof(temp)); s_now.internships = s_target.internships = atoi(temp);
    extract_value(body, "leetcode",     temp, sizeof(temp)); s_now.leetcode_problems = s_target.leetcode_problems = atoi(temp);
    extract_value(body, "domain", s_now.preferred_domain, sizeof(s_now.preferred_domain));
    strncpy(s_target.preferred_domain, s_now.preferred_domain, sizeof(s_target.preferred_domain)-1);

    s_now.cgpa    = current_cgpa;
    s_target.cgpa = target_cgpa;

    /* Fill a generic skill that matches each company's required skill */
    /* We'll compare raw score difference per company */

    char *content = malloc(16384);
    if (!content) { send_response(client, "500 Internal Server Error", "text/plain", "Out of memory."); return; }

    int w = snprintf(content, 16384,
        "<h1>CGPA Impact Simulation</h1>"
        "<p class='subtitle'>How improving from <strong>%.2f</strong> → <strong>%.2f</strong> changes your company match scores</p>"
        "<h2>Company-by-Company Score Difference</h2>",
        current_cgpa, target_cgpa
    );

    int newly_unlocked = 0;

    for (int i = 0; i < NUM_COMPANIES && w < 15500; i++) {
        int score_now    = compute_match_score(&s_now,    &companies[i]);
        int score_target = compute_match_score(&s_target, &companies[i]);
        int delta = score_target - score_now;

        int was_eligible  = (current_cgpa >= companies[i].min_cgpa);
        int now_eligible  = (target_cgpa  >= companies[i].min_cgpa);
        int newly = (!was_eligible && now_eligible);
        if (newly) newly_unlocked++;

        const char *bar_color = (score_target >= 70) ? "#10b981" : (score_target >= 50) ? "#f59e0b" : "#ef4444";
        char row[600];
        snprintf(row, sizeof(row),
            "<div class='sim-result-row'>"
            "<div class='sim-company-name'>%s<br><span style='font-size:0.75em;color:#6b7280;'>Tier %s</span></div>"
            "<div class='sim-bar-wrap'>"
            "<div class='sim-bar-fill' style='width:%d%%;background:%s;'>%d%%</div>"
            "</div>"
            "<div class='sim-status' style='color:%s;'>%s%d<br><span style='font-size:0.7em;color:#6b7280;'>was %d%%</span>"
            "%s"
            "</div>"
            "</div>",
            companies[i].name, companies[i].tier,
            score_target, bar_color, score_target,
            delta > 0 ? "#34d399" : (delta < 0 ? "#f87171" : "#9ca3af"),
            delta > 0 ? "▲+" : (delta < 0 ? "▼" : ""), delta,
            score_now,
            newly ? "<br><span style='color:#a855f7;font-size:0.72em;font-weight:700;'>★ UNLOCKED</span>" : ""
        );
        strcat(content, row);
        w += strlen(row);
    }

    char summary[512];
    snprintf(summary, sizeof(summary),
        "<div class='verdict-box' style='margin-top:24px; border-left-color:#a855f7; color:#c4b5fd;'>"
        "<strong>Summary:</strong> Raising your CGPA from %.2f to %.2f unlocks <strong>%d new compan%s</strong> and improves match scores across the board."
        "</div>"
        "<div style='margin-top: 24px; display: flex; gap: 15px;'>"
        "<a href='/simulator' class='btn' style='margin-top:0; flex:1;'>Simulate Another Scenario</a>"
        "<a href='/' class='btn btn-secondary' style='margin-top:0; flex:1;'>Full Predictor</a>"
        "</div>",
        current_cgpa, target_cgpa, newly_unlocked, newly_unlocked == 1 ? "y" : "ies"
    );
    strcat(content, summary);

    serve_page(client, "simulator", content);
    free(content);
}

/* ═══════════════════════════════════════════════════════════════
   FEATURE: BACKLOG COUNSELOR  (/backlogs)
   ═══════════════════════════════════════════════════════════════ */

void handle_get_backlogs(SOCKET client) {
    char body[3072];
    snprintf(body, sizeof(body),
        "<h1>Backlog Counselor</h1>"
        "<p class='subtitle'>Find companies that still recruit students with active backlogs, and get a clear action plan to clear them</p>"
        "<h2>Companies Open to Students With Backlogs</h2>"
        "<p style='color:#9ca3af; margin-bottom: 20px; font-size: 0.93em;'>The following companies allow up to 2 active backlogs at the time of interview. Clearing all backlogs before the placement season greatly improves your chances even at these companies.</p>"
    );

    for (int i = 0; i < NUM_COMPANIES; i++) {
        if (!companies[i].allow_backlogs) continue;
        char card[512];
        snprintf(card, sizeof(card),
            "<div class='backlog-card'>"
            "<h3>%s &nbsp;<span class='badge badge-tier'>Tier %s</span></h3>"
            "<div class='bc-meta'>Domain: %s &nbsp;|&nbsp; Min CGPA: %.1f &nbsp;|&nbsp; Package: %d LPA &nbsp;|&nbsp; Key Skill: %s</div>"
            "<div class='bc-tip'>Strategy: Focus on getting your CGPA above %.1f and learning %s basics. Even with backlogs, a strong aptitude score and good communication in the HR round can get you through.</div>"
            "</div>",
            companies[i].name, companies[i].tier,
            companies[i].domain, companies[i].min_cgpa, companies[i].base_salary, companies[i].required_skill,
            companies[i].min_cgpa, companies[i].required_skill
        );
        strncat(body, card, sizeof(body) - strlen(body) - 1);
    }

    char footer[] =
        "<h2>How to Clear Backlogs Fast</h2>"
        "<ul class='checklist'>"
        "<li class='checklist-item success'><span class='icon-check'>✓</span><div><strong>Identify the exact syllabus</strong> — download previous year papers from your college portal. Pattern recognition is key.</div></li>"
        "<li class='checklist-item success'><span class='icon-check'>✓</span><div><strong>Group study</strong> — find 2-3 batchmates in the same backlog subject. Explaining concepts to others doubles retention.</div></li>"
        "<li class='checklist-item success'><span class='icon-check'>✓</span><div><strong>Talk to the professor</strong> — ask for internal marks improvement tasks. Many allow supplementary assignments before the exam.</div></li>"
        "<li class='checklist-item success'><span class='icon-check'>✓</span><div><strong>Focus on passing, not perfecting</strong> — in the supplementary exam, aim for 50%+ consistently across all units, not 100% in one.</div></li>"
        "<li class='checklist-item warning'><span class='icon-warn'>⚠</span><div><strong>Clear ALL backlogs before placement season</strong> — even backlog-friendly companies prefer zero active backlogs on the day of joining.</div></li>"
        "</ul>"
        "<div style='margin-top: 24px;'><a href='/' class='btn'>Go to Full Predictor</a></div>";
    strncat(body, footer, sizeof(body) - strlen(body) - 1);

    serve_page(client, "backlogs", body);
}

/* ═══════════════════════════════════════════════════════════════
   FEATURE: COMPANY-SPECIFIC RESUME GENERATOR  (/resume-gen)
   ═══════════════════════════════════════════════════════════════ */

void handle_get_resumegen(SOCKET client) {
    char body[7168];
    int w = snprintf(body, sizeof(body),
        "<h1>Tailored Resume Builder</h1>"
        "<p class='subtitle'>Generate a company-specific resume draft that highlights exactly what each recruiter looks for</p>"
        "<form method='POST' action='/resume-gen'>"
        "<h2>Your Details</h2>"
        "<div class='grid'>"
        "<div class='form-group'><label>Full Name</label><input type='text' name='name' placeholder='e.g. Priya Sharma' required></div>"
        "<div class='form-group'><label>Branch / Degree</label><input type='text' name='branch' placeholder='e.g. B.Tech Computer Science' required></div>"
        "<div class='form-group'><label>CGPA</label><input type='number' step='0.01' min='0' max='10' name='cgpa' placeholder='e.g. 8.2' required></div>"
        "<div class='form-group'><label>Email</label><input type='text' name='email' placeholder='e.g. priya@gmail.com'></div>"
        "<div class='form-group'><label>Phone</label><input type='text' name='phone' placeholder='e.g. +91 98765 43210'></div>"
        "<div class='form-group'><label>LinkedIn / GitHub</label><input type='text' name='links' placeholder='linkedin.com/in/priya | github.com/priya'></div>"
        "<div class='form-group'><label>Skills (comma separated)</label><input type='text' name='skills' placeholder='C, Python, SQL, DSA, Excel' required></div>"
        "<div class='form-group'><label>Projects (semicolon separated titles)</label><input type='text' name='projects' placeholder='Inventory System; Chat App' required></div>"
        "<div class='form-group'><label>Internship / Experience (semicolon separated)</label><input type='text' name='experience' placeholder='Web Dev Intern at XYZ, Jun-Aug 2024'></div>"
        "<div class='form-group'><label>Academic Certifications (comma separated)</label><input type='text' name='certs' placeholder='e.g. AWS Cloud Practitioner, Oracle SQL'></div>"
        "</div>"
        "<h2>Beyond Academics</h2>"
        "<p class='subtitle' style='margin-bottom:16px;'>Recruiters like seeing more than grades and coursework. List any non-academic certificates, awards or activities you hold &mdash; sports, volunteering, leadership roles, cultural/arts events, hackathons, NCC/NSS, club positions, etc. Just type the certificate or achievement title; you don't need to upload the physical file, it'll be added as its own resume section.</p>"
        "<div class='form-group'><label>Non-Academic Achievements / Certificates (comma separated)</label><input type='text' name='achievements' placeholder='e.g. State-level Badminton Runner-up, Certified First Aid Volunteer - Red Cross, NSS Camp Coordinator 2024'></div>"
        "<h2>Target Company</h2>"
        "<div class='form-group'><label>Select Company to Tailor For</label><select name='company'>"
    );
    for (int i = 0; i < NUM_COMPANIES && w < 6600; i++) {
        char opt[100];
        snprintf(opt, sizeof(opt), "<option value='%s'>%s (%s)</option>", companies[i].name, companies[i].name, companies[i].domain);
        strcat(body, opt); w += strlen(opt);
    }
    char rest[] =
        "</select></div>"
        "<button type='submit' class='btn'>Generate Tailored Resume</button>"
        "</form>";
    strcat(body, rest);
    serve_page(client, "resumegen", body);
}

void handle_post_resumegen(SOCKET client, const char* body) {
    char name[MAX_NAME]={0}, branch[100]={0}, email[100]={0}, phone[50]={0};
    char links[200]={0}, skills_raw[300]={0}, projects_raw[300]={0};
    char experience[300]={0}, certs[300]={0}, achievements_raw[300]={0}, company_name[MAX_NAME]={0};
    char cgpa_s[20]={0};

    extract_value(body, "name",         name,             sizeof(name));
    extract_value(body, "branch",       branch,           sizeof(branch));
    extract_value(body, "cgpa",         cgpa_s,           sizeof(cgpa_s));
    extract_value(body, "email",        email,            sizeof(email));
    extract_value(body, "phone",        phone,            sizeof(phone));
    extract_value(body, "links",        links,            sizeof(links));
    extract_value(body, "skills",       skills_raw,       sizeof(skills_raw));
    extract_value(body, "projects",     projects_raw,     sizeof(projects_raw));
    extract_value(body, "experience",   experience,       sizeof(experience));
    extract_value(body, "certs",        certs,            sizeof(certs));
    extract_value(body, "achievements", achievements_raw, sizeof(achievements_raw));
    extract_value(body, "company",      company_name,     sizeof(company_name));

    Company *c = NULL;
    for (int i = 0; i < NUM_COMPANIES; i++)
        if (strcmp(companies[i].name, company_name) == 0) { c = &companies[i]; break; }

    char *content = malloc(16384);
    if (!content) { send_response(client, "500 Internal Server Error", "text/plain", "Out of memory."); return; }

    /* Build company-specific objective */
    char objective[400];
    if (c) {
        snprintf(objective, sizeof(objective),
            "Results-driven %s graduate seeking a %s role at %s. "
            "Strong foundation in %s with demonstrated ability to deliver high-quality solutions. "
            "Eager to contribute to %s's mission through technical excellence and collaborative problem-solving.",
            branch, c->domain, c->name, c->required_skill, c->name
        );
    } else {
        snprintf(objective, sizeof(objective),
            "Motivated %s graduate seeking a challenging role in the industry. "
            "Strong technical background with hands-on project experience and a passion for continuous learning.",
            branch
        );
    }

    /* Build recruiter tip */
    const char *tip;
    if (!c)                          tip = "Tailor your objective for every company. Generic objectives are the #1 reason resumes get rejected.";
    else if (strcmp(c->tier,"A")==0) tip = "Tier A companies scan resumes in under 10 seconds. Lead with CGPA and your strongest project. Put your GitHub link prominently. Mention the key skill in your objective line.";
    else if (strcmp(c->tier,"B")==0) tip = "Tier B recruiters value domain exposure. Highlight any internship, even if short. Certifications in the company domain carry extra weight here.";
    else                             tip = "Service companies look for communication and learning agility. Emphasize team projects, certifications, and willingness to work across domains.";

    /* Build skill note banner (on-screen coaching tip only — hidden from print/PDF) */
    char skill_banner[300] = "";
    if (c && c->required_skill[0]) {
        snprintf(skill_banner, sizeof(skill_banner),
            "<p class='tip-box-print-hide' style='color:#fbbf24; font-size:0.88em; margin-bottom:14px;'>&#11088; Key skill for %s: <strong>%s</strong> &mdash; make sure it appears prominently in your resume.</p>",
            c->name, c->required_skill);
    }

    /* Pre-build contact line */
    char contact_line[300] = "";
    if (email[0]) strncat(contact_line, email, sizeof(contact_line)-strlen(contact_line)-1);
    if (email[0] && phone[0]) strncat(contact_line, " | ", sizeof(contact_line)-strlen(contact_line)-1);
    if (phone[0]) strncat(contact_line, phone, sizeof(contact_line)-strlen(contact_line)-1);
    if ((email[0] || phone[0]) && links[0]) strncat(contact_line, " | ", sizeof(contact_line)-strlen(contact_line)-1);
    if (links[0]) strncat(contact_line, links, sizeof(contact_line)-strlen(contact_line)-1);

    /* Turn raw comma/semicolon fields into proper bullet-list HTML instead of
       dumping raw text, so the output reads like an actual resume. */
    char skills_list[1200], projects_list[1200], experience_list[1200];
    char certs_list[1200], achievements_list[1200];
    render_list_items(skills_raw,       ',', skills_list,       sizeof(skills_list));
    render_list_items(projects_raw,     ';', projects_list,     sizeof(projects_list));
    render_list_items(experience,       ';', experience_list,   sizeof(experience_list));
    render_list_items(certs,            ',', certs_list,        sizeof(certs_list));
    render_list_items(achievements_raw, ',', achievements_list, sizeof(achievements_list));

    char skills_html[1300], projects_html[1300];
    snprintf(skills_html, sizeof(skills_html),
        skills_list[0] ? "<ul class='rg-list'>%s</ul>" : "<p class='rg-empty'>%s</p>",
        skills_list[0] ? skills_list : "No skills listed");
    snprintf(projects_html, sizeof(projects_html),
        projects_list[0] ? "<ul class='rg-list'>%s</ul>" : "<p class='rg-empty'>%s</p>",
        projects_list[0] ? projects_list : "No projects listed");

    char experience_section[1350] = "";
    if (experience_list[0]) {
        snprintf(experience_section, sizeof(experience_section),
            "<span class='rg-section'>Experience</span><ul class='rg-list'>%s</ul>", experience_list);
    }

    char certs_section[1350] = "";
    if (certs_list[0]) {
        snprintf(certs_section, sizeof(certs_section),
            "<span class='rg-section'>Certifications</span><ul class='rg-list'>%s</ul>", certs_list);
    }

    char achievements_section[1400] = "";
    if (achievements_list[0]) {
        snprintf(achievements_section, sizeof(achievements_section),
            "<span class='rg-section'>Achievements &amp; Extracurricular Activities</span><ul class='rg-list'>%s</ul>",
            achievements_list);
    }

    int w = snprintf(content, 16384,
        "<h1 class='tip-box-print-hide'>Your Tailored Resume</h1>"
        "<p class='subtitle'>Optimized for <strong>%s</strong> &mdash; use the button below to save it as a PDF, or copy it into your own template</p>"
        "%s"
        "<div class='resume-gen-box' id='resume-paper'>"
        "<span class='rg-name'>%s</span>"
        "<span class='rg-contact'>%s</span>"

        "<span class='rg-section'>Objective</span>"
        "<p class='rg-objective'>%s</p>"

        "<span class='rg-section'>Education</span>"
        "<div class='rg-edu-row'><strong>%s</strong><span>CGPA: %s / 10.0</span></div>"

        "<span class='rg-section'>Technical Skills</span>"
        "%s"

        "<span class='rg-section'>Projects</span>"
        "%s"

        "%s"
        "%s"
        "%s"

        "</div>"
        "<div style='margin-top: 20px; background: rgba(139,92,246,0.07); border: 1px solid rgba(139,92,246,0.2); border-radius: 10px; padding: 14px 18px; color: #c4b5fd; font-size: 0.9em;' class='tip-box-print-hide'>"
        "<strong>&#128161; Recruiter Tip for %s:</strong> %s"
        "</div>"
        "<div style='margin-top: 20px; display: flex; gap: 15px; flex-wrap: wrap;' class='tip-box-print-hide'>"
        "<button type='button' onclick='window.print()' class='btn' style='margin-top:0; flex:1; min-width:180px; border:none; cursor:pointer;'>&#128424; Save as PDF / Print</button>"
        "<a href='/resume-gen' class='btn btn-secondary' style='margin-top:0; flex:1; min-width:180px;'>Generate for Another Company</a>"
        "<a href='/resume' class='btn btn-secondary' style='margin-top:0; flex:1; min-width:180px;'>Score This Resume</a>"
        "</div>",

        company_name,
        skill_banner,

        name,
        contact_line,

        objective,
        branch,
        cgpa_s,
        skills_html,
        projects_html,

        experience_section,
        certs_section,
        achievements_section,

        company_name,
        tip
    );
    (void)w;

    serve_page(client, "resumegen", content);
    free(content);
}

/* ═══════════════════════════════════════════════════════════════
   FEATURE: ALUMNI / SENIOR REVIEWS  (/reviews)
   ═══════════════════════════════════════════════════════════════ */

/* In-memory review store — resets on server restart */
#define MAX_REVIEWS 50
#define MAX_REVIEW_LEN 400

typedef struct {
    char author[MAX_NAME];
    char company[MAX_NAME];
    char year[10];
    char role[80];
    char review[MAX_REVIEW_LEN];
    int  rating;   /* 1-5 */
} Review;

static Review review_store[MAX_REVIEWS];
static int    review_count = 0;

void init_reviews() {
    /* Seed with realistic alumni reviews */
    review_count = 0;

    strncpy(review_store[0].author,  "Aditya K.", MAX_NAME-1);
    strncpy(review_store[0].company, "Infosys",   MAX_NAME-1);
    strncpy(review_store[0].year,    "2023",       9);
    strncpy(review_store[0].role,    "Systems Engineer", 79);
    strncpy(review_store[0].review,  "The recruitment process was straightforward — aptitude, verbal, and one technical round. They value attitude over raw coding skills. Training in Mysuru is actually quite solid and you learn a lot about enterprise Java. Good for freshers who want structured learning.", MAX_REVIEW_LEN-1);
    review_store[0].rating = 4;

    strncpy(review_store[1].author,  "Sneha R.", MAX_NAME-1);
    strncpy(review_store[1].company, "TCS",      MAX_NAME-1);
    strncpy(review_store[1].year,    "2023",     9);
    strncpy(review_store[1].role,    "Assistant Systems Engineer", 79);
    strncpy(review_store[1].review,  "TCS NQT is the main filter — practice verbal ability, numerical, and programming logic. The interview is more of a formality if you clear NQT well. Work culture varies a lot by account/project. Digital and Ninja streams pay significantly more, so try to clear those rounds.", MAX_REVIEW_LEN-1);
    review_store[1].rating = 3;

    strncpy(review_store[2].author,  "Rohan M.", MAX_NAME-1);
    strncpy(review_store[2].company, "Amazon",   MAX_NAME-1);
    strncpy(review_store[2].year,    "2024",     9);
    strncpy(review_store[2].role,    "SDE-1",    79);
    strncpy(review_store[2].review,  "Four rounds total: one OA with two DSA problems (medium difficulty), then three virtual interviews covering DSA, system design basics, and LP (Leadership Principles). LP rounds are NOT HR fluff — they are evaluated seriously. Practice STAR format for every LP principle. Must have 150+ LeetCode mediums.", MAX_REVIEW_LEN-1);
    review_store[2].rating = 5;

    strncpy(review_store[3].author,  "Divya S.",     MAX_NAME-1);
    strncpy(review_store[3].company, "Goldman Sachs", MAX_NAME-1);
    strncpy(review_store[3].year,    "2024",          9);
    strncpy(review_store[3].role,    "Analyst — Engineering", 79);
    strncpy(review_store[3].review,  "GS has a brutal but fair process: coding test (Python preferred), then 3+ rounds of technical + behavioural. They ask about OS, DBMS, networking alongside DSA. Culture is intense but pay and learning are excellent. The engineering culture here is very different from a typical service company.", MAX_REVIEW_LEN-1);
    review_store[3].rating = 5;

    strncpy(review_store[4].author,  "Kiran B.",   MAX_NAME-1);
    strncpy(review_store[4].company, "Deloitte",   MAX_NAME-1);
    strncpy(review_store[4].year,    "2023",       9);
    strncpy(review_store[4].role,    "Analyst — UST", 79);
    strncpy(review_store[4].review,  "Deloitte looks for communication and case-cracking ability as much as tech. The group discussion round is key — speak clearly, let others talk, build on their points. Excel skills matter a lot in consulting tracks. Good work-life balance compared to product firms.", MAX_REVIEW_LEN-1);
    review_store[4].rating = 4;

    strncpy(review_store[5].author,  "Arjun P.", MAX_NAME-1);
    strncpy(review_store[5].company, "ISRO",     MAX_NAME-1);
    strncpy(review_store[5].year,    "2022",     9);
    strncpy(review_store[5].role,    "Scientist/Engineer SC", 79);
    strncpy(review_store[5].review,  "ISRO written exam covers core engineering subjects deeply — control systems, digital electronics, signal processing and programming in C. It is highly competitive. The job satisfaction is unmatched if you are passionate about space tech. Pay is Government scale but perks and job security are excellent.", MAX_REVIEW_LEN-1);
    review_store[5].rating = 5;

    review_count = 6;
}

void handle_get_reviews(SOCKET client) {
    char *content = malloc(32768);
    if (!content) { send_response(client, "500 Internal Server Error", "text/plain", "Out of memory."); return; }

    int w = snprintf(content, 32768,
        "<h1>Alumni & Senior Reviews</h1>"
        "<p class='subtitle'>Real experiences from seniors who have been placed — filter by company to make an informed choice</p>"

        "<h2>Submit Your Review</h2>"
        "<div class='review-form'>"
        "<form method='POST' action='/reviews'>"
        "<div class='grid'>"
        "<div class='form-group'><label>Your Name / Alias</label><input type='text' name='author' placeholder='e.g. Priya S.' required></div>"
        "<div class='form-group'><label>Company</label><select name='company'>"
    );

    for (int i = 0; i < NUM_COMPANIES && w < 30000; i++) {
        char opt[100];
        snprintf(opt, sizeof(opt), "<option value='%s'>%s</option>", companies[i].name, companies[i].name);
        strcat(content, opt); w += strlen(opt);
    }

    char form_rest[1024];
    int fr = snprintf(form_rest, sizeof(form_rest),
        "</select></div>"
        "<div class='form-group'><label>Placement Year</label>"
        "<select name='year'><option>2025</option><option>2024</option><option>2023</option><option>2022</option><option>2021</option></select></div>"
        "<div class='form-group'><label>Your Role / Designation</label><input type='text' name='role' placeholder='e.g. SDE-1, Analyst' required></div>"
        "<div class='form-group'><label>Rating (1-5)</label>"
        "<select name='rating'><option value='5'>★★★★★ Excellent</option><option value='4'>★★★★☆ Good</option><option value='3'>★★★☆☆ Average</option><option value='2'>★★☆☆☆ Below Avg</option><option value='1'>★☆☆☆☆ Poor</option></select></div>"
        "</div>"
        "<div class='form-group'><label>Your Review &amp; Advice for Juniors</label>"
        "<textarea name='review' placeholder='Share your interview experience, work culture, tips for juniors...' required></textarea></div>"
        "<button type='submit' class='btn'>Submit Review</button>"
        "</form>"
        "</div>"
        "<h2>Company Reviews from Seniors</h2>"
    );
    strcat(content, form_rest); w += fr;

    /* Group reviews by company */
    for (int c = 0; c < NUM_COMPANIES && w < 30000; c++) {
        int found = 0;
        for (int r = 0; r < review_count; r++)
            if (strcmp(review_store[r].company, companies[c].name) == 0) { found = 1; break; }
        if (!found) continue;

        char ch[100];
        snprintf(ch, sizeof(ch), "<h3 style='color:#06b6d4; margin: 24px 0 14px;'>%s</h3>", companies[c].name);
        strcat(content, ch); w += strlen(ch);

        for (int r = 0; r < review_count && w < 30000; r++) {
            if (strcmp(review_store[r].company, companies[c].name) != 0) continue;
            char stars[12] = ""; int s = review_store[r].rating;
            for (int j=0;j<s;j++) strncat(stars,"★",sizeof(stars)-strlen(stars)-1);
            for (int j=s;j<5;j++) strncat(stars,"☆",sizeof(stars)-strlen(stars)-1);

            char initials[3] = {review_store[r].author[0], 0, 0};

            char card[1024];
            snprintf(card, sizeof(card),
                "<div class='review-card'>"
                "<div class='rc-header'>"
                "<div class='rc-avatar'>%s</div>"
                "<div class='rc-meta'>"
                "<h4>%s <span class='rc-stars'>%s</span></h4>"
                "<span>%s &nbsp;|&nbsp; Placed %s</span>"
                "</div>"
                "</div>"
                "<div class='rc-body'>%s</div>"
                "<span class='rc-tag'>%s</span>"
                "<span class='rc-tag'>%s</span>"
                "</div>",
                initials,
                review_store[r].author, stars,
                review_store[r].role, review_store[r].year,
                review_store[r].review,
                companies[c].domain, companies[c].tier[0]=='A'?"Tier A":"Tier B/C"
            );
            strcat(content, card); w += strlen(card);
        }
    }

    char footer[] = "<div style='margin-top: 24px;'><a href='/' class='btn'>Back to Predictor</a></div>";
    if (w + (int)strlen(footer) < 32700) strcat(content, footer);

    serve_page(client, "reviews", content);
    free(content);
}

void handle_post_reviews(SOCKET client, const char* body) {
    if (review_count >= MAX_REVIEWS) {
        handle_get_reviews(client);
        return;
    }
    Review *rv = &review_store[review_count];
    char rating_s[10] = {0};

    extract_value(body, "author",  rv->author,  sizeof(rv->author));
    extract_value(body, "company", rv->company, sizeof(rv->company));
    extract_value(body, "year",    rv->year,    sizeof(rv->year));
    extract_value(body, "role",    rv->role,    sizeof(rv->role));
    extract_value(body, "review",  rv->review,  sizeof(rv->review));
    extract_value(body, "rating",  rating_s,    sizeof(rating_s));
    rv->rating = atoi(rating_s);
    if (rv->rating < 1) rv->rating = 1;
    if (rv->rating > 5) rv->rating = 5;

    /* Basic validation */
    if (rv->author[0] && rv->company[0] && rv->review[0]) {
        review_count++;
    }

    handle_get_reviews(client);
}

/* ═══════════════════════════════════════════════════════════════
   SERVER ENTRY POINT & ROUTE DISPATCHER
   ═══════════════════════════════════════════════════════════════ */

int main() {
    init_reviews();
    load_winsock();
    WSADATA wsa;
    SOCKET server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    printf("Initializing Winsock...\n");
    if (my_WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed. Error Code : %d\n", my_WSAGetLastError());
        return 1;
    }

    if ((server_fd = my_socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Socket creation failed. Error Code : %d\n", my_WSAGetLastError());
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = my_htons(PORT);

    if (my_bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        printf("Bind failed. Error Code : %d\n", my_WSAGetLastError());
        my_closesocket(server_fd);
        my_WSACleanup();
        return 1;
    }

    if (my_listen(server_fd, 5) == SOCKET_ERROR) {
        printf("Listen failed. Error Code : %d\n", my_WSAGetLastError());
        my_closesocket(server_fd);
        my_WSACleanup();
        return 1;
    }

    printf("====================================================\n");
    printf("   AI PLACEMENT ADVISOR - PREMIUM WEB SERVER\n");
    printf("   Serving on http://localhost:%d\n", PORT);
    printf("   Multi-Page Routing Active.\n");
    printf("====================================================\n");

    while (1) {
        new_socket = my_accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (new_socket == INVALID_SOCKET) {
            printf("Accept failed. Error Code : %d\n", my_WSAGetLastError());
            continue;
        }

        memset(buffer, 0, BUFFER_SIZE);
        int valread = my_recv(new_socket, buffer, BUFFER_SIZE, 0);
        if (valread > 0) {
            char method[16] = {0};
            char path[128] = {0};
            
            sscanf(buffer, "%15s %127s", method, path);

            if (strcmp(method, "GET") == 0) {
                if (strcmp(path, "/") == 0 || strcmp(path, "/index") == 0) {
                    handle_get_home(new_socket);
                } else if (strcmp(path, "/resume") == 0) {
                    handle_get_resume(new_socket);
                } else if (strcmp(path, "/compare") == 0) {
                    handle_get_compare(new_socket);
                } else if (strcmp(path, "/roadmap") == 0) {
                    handle_get_roadmap(new_socket);
                } else if (strcmp(path, "/interview") == 0) {
                    handle_get_interview(new_socket);
                } else if (strcmp(path, "/simulator") == 0) {
                    handle_get_simulator(new_socket);
                } else if (strcmp(path, "/backlogs") == 0) {
                    handle_get_backlogs(new_socket);
                } else if (strcmp(path, "/resume-gen") == 0) {
                    handle_get_resumegen(new_socket);
                } else if (strcmp(path, "/reviews") == 0) {
                    handle_get_reviews(new_socket);
                } else {
                    send_response(new_socket, "404 Not Found", "text/html", 
                                  "<h1>404 Not Found</h1><p>The requested page was not found on this server.</p><a href='/'>Go to Home</a>");
                }
            } else if (strcmp(method, "POST") == 0) {
                const char* body = strstr(buffer, "\r\n\r\n");
                if (body) {
                    body += 4;
                    if (strcmp(path, "/predict") == 0) {
                        handle_post_predict(new_socket, body);
                    } else if (strcmp(path, "/resume") == 0) {
                        handle_post_resume(new_socket, body);
                    } else if (strcmp(path, "/compare") == 0) {
                        handle_post_compare(new_socket, body);
                    } else if (strcmp(path, "/roadmap") == 0) {
                        handle_post_roadmap(new_socket, body);
                    } else if (strcmp(path, "/interview") == 0) {
                        handle_post_interview(new_socket, body);
                    } else if (strcmp(path, "/simulator") == 0) {
                        handle_post_simulator(new_socket, body);
                    } else if (strcmp(path, "/resume-gen") == 0) {
                        handle_post_resumegen(new_socket, body);
                    } else if (strcmp(path, "/reviews") == 0) {
                        handle_post_reviews(new_socket, body);
                    } else {
                        send_response(new_socket, "404 Not Found", "text/html", 
                                      "<h1>404 Not Found</h1><p>The requested endpoint was not found.</p><a href='/'>Go to Home</a>");
                    }
                } else {
                    send_response(new_socket, "400 Bad Request", "text/plain", "Missing request body.");
                }
            } else {
                send_response(new_socket, "405 Method Not Allowed", "text/plain", "Only GET and POST are supported.");
            }
        }
        my_closesocket(new_socket);
    }

    my_closesocket(server_fd);
    my_WSACleanup();
    return 0;
}