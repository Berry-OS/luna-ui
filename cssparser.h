/*
 * cssparser.h - Single-header CSS (+ HTML) Parser Library for C
 *
 * No flex/bison dependency. Hand-written recursive descent parser.
 * Supports CSS2.1 + CSS3 common features.
 *
 * USAGE:
 *   #define CSS_PARSER_IMPLEMENTATION
 *   #include "cssparser.h"
 *
 * Copyright © 2026 Yuichiro Nakada / Project Vespera
 * License: Mozilla Public License 2.0
 */

#ifndef CSSPARSER_H
#define CSSPARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdbool.h>

/* ── Limits ────────────────────────────────────────────────────────────
 *
 * A rule's selector list and declarations are heap arrays that grow as the
 * sheet asks (framework themes declare hundreds of custom properties in one
 * block, and selector lists run long); only the parts inside one selector
 * and the string sizes are fixed.  Long values such as data: URLs are
 * replaced by short keys before parsing (luna-ui), so CSS_MAX_VALUE bounds
 * ordinary values only.
 */
#define CSS_MAX_SELECTORS  4096   /* per rule: a sanity bound, not storage */
#define CSS_MAX_PARTS         6
#define CSS_MAX_DECLS      4096   /* per rule: a sanity bound, not storage */
#define CSS_MAX_RULES       512
#define CSS_MAX_ATRULES      64
#define CSS_MAX_STR          96
#define CSS_MAX_VALUE       1024

/* ── Enums ───────────────────────────────────────────────────────────── */

typedef enum {
    CSS_SEL_UNIVERSAL = 0,  /* *          */
    CSS_SEL_TYPE,           /* div        */
    CSS_SEL_CLASS,          /* .foo       */
    CSS_SEL_ID,             /* #bar       */
    CSS_SEL_ATTR,           /* [attr]     */
    CSS_SEL_ATTR_EQ,        /* [attr=val] */
    CSS_SEL_ATTR_INCLUDES,  /* [attr~=v]  */
    CSS_SEL_ATTR_DASH,      /* [attr|=v]  */
    CSS_SEL_ATTR_PREFIX,    /* [attr^=v]  */
    CSS_SEL_ATTR_SUFFIX,    /* [attr$=v]  */
    CSS_SEL_ATTR_SUBSTR,    /* [attr*=v]  */
    CSS_SEL_PSEUDO_CLASS,   /* :hover     */
    CSS_SEL_PSEUDO_ELEM,    /* ::before   */
} CSSSelectorPartType;

typedef enum {
    CSS_COMB_NONE = 0,   /* (start)          */
    CSS_COMB_DESC,       /* ' ' descendant   */
    CSS_COMB_CHILD,      /* >  child         */
    CSS_COMB_ADJACENT,   /* +  adjacent      */
    CSS_COMB_SIBLING,    /* ~  general sibling */
} CSSCombinator;

typedef enum {
    CSS_AT_NONE = 0,
    CSS_AT_CHARSET,
    CSS_AT_IMPORT,
    CSS_AT_MEDIA,
    CSS_AT_KEYFRAMES,
    CSS_AT_FONT_FACE,
    CSS_AT_SUPPORTS,
    CSS_AT_PAGE,
    CSS_AT_UNKNOWN,
} CSSAtRuleType;

/* ── Structures ──────────────────────────────────────────────────────── */

/* One simple selector part (e.g., div, .class, #id, [attr=val], :hover) */
/* Design systems declare hundreds of --tokens on :root (Yahoo's riff has ~200). */
#ifndef CSS_MAX_CUSTOM_PROPS
#define CSS_MAX_CUSTOM_PROPS 384
#endif

typedef struct {
    CSSSelectorPartType type;
    char name[CSS_MAX_STR];       /* tag / class / id / attr name / pseudo */
    char attr_op[4];              /* =, ~=, |=, ^=, $=, *= */
    char attr_val[CSS_MAX_STR];   /* attribute value */
    char pseudo_arg[CSS_MAX_STR]; /* :nth-child(2n+1) argument */
} CSSSelectorPart;

/* One compound selector: sequence of parts with a combinator prefix */
typedef struct {
    CSSCombinator combinator;            /* relation to previous compound */
    CSSSelectorPart parts[CSS_MAX_PARTS];
    int part_count;
    int specificity;                     /* (id<<16)|(cls<<8)|type */
} CSSCompound;

/* One full selector (chain of compounds) */
typedef struct {
    CSSCompound compounds[CSS_MAX_PARTS];
    int compound_count;
    int specificity;
} CSSSelector;

/* One CSS declaration: property + value + !important */
typedef struct {
    char property[CSS_MAX_STR];
    char value[CSS_MAX_VALUE];
    /* Preserved only when the declaration contains var().  The resolver may
     * rewrite value, while Luna's CSSOM uses this source to re-evaluate the
     * declaration after a custom-property update. */
    char *variable_source;
    bool important;
} CSSDeclaration;

/* One CSS rule: selector list + declaration block */
typedef struct {
    CSSSelector *selectors;   /* malloc'd; owned by the rule */
    int selector_count, selector_cap;
    CSSDeclaration *decls;    /* malloc'd; owned by the rule */
    int decl_count, decl_cap;
    int specificity; /* max of all selectors */
    /* The selector list as written, valid only while the sheet is being parsed
     * (it points into the source, or into text a nested rule owns): nesting
     * needs it to build the selectors of the rules inside this one. */
    const char *sel_src;
    size_t sel_len;
} CSSRule;

/* One @-rule */
typedef struct {
    CSSAtRuleType type;
    char prelude[CSS_MAX_VALUE];  /* e.g., media query, charset string */
    /* For @media / @keyframes: nested rules */
    CSSRule *nested_rules;
    int nested_rule_count;
    int nested_rule_cap;
} CSSAtRule;

/* Parsed stylesheet */
typedef struct {
    CSSRule  *rules;
    int       rule_count;
    int       rule_cap;
    CSSAtRule *at_rules;
    int        at_rule_count;
    int        at_rule_cap;
    char      *charset;
    /* error info */
    int        error_count;
    char       last_error[256];
    /* CSS custom properties from :root / any rule */
    struct { char name[64]; char value[1024]; } custom_props[CSS_MAX_CUSTOM_PROPS];
    int custom_prop_count;
} CSSStyleSheet;

/* ── Callback-based (SAX-style) API ──────────────────────────────────── */

typedef struct {
    void *user_data;
    void (*on_selector)(void *ud, const CSSSelector *sel, int sel_idx, int sel_count);
    void (*on_declaration)(void *ud, const CSSDeclaration *decl);
    void (*on_rule_start)(void *ud);
    void (*on_rule_end)(void *ud);
    void (*on_at_rule)(void *ud, const CSSAtRule *at);
    void (*on_error)(void *ud, const char *msg, int line);
} CSSCallbacks;

/* ── DOM-style API ───────────────────────────────────────────────────── */

CSSStyleSheet *css_parse(const char *text, size_t len);
CSSStyleSheet *css_parse_file(const char *path);
void           css_free(CSSStyleSheet *sheet);

/* Apply callbacks while parsing (streaming) */
void css_parse_cb(const char *text, size_t len, CSSCallbacks *cb);

/* HTML integration: extract <style> blocks + style="" attributes */
CSSStyleSheet *css_parse_html(const char *html, size_t len);

/* Lookup helpers */
const CSSDeclaration *css_find_decl(const CSSRule *rule, const char *property);

/* Debug */
void css_dump(const CSSStyleSheet *sheet, FILE *fp);

/* Resolve var(--name) references in all declarations using collected custom props */
void css_resolve_vars(CSSStyleSheet *sheet);

#endif /* CSSPARSER_H */

/* ═══════════════════════════════════════════════════════════════════════
 * IMPLEMENTATION
 * ═════════════════════════════════════════════════════════════════════ */
#ifdef CSS_PARSER_IMPLEMENTATION

/* strdup() is POSIX rather than ISO C.  Keep the single-header parser usable
 * with the documented `-std=c11` build on every supported host. */
static char *css_strdup(const char *src) {
    size_t len = strlen(src) + 1;
    char *copy = (char *)malloc(len);
    if (copy) memcpy(copy, src, len);
    return copy;
}

static int css_strncasecmp(const char *lhs, const char *rhs, size_t count) {
    while (count--) {
        unsigned char a = (unsigned char)*lhs++;
        unsigned char b = (unsigned char)*rhs++;
        int diff = tolower(a) - tolower(b);
        if (diff || !a || !b) return diff;
    }
    return 0;
}

/* ── Internal lexer ──────────────────────────────────────────────────── */

typedef struct {
    const char *src;
    size_t      len;
    size_t      pos;
    int         line;
} Lexer;

static void lex_init(Lexer *l, const char *src, size_t len) {
    l->src = src;
    l->len = len;
    l->pos = 0;
    l->line = 1;
}

static inline int lex_eof(const Lexer *l) { return l->pos >= l->len; }
static inline char lex_peek(const Lexer *l) {
    return l->pos < l->len ? l->src[l->pos] : '\0';
}
static inline char lex_peek2(const Lexer *l) {
    return (l->pos + 1) < l->len ? l->src[l->pos + 1] : '\0';
}
static inline char lex_advance(Lexer *l) {
    char c = l->src[l->pos++];
    if (c == '\n') l->line++;
    return c;
}

/* Skip whitespace and CSS comments */
static void lex_skip_ws(Lexer *l) {
    while (!lex_eof(l)) {
        char c = lex_peek(l);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f') {
            lex_advance(l);
        } else if (c == '/' && lex_peek2(l) == '*') {
            /* block comment */
            l->pos += 2;
            while (!lex_eof(l)) {
                if (lex_peek(l) == '*' && lex_peek2(l) == '/') {
                    l->pos += 2;
                    break;
                }
                if (lex_advance(l) == '\n') l->line++;
            }
        } else {
            break;
        }
    }
}

/* Read a CSS identifier into buf (max buflen-1 chars). Returns length. */
static int lex_ident(Lexer *l, char *buf, int buflen) {
    int n = 0;
    /* allow leading hyphen(s) for vendor prefixes and custom props */
    while (!lex_eof(l)) {
        char c = lex_peek(l);
        if (c == '\\' && (l->pos + 1) < l->len) {
            /* simple escape: skip backslash, take next char */
            lex_advance(l);
            c = lex_advance(l);
            if (n < buflen - 1) buf[n++] = c;
            continue;
        }
        if (isalnum((unsigned char)c) || c == '-' || c == '_' ||
            (unsigned char)c >= 0x80) {
            if (n < buflen - 1) buf[n++] = c;
            lex_advance(l);
        } else {
            break;
        }
    }
    buf[n] = '\0';
    return n;
}

/* Read a CSS string (quoted). Strips quotes. Returns length. */
static int lex_string(Lexer *l, char *buf, int buflen) {
    char quote = lex_advance(l); /* ' or " */
    int n = 0;
    while (!lex_eof(l)) {
        char c = lex_peek(l);
        if (c == quote) { lex_advance(l); break; }
        if (c == '\\') {
            lex_advance(l);
            if (!lex_eof(l)) {
                c = lex_advance(l);
                if (c == '\n') continue; /* escaped newline = ignore */
                if (n < buflen - 1) buf[n++] = c;
            }
            continue;
        }
        if (n < buflen - 1) buf[n++] = c;
        lex_advance(l);
    }
    buf[n] = '\0';
    return n;
}

/* Read until one of the stop chars (or EOF), respecting parens/quotes.
   Returns chars written (excluding NUL). Stops before the stop char. */
static int lex_until(Lexer *l, char *buf, int buflen, const char *stops) {
    int n = 0;
    int depth = 0;
    while (!lex_eof(l)) {
        char c = lex_peek(l);
        if (depth == 0 && strchr(stops, c)) break;
        if (c == '(') depth++;
        else if (c == ')') { if (depth > 0) depth--; else break; }
        else if (c == '"' || c == '\'') {
            /* read quoted string as a unit */
            char q = c;
            if (n < buflen - 1) buf[n++] = q;
            lex_advance(l);
            while (!lex_eof(l)) {
                char qc = lex_advance(l);
                if (n < buflen - 1) buf[n++] = qc;
                if (qc == q) break;
                if (qc == '\\' && !lex_eof(l)) {
                    qc = lex_advance(l);
                    if (n < buflen - 1) buf[n++] = qc;
                }
            }
            continue;
        } else if (c == '/' && lex_peek2(l) == '*') {
            /* skip comment inside value */
            l->pos += 2;
            while (!lex_eof(l)) {
                if (lex_peek(l) == '*' && lex_peek2(l) == '/') {
                    l->pos += 2; break;
                }
                if (lex_advance(l) == '\n') l->line++;
            }
            continue;
        }
        if (n < buflen - 1) buf[n++] = c;
        lex_advance(l);
    }
    buf[n] = '\0';
    return n;
}

/* ── String utilities ────────────────────────────────────────────────── */

static void str_trim(char *s) {
    if (!s || !*s) return;
    char *e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e)) *e-- = '\0';
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
}

static int css_starts_with_ci(const char *s, const char *prefix) {
    while (*prefix) {
        if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix)) return 0;
        s++; prefix++;
    }
    return 1;
}

/* ── Specificity ─────────────────────────────────────────────────────── */

static int calc_specificity(const CSSSelector *sel) {
    int a = 0, b = 0, c = 0;
    for (int i = 0; i < sel->compound_count; i++) {
        const CSSCompound *cmp = &sel->compounds[i];
        for (int j = 0; j < cmp->part_count; j++) {
            const CSSSelectorPart *p = &cmp->parts[j];
            switch (p->type) {
            case CSS_SEL_ID:            a++; break;
            case CSS_SEL_CLASS:
            case CSS_SEL_ATTR:
            case CSS_SEL_ATTR_EQ:
            case CSS_SEL_ATTR_INCLUDES:
            case CSS_SEL_ATTR_DASH:
            case CSS_SEL_ATTR_PREFIX:
            case CSS_SEL_ATTR_SUFFIX:
            case CSS_SEL_ATTR_SUBSTR:
            case CSS_SEL_PSEUDO_CLASS:  b++; break;
            case CSS_SEL_TYPE:          c++; break;
            default: break;
            }
        }
    }
    return (a << 16) | (b << 8) | c;
}

/* ── Selector parser ─────────────────────────────────────────────────── */

/* Forward declaration */
static void parse_rule_block(Lexer *l, CSSRule *rule, CSSCallbacks *cb);
static void skip_block(Lexer *l);

static void parse_selector_part(Lexer *l, CSSSelectorPart *part) {
    char c = lex_peek(l);

    if (c == '*') {
        lex_advance(l);
        part->type = CSS_SEL_UNIVERSAL;
        strcpy(part->name, "*");
    } else if (c == '.') {
        lex_advance(l);
        part->type = CSS_SEL_CLASS;
        lex_ident(l, part->name, CSS_MAX_STR);
    } else if (c == '#') {
        lex_advance(l);
        part->type = CSS_SEL_ID;
        lex_ident(l, part->name, CSS_MAX_STR);
    } else if (c == ':') {
        lex_advance(l);
        bool dbl = (lex_peek(l) == ':');
        if (dbl) lex_advance(l);
        part->type = dbl ? CSS_SEL_PSEUDO_ELEM : CSS_SEL_PSEUDO_CLASS;
        lex_ident(l, part->name, CSS_MAX_STR);
        if (lex_peek(l) == '(') {
            lex_advance(l);
            lex_until(l, part->pseudo_arg, CSS_MAX_STR, ")");
            if (lex_peek(l) == ')') lex_advance(l);
        }
    } else if (c == '[') {
        lex_advance(l);
        lex_skip_ws(l);
        char attr[CSS_MAX_STR] = {0};
        lex_ident(l, attr, CSS_MAX_STR);
        snprintf(part->name, CSS_MAX_STR, "%s", attr);
        lex_skip_ws(l);
        char nc = lex_peek(l);
        if (nc == ']') {
            lex_advance(l);
            part->type = CSS_SEL_ATTR;
        } else {
            /* parse operator */
            char op[4] = {0};
            if (nc != '=') {
                op[0] = nc; op[1] = '='; op[2] = '\0';
                lex_advance(l); lex_advance(l);
            } else {
                op[0] = '='; op[1] = '\0';
                lex_advance(l);
            }
            snprintf(part->attr_op, sizeof(part->attr_op), "%s", op);
            lex_skip_ws(l);
            if (lex_peek(l) == '"' || lex_peek(l) == '\'') {
                lex_string(l, part->attr_val, CSS_MAX_STR);
            } else {
                lex_ident(l, part->attr_val, CSS_MAX_STR);
            }
            lex_skip_ws(l);
            if (lex_peek(l) == ']') lex_advance(l);

            if      (strcmp(op, "=")  == 0) part->type = CSS_SEL_ATTR_EQ;
            else if (strcmp(op, "~=") == 0) part->type = CSS_SEL_ATTR_INCLUDES;
            else if (strcmp(op, "|=") == 0) part->type = CSS_SEL_ATTR_DASH;
            else if (strcmp(op, "^=") == 0) part->type = CSS_SEL_ATTR_PREFIX;
            else if (strcmp(op, "$=") == 0) part->type = CSS_SEL_ATTR_SUFFIX;
            else if (strcmp(op, "*=") == 0) part->type = CSS_SEL_ATTR_SUBSTR;
            else                            part->type = CSS_SEL_ATTR_EQ;
        }
    } else if (isalpha((unsigned char)c) || c == '_' ||
               (unsigned char)c >= 0x80 || c == '-') {
        part->type = CSS_SEL_TYPE;
        lex_ident(l, part->name, CSS_MAX_STR);
    }
}

/* Parse one complex selector (no commas). Returns 0 on empty. */
static int parse_one_selector(Lexer *l, CSSSelector *sel) {
    memset(sel, 0, sizeof(*sel));
    int ci = 0;

    while (!lex_eof(l)) {
        char c = lex_peek(l);
        /* stop at comma, open brace, or end */
        if (c == ',' || c == '{' || c == ')') break;

        CSSCompound *cmp = &sel->compounds[ci];
        cmp->combinator = (ci == 0) ? CSS_COMB_NONE : CSS_COMB_DESC;

        /* check combinator */
        if (ci > 0) {
            bool had_ws = false;
            /* skip whitespace, detect combinator chars */
            while (!lex_eof(l) && (lex_peek(l) == ' ' || lex_peek(l) == '\t' ||
                   lex_peek(l) == '\r' || lex_peek(l) == '\n')) {
                had_ws = true;
                lex_advance(l);
            }
            c = lex_peek(l);
            if (c == ',' || c == '{' || c == ')') break;
            if (c == '>') { cmp->combinator = CSS_COMB_CHILD;    lex_advance(l); lex_skip_ws(l); }
            else if (c == '+') { cmp->combinator = CSS_COMB_ADJACENT; lex_advance(l); lex_skip_ws(l); }
            else if (c == '~') { cmp->combinator = CSS_COMB_SIBLING;  lex_advance(l); lex_skip_ws(l); }
            else if (had_ws) { cmp->combinator = CSS_COMB_DESC; }
        }

        /* parse parts of this compound */
        int pi = 0;
        c = lex_peek(l);
        while (!lex_eof(l) && c != ',' && c != '{' && c != ')' &&
               c != ' ' && c != '\t' && c != '\n' && c != '\r' &&
               c != '>' && c != '+' && c != '~') {
            if (pi >= CSS_MAX_PARTS) break;
            parse_selector_part(l, &cmp->parts[pi++]);
            c = lex_peek(l);
        }
        cmp->part_count = pi;

        /* compute compound specificity */
        int sa = 0, sb = 0, sc = 0;
        for (int i = 0; i < pi; i++) {
            switch (cmp->parts[i].type) {
            case CSS_SEL_ID: sa++; break;
            case CSS_SEL_CLASS: case CSS_SEL_ATTR: case CSS_SEL_ATTR_EQ:
            case CSS_SEL_ATTR_INCLUDES: case CSS_SEL_ATTR_DASH:
            case CSS_SEL_ATTR_PREFIX: case CSS_SEL_ATTR_SUFFIX:
            case CSS_SEL_ATTR_SUBSTR: case CSS_SEL_PSEUDO_CLASS: sb++; break;
            case CSS_SEL_TYPE: sc++; break;
            default: break;
            }
        }
        cmp->specificity = (sa << 16) | (sb << 8) | sc;

        if (pi == 0) break;
        if (++ci >= CSS_MAX_PARTS) break;
    }

    sel->compound_count = ci;
    sel->specificity = calc_specificity(sel);
    return ci > 0 || (sel->compound_count == 1 && sel->compounds[0].part_count == 0) ? 1 : 0;
}

/* The next free selector / declaration of a rule, grown on demand (NULL
 * past the sanity bound or out of memory). */
static CSSSelector *rule_selector_slot(CSSRule *r) {
    if (r->selector_count >= CSS_MAX_SELECTORS) return NULL;
    if (r->selector_count == r->selector_cap) {
        int cap = r->selector_cap ? r->selector_cap * 2 : 2;
        CSSSelector *g = realloc(r->selectors, (size_t)cap * sizeof(*g));
        if (!g) return NULL;
        r->selectors = g;
        r->selector_cap = cap;
    }
    return &r->selectors[r->selector_count];
}

static CSSDeclaration *rule_decl_slot(CSSRule *r) {
    if (r->decl_count >= CSS_MAX_DECLS) return NULL;
    if (r->decl_count == r->decl_cap) {
        int cap = r->decl_cap ? r->decl_cap * 2 : 8;
        CSSDeclaration *g = realloc(r->decls, (size_t)cap * sizeof(*g));
        if (!g) return NULL;
        r->decls = g;
        r->decl_cap = cap;
    }
    return &r->decls[r->decl_count];
}

/* Frees what a rule owns (not the rule itself). */
static void css_rule_release(CSSRule *r) {
    for (int d = 0; d < r->decl_count; d++) free(r->decls[d].variable_source);
    free(r->decls);
    free(r->selectors);
    r->decls = NULL;
    r->selectors = NULL;
    r->decl_count = r->decl_cap = r->selector_count = r->selector_cap = 0;
}

/* ── Selector text rewriting ─────────────────────────────────────────────
 * :is() / :where() / :matches() / :-webkit-any() lists, :not(a, b) and the `&`
 * of nested rules are resolved on the selector text, before the ordinary
 * selector parser sees it: one rule becomes several plain selectors. */

typedef struct { char *p; size_t n, cap; } CssSelBuf;

static void css_selbuf_put(CssSelBuf *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 128;
        while (cap < b->n + n + 1) cap *= 2;
        char *g = realloc(b->p, cap);
        if (!g) return;
        b->p = g; b->cap = cap;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

/* Index of the char that ends the nest opened at s[i] ('(' or '['), or n. */
static size_t css_sel_match(const char *s, size_t n, size_t i) {
    int depth = 0;
    for (; i < n; i++) {
        char c = s[i];
        if (c == '"' || c == '\'') { char q = c; for (i++; i < n && s[i] != q; i++) if (s[i] == '\\') i++; }
        else if (c == '(' || c == '[') depth++;
        else if ((c == ')' || c == ']') && --depth == 0) return i;
    }
    return n;
}

/* Splits s[0..n) at top-level commas: calls fn(start, len, user) for each. */
static void css_sel_split(const char *s, size_t n, void (*fn)(const char *, size_t, void *), void *user) {
    size_t start = 0;
    for (size_t i = 0; i <= n; i++) {
        if (i < n && (s[i] == '(' || s[i] == '[')) { i = css_sel_match(s, n, i); continue; }
        if (i < n && (s[i] == '"' || s[i] == '\'')) { char q = s[i]; for (i++; i < n && s[i] != q; i++) if (s[i] == '\\') i++; continue; }
        if (i == n || s[i] == ',') {
            size_t a = start, b = i;
            while (a < b && isspace((unsigned char)s[a])) a++;
            while (b > a && isspace((unsigned char)s[b - 1])) b--;
            if (b > a) fn(s + a, b - a, user);
            start = i + 1;
        }
    }
}

static int css_sel_is_ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_' || c == '*' || (unsigned char)c >= 0x80;
}

/* Start of the last compound of s[0..n): after the last top-level space or combinator. */
static size_t css_sel_last_compound(const char *s, size_t n) {
    size_t cs = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '(' || s[i] == '[') { i = css_sel_match(s, n, i); continue; }
        if (isspace((unsigned char)s[i]) || s[i] == '>' || s[i] == '+' || s[i] == '~') cs = i + 1;
    }
    return cs;
}

/* The leading type selector (ident or *) of a compound: its length, 0 if none. */
static size_t css_sel_type_len(const char *s, size_t n) {
    if (!n || !css_sel_is_ident_start(s[0])) return 0;
    if (s[0] == '*') return 1;
    size_t i = 0;
    while (i < n && (isalnum((unsigned char)s[i]) || s[i] == '-' || s[i] == '_' || (unsigned char)s[i] >= 0x80)) i++;
    return i;
}

static int css_sel_starts_ci(const char *s, const char *lit) { return css_strncasecmp(s, lit, strlen(lit)) == 0; }

/* Length of the ":is(" style prefix at s, 0 if there is none. */
static size_t css_sel_group_pseudo(const char *s) {
    static const char *names[] = { ":is(", ":where(", ":matches(", ":-webkit-any(", ":-moz-any(", NULL };
    for (int i = 0; names[i]; i++) if (css_sel_starts_ci(s, names[i])) return strlen(names[i]);
    return 0;
}

typedef struct { const char *prefix; size_t plen; const char *suffix; size_t slen; CssSelBuf *out; int depth; } CssSelAlt;
static void css_sel_expand_one(const char *s, size_t n, CssSelBuf *out, int depth);

static void css_sel_alt_cb(const char *alt, size_t an, void *user) {
    CssSelAlt *a = (CssSelAlt *)user;
    size_t cs = css_sel_last_compound(a->prefix, a->plen);
    const char *pc = a->prefix + cs;
    size_t pcn = a->plen - cs;
    /* the alternative's own last compound is what merges with the prefix's */
    size_t ac = css_sel_last_compound(alt, an);
    size_t tp = css_sel_type_len(pc, pcn), ta = css_sel_type_len(alt + ac, an - ac);
    CssSelBuf r = {0};
    css_selbuf_put(&r, a->prefix, cs);                 /* everything before the compound */
    css_selbuf_put(&r, alt, ac);                       /* the alternative's ancestors */
    if (tp && ta) {
        if (pc[0] == '*') { css_selbuf_put(&r, alt + ac, ta); }
        else if (alt[ac] == '*' || (tp == ta && !css_strncasecmp(pc, alt + ac, tp))) { css_selbuf_put(&r, pc, tp); }
        else { free(r.p); return; }                // div:is(p) can match nothing
    } else if (tp) css_selbuf_put(&r, pc, tp);
    else if (ta) css_selbuf_put(&r, alt + ac, ta);
    css_selbuf_put(&r, pc + tp, pcn - tp);
    css_selbuf_put(&r, alt + ac + ta, an - ac - ta);
    css_selbuf_put(&r, a->suffix, a->slen);
    if (r.p) css_sel_expand_one(r.p, r.n, a->out, a->depth + 1);
    free(r.p);
}

static void css_sel_not_cb(const char *alt, size_t an, void *user) {
    CssSelBuf *b = (CssSelBuf *)user;
    css_selbuf_put(b, ":not(", 5); css_selbuf_put(b, alt, an); css_selbuf_put(b, ")", 1);
}

/* One complex selector -> its plain forms, appended to out (comma separated). */
static void css_sel_expand_one(const char *s, size_t n, CssSelBuf *out, int depth) {
    for (size_t i = 0; i < n && depth < 8; i++) {
        if (s[i] == '[') { i = css_sel_match(s, n, i); continue; }
        if (s[i] == '"' || s[i] == '\'') { char q = s[i]; for (i++; i < n && s[i] != q; i++) if (s[i] == '\\') i++; continue; }
        if (s[i] != ':') continue;
        size_t gl = css_sel_group_pseudo(s + i);
        if (gl) {
            size_t close = css_sel_match(s, n, i + gl - 1);
            if (close >= n) break;
            CssSelAlt a = { s, i, s + close + 1, n - close - 1, out, depth };
            css_sel_split(s + i + gl, close - i - gl, css_sel_alt_cb, &a);
            return;
        }
        if (css_sel_starts_ci(s + i, ":not(")) {
            size_t close = css_sel_match(s, n, i + 4);
            if (close >= n) break;
            const char *arg = s + i + 5; size_t an = close - i - 5;
            int has_comma = 0;
            for (size_t k = 0; k < an; k++) { if (arg[k] == '(' || arg[k] == '[') { k = css_sel_match(arg, an, k); continue; } if (arg[k] == ',') has_comma = 1; }
            if (has_comma) {
                CssSelBuf r = {0};
                css_selbuf_put(&r, s, i);
                css_sel_split(arg, an, css_sel_not_cb, &r);
                css_selbuf_put(&r, s + close + 1, n - close - 1);
                css_sel_expand_one(r.p, r.n, out, depth + 1);
                free(r.p);
                return;
            }
            i = close;
        }
    }
    if (out->n) css_selbuf_put(out, ", ", 2);
    css_selbuf_put(out, s, n);
}

static void css_sel_expand_cb(const char *sel, size_t n, void *user) { css_sel_expand_one(sel, n, (CssSelBuf *)user, 0); }

/* Whether a selector list needs the rewrite at all. */
static int css_sel_needs_expansion(const char *s, size_t n) {
    for (size_t i = 0; i + 4 < n; i++) {
        if (s[i] != ':') continue;
        if (css_sel_group_pseudo(s + i)) return 1;
        if (css_sel_starts_ci(s + i, ":not(")) {
            size_t close = css_sel_match(s, n, i + 4);
            for (size_t k = i + 5; k < close && k < n; k++) if (s[k] == ',') return 1;
        }
    }
    return 0;
}

/* The selector list s[0..n) with every group rewritten; malloc'd. */
static char *css_sel_expand_list(const char *s, size_t n) {
    CssSelBuf out = {0};
    css_sel_split(s, n, css_sel_expand_cb, &out);
    if (!out.p) { out.p = calloc(1, 1); }
    return out.p;
}

/* The selectors of a nested rule: for every child selector and parent
 * selector, `&` becomes the parent (as :is(parent)); without `&` the child is
 * a descendant of it. */
typedef struct { const char *child; size_t cn; CssSelBuf *out; } CssSelNest;
static void css_sel_nest_parent_cb(const char *par, size_t pn, void *user) {
    CssSelNest *nst = (CssSelNest *)user;
    CssSelBuf r = {0};
    int amp = 0;
    for (size_t i = 0; i < nst->cn; i++) {
        if (nst->child[i] == '&') {
            amp = 1;
            css_selbuf_put(&r, ":is(", 4); css_selbuf_put(&r, par, pn); css_selbuf_put(&r, ")", 1);
        } else css_selbuf_put(&r, nst->child + i, 1);
    }
    if (!amp) {
        free(r.p); r.p = NULL; r.n = r.cap = 0;
        css_selbuf_put(&r, par, pn); css_selbuf_put(&r, " ", 1); css_selbuf_put(&r, nst->child, nst->cn);
    }
    if (nst->out->n) css_selbuf_put(nst->out, ", ", 2);
    css_selbuf_put(nst->out, r.p, r.n);
    free(r.p);
}
typedef struct { const char *parent; size_t pn; CssSelBuf *out; } CssSelNestC;
static void css_sel_nest_child_cb(const char *ch, size_t cn, void *user) {
    CssSelNestC *c = (CssSelNestC *)user;
    CssSelNest nst = { ch, cn, c->out };
    css_sel_split(c->parent, c->pn, css_sel_nest_parent_cb, &nst);
}

/* Parse selector list (comma-separated) into the rule. Returns count. */
static int parse_selector_list_text(const char *text, size_t n, CSSRule *rule) {
    Lexer sub;
    lex_init(&sub, text, n);
    while (!lex_eof(&sub)) {
        lex_skip_ws(&sub);
        if (lex_eof(&sub)) break;
        CSSSelector *slot = rule_selector_slot(rule);
        if (!slot) break;
        size_t before = sub.pos;
        if (parse_one_selector(&sub, slot)) rule->selector_count++;
        lex_skip_ws(&sub);
        if (lex_peek(&sub) == ',') lex_advance(&sub);
        else if (sub.pos == before) lex_advance(&sub);     /* always progress */
    }
    return rule->selector_count;
}

static int parse_selector_list(Lexer *l, CSSRule *rule) {
    lex_skip_ws(l);
    size_t start = l->pos;
    const char *brace = memchr(l->src + start, '{', l->len - start);
    size_t end = brace ? (size_t)(brace - l->src) : l->len;
    if (!rule->sel_src) { rule->sel_src = l->src + start; rule->sel_len = end - start; }
    if (css_sel_needs_expansion(l->src + start, end - start)) {
        char *ex = css_sel_expand_list(l->src + start, end - start);
        parse_selector_list_text(ex, strlen(ex), rule);
        free(ex);
        l->pos = end;
        return rule->selector_count;
    }
    while (!lex_eof(l) && lex_peek(l) != '{') {
        lex_skip_ws(l);
        if (lex_peek(l) == '{') break;
        CSSSelector *slot = rule_selector_slot(rule);
        if (!slot) {
            /* skip remaining selectors */
            while (!lex_eof(l) && lex_peek(l) != '{') lex_advance(l);
            break;
        }
        size_t before = l->pos;
        if (parse_one_selector(l, slot)) rule->selector_count++;
        lex_skip_ws(l);
        if (lex_peek(l) == ',') lex_advance(l);
        else if (l->pos == before && !lex_eof(l) && lex_peek(l) != '{') lex_advance(l);
    }
    return rule->selector_count;
}

/* ── Declaration parser ──────────────────────────────────────────────── */

static void parse_declaration(Lexer *l, CSSDeclaration *decl) {
    memset(decl, 0, sizeof(*decl));
    lex_skip_ws(l);
    lex_ident(l, decl->property, CSS_MAX_STR);
    /* handle custom properties --foo */
    if (decl->property[0] == '\0') {
        /* try to pick up -- prefix */
        if (lex_peek(l) == '-' && lex_peek2(l) == '-') {
            int n = 0;
            while (!lex_eof(l) && lex_peek(l) != ':' && n < CSS_MAX_STR - 1)
                decl->property[n++] = lex_advance(l);
            decl->property[n] = '\0';
        }
    }
    lex_skip_ws(l);
    if (lex_peek(l) != ':') { decl->property[0] = '\0'; return; }
    lex_advance(l); /* consume ':' */
    lex_skip_ws(l);

    /* read value until ';' or '}' */
    char raw[CSS_MAX_VALUE] = {0};
    lex_until(l, raw, CSS_MAX_VALUE, ";}");
    str_trim(raw);

    /* detect !important */
    char *imp = strstr(raw, "!important");
    if (!imp) imp = strstr(raw, "! important");
    if (imp) {
        decl->important = true;
        *imp = '\0';
        str_trim(raw);
    }
    snprintf(decl->value, CSS_MAX_VALUE, "%s", raw);
    str_trim(decl->value);
    if (strstr(decl->value, "var(")) decl->variable_source = css_strdup(decl->value);
}

/* Rules nested in a rule's block (`.a { &:hover {..} .b {..} }`) are collected
 * here while the block is parsed and added after their parent, in order. */
typedef struct { CSSRule rule; char *text; } NestItem;
typedef struct { NestItem *items; int n, cap; } NestList;
static NestList *g_nest_list;

static void nest_release(NestList *nl) {
    for (int i = 0; i < nl->n; i++) { css_rule_release(&nl->items[i].rule); free(nl->items[i].text); }
    free(nl->items);
    nl->items = NULL; nl->n = nl->cap = 0;
}

/* Whether the item at the lexer starts a nested rule: a '{' comes before any
 * ';' or '}' at the top level. */
static int looks_nested(const Lexer *l) {
    if (l->src[l->pos] == '-' && l->pos + 1 < l->len && l->src[l->pos + 1] == '-') return 0;
    for (size_t i = l->pos; i < l->len; i++) {
        char c = l->src[i];
        if (c == '(' || c == '[') { i = css_sel_match(l->src, l->len, i); continue; }
        if (c == '"' || c == '\'') { char q = c; for (i++; i < l->len && l->src[i] != q; i++) if (l->src[i] == '\\') i++; continue; }
        if (c == '{') return 1;
        if (c == ';' || c == '}') return 0;
    }
    return 0;
}

static void parse_nested_rule(Lexer *l, CSSRule *parent, CSSCallbacks *cb) {
    size_t start = l->pos;
    while (!lex_eof(l) && lex_peek(l) != '{') lex_advance(l);
    size_t end = l->pos;
    if (l->src[start] == '@' || !parent->sel_src) { skip_block(l); return; }
    CssSelBuf out = {0};
    CssSelNestC c = { parent->sel_src, parent->sel_len, &out };
    css_sel_split(l->src + start, end - start, css_sel_nest_child_cb, &c);
    if (!out.p) { skip_block(l); return; }
    NestList *nl = g_nest_list;
    CSSRule child;
    memset(&child, 0, sizeof child);
    child.sel_src = out.p;
    child.sel_len = out.n;
    char *ex = css_sel_expand_list(out.p, out.n);
    parse_selector_list_text(ex, strlen(ex), &child);
    free(ex);
    parse_rule_block(l, &child, cb);
    for (int i = 0; i < child.selector_count; i++)
        if (child.selectors[i].specificity > child.specificity) child.specificity = child.selectors[i].specificity;
    if (nl->n == nl->cap) {
        int cap = nl->cap ? nl->cap * 2 : 4;
        NestItem *g = realloc(nl->items, (size_t)cap * sizeof *g);
        if (!g) { css_rule_release(&child); free(out.p); return; }
        nl->items = g; nl->cap = cap;
    }
    nl->items[nl->n].rule = child;
    nl->items[nl->n].text = out.p;
    nl->n++;
}

static void parse_rule_block(Lexer *l, CSSRule *rule, CSSCallbacks *cb) {
    if (lex_peek(l) != '{') return;
    lex_advance(l); /* consume '{' */

    while (!lex_eof(l)) {
        lex_skip_ws(l);
        char c = lex_peek(l);
        if (c == '}') { lex_advance(l); break; }
        if (c == ';') { lex_advance(l); continue; }
        if (c == '\0') break;

        if (g_nest_list && looks_nested(l)) { parse_nested_rule(l, rule, cb); continue; }

        CSSDeclaration *d = rule_decl_slot(rule);
        if (d) {
            size_t before = l->pos;
            parse_declaration(l, d);
            if (d->property[0]) {
                if (cb && cb->on_declaration) cb->on_declaration(cb->user_data, d);
                rule->decl_count++;
            } else {
                /* Not a declaration (a nested rule, stray token): drop it up to
                 * the next ';' or past its {block}, always making progress. */
                l->pos = before;
                while (!lex_eof(l) && lex_peek(l) != ';' && lex_peek(l) != '}' && lex_peek(l) != '{')
                    lex_advance(l);
                if (lex_peek(l) == '{') skip_block(l);
                else if (l->pos == before && !lex_eof(l) && lex_peek(l) != '}') lex_advance(l);
            }
        } else {
            /* skip remaining */
            lex_until(l, (char[CSS_MAX_VALUE]){0}, CSS_MAX_VALUE, "}");
            break;
        }
        lex_skip_ws(l);
        if (lex_peek(l) == ';') lex_advance(l);
    }
}

/* ── @rule parser ────────────────────────────────────────────────────── */

static CSSAtRuleType classify_at(const char *kw) {
    if (css_starts_with_ci(kw, "charset"))    return CSS_AT_CHARSET;
    if (css_starts_with_ci(kw, "import"))     return CSS_AT_IMPORT;
    if (css_starts_with_ci(kw, "media"))      return CSS_AT_MEDIA;
    if (css_starts_with_ci(kw, "keyframes") ||
        css_starts_with_ci(kw, "-webkit-keyframes") ||
        css_starts_with_ci(kw, "-moz-keyframes")) return CSS_AT_KEYFRAMES;
    if (css_starts_with_ci(kw, "font-face")) return CSS_AT_FONT_FACE;
    if (css_starts_with_ci(kw, "supports"))  return CSS_AT_SUPPORTS;
    if (css_starts_with_ci(kw, "page"))       return CSS_AT_PAGE;
    return CSS_AT_UNKNOWN;
}

/* Skip a {...} block including nested braces */
static void skip_block(Lexer *l) {
    if (lex_peek(l) != '{') return;
    lex_advance(l);
    int depth = 1;
    while (!lex_eof(l) && depth > 0) {
        char c = lex_advance(l);
        if (c == '{') depth++;
        else if (c == '}') depth--;
        else if (c == '"' || c == '\'') {
            /* skip string */
            char q = c;
            while (!lex_eof(l)) {
                c = lex_advance(l);
                if (c == q) break;
                if (c == '\\' && !lex_eof(l)) lex_advance(l);
            }
        }
    }
}

/* ── Main stylesheet parser ──────────────────────────────────────────── */

static int nested_rules_push(CSSAtRule *at, const CSSRule *rule) {
    if (at->nested_rule_count >= at->nested_rule_cap) {
        int next = at->nested_rule_cap ? at->nested_rule_cap + 4 : 4;
        CSSRule *grown = realloc(at->nested_rules, (size_t)next * sizeof(CSSRule));
        if (!grown) return 0;
        at->nested_rules = grown;
        at->nested_rule_cap = next;
    }
    at->nested_rules[at->nested_rule_count++] = *rule;
    return 1;
}

static void sheet_add_rule(CSSStyleSheet *sheet, const CSSRule *rule) {
    if (sheet->rule_count >= sheet->rule_cap) {
        /* Grow in modest steps.  Doubling from 256→512 with a large CSSRule
         * briefly needs both buffers resident; +64 keeps the peak closer to
         * the final sheet size. */
        int next = sheet->rule_cap ? sheet->rule_cap + 64 : 32;
        if (next < sheet->rule_count + 1) next = sheet->rule_count + 1;
        CSSRule *grown = realloc(sheet->rules, (size_t)next * sizeof(CSSRule));
        if (!grown) { css_rule_release((CSSRule *)rule); return; }
        sheet->rules = grown;
        sheet->rule_cap = next;
    }
    sheet->rules[sheet->rule_count++] = *rule;
}

static void sheet_add_at(CSSStyleSheet *sheet, const CSSAtRule *at) {
    if (sheet->at_rule_count >= sheet->at_rule_cap) {
        sheet->at_rule_cap = sheet->at_rule_cap ? sheet->at_rule_cap * 2 : 16;
        sheet->at_rules = realloc(sheet->at_rules, sheet->at_rule_cap * sizeof(CSSAtRule));
    }
    sheet->at_rules[sheet->at_rule_count++] = *at;
}

static void parse_stylesheet_inner(Lexer *l, CSSStyleSheet *sheet, CSSCallbacks *cb, int in_block);

/* @media / @supports { rules }.  Nested conditional groups are flattened: the
 * inner group is emitted as its own at-rule whose prelude carries the outer
 * condition (media conditions are ANDed; @supports is assumed true). */
static void parse_cond_group(Lexer *l, CSSStyleSheet *sheet, CSSCallbacks *cb,
                             CSSAtRuleType type, const char *outer)
{
    CSSAtRule at = {0};
    at.type = type;
    char tmp[CSS_MAX_VALUE] = {0};
    lex_until(l, tmp, CSS_MAX_VALUE, "{");
    str_trim(tmp);
    if (outer && *outer) {
        at.type = CSS_AT_MEDIA;      /* @supports is assumed true: only media conditions survive */
        if (type == CSS_AT_MEDIA) snprintf(at.prelude, CSS_MAX_VALUE, "%s and %s", outer, tmp);
        else snprintf(at.prelude, CSS_MAX_VALUE, "%s", outer);
    } else {
        snprintf(at.prelude, CSS_MAX_VALUE, "%s", tmp);
    }
    lex_skip_ws(l);
    if (lex_peek(l) == '{') {
        lex_advance(l);
        while (!lex_eof(l)) {
            lex_skip_ws(l);
            char c = lex_peek(l);
            if (c == '}') { lex_advance(l); break; }
            if (c == '@') {
                lex_advance(l);
                char kw[128] = {0};
                lex_ident(l, kw, 128);
                lex_skip_ws(l);
                CSSAtRuleType t = classify_at(kw);
                if (t == CSS_AT_MEDIA || t == CSS_AT_SUPPORTS) {
                    parse_cond_group(l, sheet, cb, t, at.type == CSS_AT_MEDIA ? at.prelude : NULL);
                } else {
                    char skip[CSS_MAX_VALUE] = {0};
                    lex_until(l, skip, CSS_MAX_VALUE, ";{");
                    if (lex_peek(l) == ';') lex_advance(l);
                    else skip_block(l);
                }
                continue;
            }
            CSSRule *inner = calloc(1, sizeof(*inner));
            if (!inner) return;
            parse_selector_list(l, inner);
            lex_skip_ws(l);
            if (lex_peek(l) == '{') {
                NestList nest = {0};
                NestList *saved_nest = g_nest_list;
                g_nest_list = &nest;
                parse_rule_block(l, inner, NULL);
                g_nest_list = saved_nest;
                if (!nested_rules_push(&at, inner)) { css_rule_release(inner); free(inner); nest_release(&nest); return; }
                for (int ni = 0; ni < nest.n; ni++) {
                    CSSRule *nr = &nest.items[ni].rule;
                    if (nr->selector_count > 0 && nr->decl_count > 0 && nested_rules_push(&at, nr)) { }
                    else css_rule_release(nr);
                    free(nest.items[ni].text);
                }
                free(nest.items);
            } else {
                size_t before = l->pos;
                while (!lex_eof(l) && lex_peek(l) != '}' && lex_peek(l) != '{' && lex_peek(l) != ';')
                    lex_advance(l);
                if (lex_peek(l) == ';') lex_advance(l);
                if (l->pos == before && !lex_eof(l) && lex_peek(l) != '}') lex_advance(l);
                css_rule_release(inner);
            }
            free(inner);
        }
    }
    if (cb && cb->on_at_rule) cb->on_at_rule(cb->user_data, &at);
    sheet_add_at(sheet, &at);
}

static void parse_stylesheet_inner(Lexer *l, CSSStyleSheet *sheet, CSSCallbacks *cb, int in_block) {
    while (!lex_eof(l)) {
        lex_skip_ws(l);
        if (lex_eof(l)) break;

        char c = lex_peek(l);

        /* a '}' closes an enclosing @layer-style group; elsewhere it is stray */
        if (c == '}') { lex_advance(l); if (in_block) return; continue; }

        /* CDO/CDC (HTML comment markers in CSS) */
        if (c == '<' && (l->pos + 3 < l->len) &&
            memcmp(l->src + l->pos, "<!--", 4) == 0) { l->pos += 4; continue; }
        if (c == '-' && (l->pos + 2 < l->len) &&
            memcmp(l->src + l->pos, "-->", 3) == 0)  { l->pos += 3; continue; }

        if (c == '@') {
            lex_advance(l); /* consume '@' */
            char kw[128] = {0};
            lex_ident(l, kw, 128);
            lex_skip_ws(l);

            CSSAtRule at = {0};
            at.type = classify_at(kw);

            if (at.type == CSS_AT_CHARSET) {
                if (lex_peek(l) == '"' || lex_peek(l) == '\'') {
                    lex_string(l, at.prelude, CSS_MAX_VALUE);
                    if (!sheet->charset) sheet->charset = css_strdup(at.prelude);
                }
                lex_skip_ws(l);
                if (lex_peek(l) == ';') lex_advance(l);
                if (cb && cb->on_at_rule) cb->on_at_rule(cb->user_data, &at);
                sheet_add_at(sheet, &at);
                continue;
            }

            if (at.type == CSS_AT_IMPORT) {
                char tmp[CSS_MAX_VALUE] = {0};
                lex_until(l, tmp, CSS_MAX_VALUE, ";{");
                str_trim(tmp);
                snprintf(at.prelude, CSS_MAX_VALUE, "%s", tmp);
                if (lex_peek(l) == ';') lex_advance(l);
                if (cb && cb->on_at_rule) cb->on_at_rule(cb->user_data, &at);
                sheet_add_at(sheet, &at);
                continue;
            }

            if (at.type == CSS_AT_MEDIA || at.type == CSS_AT_SUPPORTS) {
                parse_cond_group(l, sheet, cb, at.type, NULL);
                continue;
            }

            /* @layer / @container wrap ordinary rules: parse the body in place. */
            if (css_starts_with_ci(kw, "layer") || css_starts_with_ci(kw, "container") ||
                css_starts_with_ci(kw, "scope")) {
                char tmp[CSS_MAX_VALUE] = {0};
                lex_until(l, tmp, CSS_MAX_VALUE, ";{");
                if (lex_peek(l) == ';') { lex_advance(l); continue; }
                if (lex_peek(l) == '{') {
                    lex_advance(l);
                    parse_stylesheet_inner(l, sheet, cb, 1);   /* stops at the matching '}' */
                }
                continue;
            }

            if (at.type == CSS_AT_KEYFRAMES) {
                char tmp[CSS_MAX_VALUE] = {0};
                lex_until(l, tmp, CSS_MAX_VALUE, "{");
                str_trim(tmp);
                snprintf(at.prelude, CSS_MAX_VALUE, "%s", tmp);
                lex_skip_ws(l);
                /* Parse keyframe blocks as nested rules with stop selectors */
                if (lex_peek(l) == '{') {
                    lex_advance(l);
                    at.nested_rule_cap = 0;
                    at.nested_rules = NULL;
                    at.nested_rule_count = 0;
                    while (!lex_eof(l)) {
                        lex_skip_ws(l);
                        if (lex_peek(l) == '}') { lex_advance(l); break; }
                        CSSRule *kf = calloc(1, sizeof(*kf));
                        if (!kf) return;
                        /* keyframe selector: from / to / percentage */
                        char stop_sel[CSS_MAX_STR] = {0};
                        lex_until(l, stop_sel, CSS_MAX_STR, "{");
                        str_trim(stop_sel);
                        /* A block may target several stops: `0%, 100% {...}`. */
                        char *stop = stop_sel;
                        while (*stop) {
                            char *comma = strchr(stop, ',');
                            if (comma) *comma = '\0';
                            str_trim(stop);
                            CSSSelector *sel = *stop ? rule_selector_slot(kf) : NULL;
                            if (sel) {
                                memset(sel, 0, sizeof(*sel));
                                kf->selector_count++;
                                snprintf(sel->compounds[0].parts[0].name, CSS_MAX_STR, "%s", stop);
                                sel->compounds[0].parts[0].type = CSS_SEL_TYPE;
                                sel->compounds[0].part_count = 1;
                                sel->compound_count = 1;
                            }
                            if (!comma) break;
                            stop = comma + 1;
                        }
                        lex_skip_ws(l);
                        if (lex_peek(l) == '{') parse_rule_block(l, kf, NULL);
                        if (!nested_rules_push(&at, kf)) { css_rule_release(kf); free(kf); return; }
                        free(kf);
                    }
                }
                if (cb && cb->on_at_rule) cb->on_at_rule(cb->user_data, &at);
                sheet_add_at(sheet, &at);
                continue;
            }

            if (at.type == CSS_AT_FONT_FACE || at.type == CSS_AT_PAGE) {
                char tmp[CSS_MAX_VALUE] = {0};
                lex_until(l, tmp, CSS_MAX_VALUE, "{");
                str_trim(tmp);
                snprintf(at.prelude, CSS_MAX_VALUE, "%s", tmp);
                lex_skip_ws(l);
                CSSRule *inner = calloc(1, sizeof(*inner));
                if (!inner) return;
                if (lex_peek(l) == '{') parse_rule_block(l, inner, NULL);
                at.nested_rule_cap = 1;
                at.nested_rules = malloc(sizeof(CSSRule));
                at.nested_rules[0] = *inner;
                free(inner);
                at.nested_rule_count = 1;
                if (cb && cb->on_at_rule) cb->on_at_rule(cb->user_data, &at);
                sheet_add_at(sheet, &at);
                continue;
            }

            /* Unknown @rule: skip to ';' or end of block */
            char tmp[CSS_MAX_VALUE] = {0};
            lex_until(l, tmp, CSS_MAX_VALUE, ";{");
            str_trim(tmp); snprintf(at.prelude, CSS_MAX_VALUE, "%s", tmp);
            if (!lex_eof(l)) {
                if (lex_peek(l) == ';') lex_advance(l);
                else if (lex_peek(l) == '{') skip_block(l);
            }
            sheet_add_at(sheet, &at);
            continue;
        }

        /* Normal rule */
        CSSRule *rule = calloc(1, sizeof(*rule));
        if (!rule) return;
        if (cb && cb->on_rule_start) cb->on_rule_start(cb->user_data);

        parse_selector_list(l, rule);
        lex_skip_ws(l);

        if (lex_peek(l) == '{') {
            /* emit selectors */
            if (cb && cb->on_selector) {
                for (int i = 0; i < rule->selector_count; i++)
                    cb->on_selector(cb->user_data, &rule->selectors[i], i, rule->selector_count);
            }
            NestList nest = {0};
            NestList *saved_nest = g_nest_list;
            g_nest_list = &nest;
            parse_rule_block(l, rule, cb);
            g_nest_list = saved_nest;

            /* compute max specificity */
            for (int i = 0; i < rule->selector_count; i++) {
                if (rule->selectors[i].specificity > rule->specificity)
                    rule->specificity = rule->selectors[i].specificity;
            }

            if (rule->selector_count > 0 || rule->decl_count > 0)
                sheet_add_rule(sheet, rule);      /* the sheet now owns its arrays */
            else
                css_rule_release(rule);
            for (int ni = 0; ni < nest.n; ni++) {
                CSSRule *nr = &nest.items[ni].rule;
                if (nr->selector_count > 0 && nr->decl_count > 0) sheet_add_rule(sheet, nr);
                else css_rule_release(nr);
                free(nest.items[ni].text);
            }
            free(nest.items);
        } else {
            css_rule_release(rule);
            /* malformed: skip to next brace or semicolon */
            while (!lex_eof(l) && lex_peek(l) != '{' && lex_peek(l) != ';')
                lex_advance(l);
            if (!lex_eof(l)) {
                if (lex_peek(l) == '{') skip_block(l);
                else lex_advance(l);
            }
        }

        if (cb && cb->on_rule_end) cb->on_rule_end(cb->user_data);
        free(rule);
    }
}

/* ── CSS custom property resolver ───────────────────────────────── */

static void custom_props_take(CSSStyleSheet *sheet, const CSSRule *rule) {
    for (int di = 0; di < rule->decl_count; di++) {
        const char *prop = rule->decls[di].property;
        if (prop[0] != '-' || prop[1] != '-') continue;
        int ci = 0;
        while (ci < sheet->custom_prop_count && strcmp(sheet->custom_props[ci].name, prop) != 0) ci++;
        if (ci == sheet->custom_prop_count) {
            if (ci >= CSS_MAX_CUSTOM_PROPS) continue;
            sheet->custom_prop_count++;
            snprintf(sheet->custom_props[ci].name, sizeof sheet->custom_props[ci].name, "%.63s", prop);
        }
        snprintf(sheet->custom_props[ci].value, sizeof sheet->custom_props[ci].value, "%.1023s",
                 rule->decls[di].value);
    }
}

static void css_collect_custom_props(CSSStyleSheet *sheet) {
    sheet->custom_prop_count = 0;
    /* Pass 1: collect all --name: value declarations (later ones win) */
    for (int ri = 0; ri < sheet->rule_count; ri++)
        custom_props_take(sheet, &sheet->rules[ri]);
    /* Also check nested (at-rule) rules */
    for (int ai = 0; ai < sheet->at_rule_count; ai++)
        for (int ri2 = 0; ri2 < sheet->at_rules[ai].nested_rule_count; ri2++)
            custom_props_take(sheet, &sheet->at_rules[ai].nested_rules[ri2]);
}

static void resolve_var_in_value(const CSSStyleSheet *sheet, char *value, int maxlen) {
    if (!strstr(value, "var(")) return;
    char out[1024] = {0};
    size_t oi = 0;
    const char *p = value;
    while (*p && oi < (size_t)(maxlen - 1)) {
        if (strncmp(p, "var(", 4) == 0) {
            const char *ns = p + 4;
            while (*ns == ' ') ns++;
            const char *ne = ns;
            while (*ne && *ne != ',' && *ne != ')') ne++;
            char vname[64] = {0};
            int nl = (int)(ne - ns);
            if (nl > 63) nl = 63;
            strncpy(vname, ns, nl);
            /* trim trailing spaces */
            int vl = (int)strlen(vname);
            while (vl > 0 && vname[vl-1] == ' ') vname[--vl] = '\0';
            /* find value */
            const char *found = NULL;
            for (int i = 0; i < sheet->custom_prop_count; i++) {
                if (strcmp(sheet->custom_props[i].name, vname) == 0) {
                    found = sheet->custom_props[i].value; break;
                }
            }
            /* skip to closing paren */
            int depth = 1; p += 4;
            const char *fallback = NULL;
            while (*p && depth > 0) {
                if (*p == '(') depth++;
                else if (*p == ')') { depth--; if (depth == 0) break; }
                else if (*p == ',' && depth == 1 && !fallback) fallback = p + 1;
                p++;
            }
            if (*p == ')') p++;
            if (found) {
                size_t fl = strlen(found);
                if (oi + fl >= (size_t)(maxlen - 1)) fl = (size_t)(maxlen - 1) - oi;
                memcpy(out + oi, found, fl);
                oi += fl;
            } else if (fallback) {
                /* use fallback: trim and copy */
                const char *fb = fallback;
                while (*fb == ' ') fb++;
                /* end is at p (char after the ')' we already skipped) */
                size_t fbl = (size_t)(p - 1 - fallback); /* approx */
                if (fbl > 0 && fbl < 511) {
                    char fbbuf[512] = {0};
                    strncpy(fbbuf, fallback, fbl); fbbuf[fbl] = '\0';
                    /* basic trim */
                    int ftl = (int)strlen(fbbuf);
                    while (ftl > 0 && (fbbuf[ftl-1] == ')' || fbbuf[ftl-1] == ' ')) fbbuf[--ftl] = '\0';
                    size_t rfl = strlen(fbbuf);
                    if (oi + rfl >= (size_t)(maxlen - 1)) rfl = (size_t)(maxlen - 1) - oi;
                    memcpy(out + oi, fbbuf, rfl);
                    oi += rfl;
                }
            }
        } else {
            out[oi++] = *p++;
        }
    }
    out[oi] = '\0';
    strncpy(value, out, (size_t)(maxlen - 1));
    value[maxlen - 1] = '\0';
}

void css_resolve_vars(CSSStyleSheet *sheet) {
    if (!sheet) return;
    css_collect_custom_props(sheet);
    /* Pass 2: resolve var() in all non-custom declarations */
    for (int ri = 0; ri < sheet->rule_count; ri++) {
        CSSRule *rule = &sheet->rules[ri];
        for (int di = 0; di < rule->decl_count; di++) {
            if (rule->decls[di].property[0] == '-' && rule->decls[di].property[1] == '-') continue;
            resolve_var_in_value(sheet, rule->decls[di].value, CSS_MAX_VALUE);
        }
    }
    for (int ai = 0; ai < sheet->at_rule_count; ai++) {
        CSSAtRule *at = &sheet->at_rules[ai];
        for (int ri2 = 0; ri2 < at->nested_rule_count; ri2++) {
            CSSRule *rule = &at->nested_rules[ri2];
            for (int di = 0; di < rule->decl_count; di++) {
                if (rule->decls[di].property[0] == '-' && rule->decls[di].property[1] == '-') continue;
                resolve_var_in_value(sheet, rule->decls[di].value, CSS_MAX_VALUE);
            }
        }
    }
}

/* ── Public DOM API ──────────────────────────────────────────────────── */

CSSStyleSheet *css_parse(const char *text, size_t len) {
    if (!text) return NULL;
    if (len == 0) len = strlen(text);
    CSSStyleSheet *sheet = calloc(1, sizeof(CSSStyleSheet));
    Lexer l;
    lex_init(&l, text, len);
    parse_stylesheet_inner(&l, sheet, NULL, 0);
    css_resolve_vars(sheet);
    return sheet;
}

CSSStyleSheet *css_parse_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);
    CSSStyleSheet *sheet = css_parse(buf, (size_t)sz);
    free(buf);
    return sheet;
}

void css_parse_cb(const char *text, size_t len, CSSCallbacks *cb) {
    if (!text || !cb) return;
    if (len == 0) len = strlen(text);
    CSSStyleSheet *sheet = calloc(1, sizeof(CSSStyleSheet));
    Lexer l;
    lex_init(&l, text, len);
    parse_stylesheet_inner(&l, sheet, cb, 0);
    css_free(sheet);
}

void css_free(CSSStyleSheet *sheet) {
    if (!sheet) return;
    for (int i = 0; i < sheet->rule_count; i++)
        css_rule_release(&sheet->rules[i]);
    for (int i = 0; i < sheet->at_rule_count; i++) {
        for (int r = 0; r < sheet->at_rules[i].nested_rule_count; r++)
            css_rule_release(&sheet->at_rules[i].nested_rules[r]);
        free(sheet->at_rules[i].nested_rules);
    }
    free(sheet->rules);
    free(sheet->at_rules);
    free(sheet->charset);
    free(sheet);
}

const CSSDeclaration *css_find_decl(const CSSRule *rule, const char *property) {
    for (int i = 0; i < rule->decl_count; i++) {
        if (strcmp(rule->decls[i].property, property) == 0)
            return &rule->decls[i];
    }
    return NULL;
}

/* ── HTML integration ────────────────────────────────────────────────── */

/* Extract text content of all <style> blocks from HTML.
   Also parse inline style="" attributes as individual rules. */
CSSStyleSheet *css_parse_html(const char *html, size_t len) {
    if (!html) return NULL;
    if (len == 0) len = strlen(html);

    /* Accumulate CSS text */
    size_t css_cap = 4096, css_len = 0;
    char *css_buf = malloc(css_cap);
    if (!css_buf) return NULL;
    css_buf[0] = '\0';

    const char *p = html;
    const char *end = html + len;

    while (p < end) {
        /* Find <style or <STYLE */
        const char *tag = p;
        while (tag < end) {
            if (*tag == '<') {
                if ((end - tag) >= 7 &&
                    (css_strncasecmp(tag, "<style", 6) == 0) &&
                    (tag[6] == '>' || tag[6] == ' ' || tag[6] == '\t' ||
                     tag[6] == '\r' || tag[6] == '\n')) break;
            }
            tag++;
        }
        if (tag >= end) break;

        /* skip to end of opening tag */
        const char *content_start = tag;
        while (content_start < end && *content_start != '>') content_start++;
        if (content_start < end) content_start++; /* skip '>' */

        /* find </style> */
        const char *content_end = content_start;
        while (content_end < end) {
            if ((end - content_end) >= 8 &&
                css_strncasecmp(content_end, "</style", 7) == 0) break;
            content_end++;
        }

        size_t block_len = (size_t)(content_end - content_start);
        if (css_len + block_len + 2 > css_cap) {
            css_cap = (css_len + block_len + 2) * 2;
            css_buf = realloc(css_buf, css_cap);
        }
        memcpy(css_buf + css_len, content_start, block_len);
        css_len += block_len;
        css_buf[css_len++] = '\n';
        css_buf[css_len] = '\0';

        p = content_end;
        /* skip </style> */
        while (p < end && *p != '>') p++;
        if (p < end) p++;
    }

    /* Also parse inline style="" attributes */
    /* We synthesize rules like: [data-inline-N] { ... } */
    p = html;
    int inline_idx = 0;
    while (p < end) {
        /* find style=" */
        const char *attr = p;
        while (attr < end) {
            if ((end - attr) >= 7 &&
                css_strncasecmp(attr, "style=", 6) == 0 &&
                (attr[6] == '"' || attr[6] == '\'')) {
                break;
            }
            attr++;
        }
        if (attr >= end) break;

        /* back-track to find element tag name */
        const char *tag_start = attr - 1;
        while (tag_start > html && *tag_start != '<') tag_start--;
        char elem[64] = {0};
        if (*tag_start == '<') {
            const char *ts = tag_start + 1;
            while (*ts && !isspace((unsigned char)*ts) && *ts != '>' && *ts != '/')
                ts++;
            size_t tl = (size_t)(ts - (tag_start + 1));
            if (tl > 63) tl = 63;
            memcpy(elem, tag_start + 1, tl);
            elem[tl] = '\0';
        }

        char quote = attr[6];
        const char *val_start = attr + 7;
        const char *val_end = val_start;
        while (val_end < end && *val_end != quote) val_end++;

        /* synthesize: elem[data-inline-N] { ... } */
        size_t val_len = (size_t)(val_end - val_start);
        char rule_text[CSS_MAX_VALUE * 2];
        snprintf(rule_text, sizeof(rule_text) - 1,
                 "%s[data-inline-%d] { %.*s }\n",
                 elem[0] ? elem : "*", inline_idx++,
                 (int)val_len, val_start);

        size_t rl = strlen(rule_text);
        if (css_len + rl + 2 > css_cap) {
            css_cap = (css_len + rl + 2) * 2;
            css_buf = realloc(css_buf, css_cap);
        }
        memcpy(css_buf + css_len, rule_text, rl);
        css_len += rl;
        css_buf[css_len] = '\0';

        p = val_end + 1;
    }

    CSSStyleSheet *sheet = NULL;
    if (css_len > 0) sheet = css_parse(css_buf, css_len);
    free(css_buf);
    return sheet;
}

/* ── Debug dump ──────────────────────────────────────────────────────── */

static const char *comb_str(CSSCombinator c) {
    switch (c) {
    case CSS_COMB_DESC:     return " ";
    case CSS_COMB_CHILD:    return " > ";
    case CSS_COMB_ADJACENT: return " + ";
    case CSS_COMB_SIBLING:  return " ~ ";
    default: return "";
    }
}

static void dump_selector(const CSSSelector *sel, FILE *fp) {
    for (int ci = 0; ci < sel->compound_count; ci++) {
        const CSSCompound *cmp = &sel->compounds[ci];
        if (ci > 0) fputs(comb_str(cmp->combinator), fp);
        for (int pi = 0; pi < cmp->part_count; pi++) {
            const CSSSelectorPart *p = &cmp->parts[pi];
            switch (p->type) {
            case CSS_SEL_UNIVERSAL:    fputc('*', fp); break;
            case CSS_SEL_TYPE:         fputs(p->name, fp); break;
            case CSS_SEL_CLASS:        fprintf(fp, ".%s", p->name); break;
            case CSS_SEL_ID:           fprintf(fp, "#%s", p->name); break;
            case CSS_SEL_ATTR:         fprintf(fp, "[%s]", p->name); break;
            case CSS_SEL_ATTR_EQ:      fprintf(fp, "[%s=%s]", p->name, p->attr_val); break;
            case CSS_SEL_ATTR_INCLUDES:fprintf(fp, "[%s~=%s]", p->name, p->attr_val); break;
            case CSS_SEL_ATTR_DASH:    fprintf(fp, "[%s|=%s]", p->name, p->attr_val); break;
            case CSS_SEL_ATTR_PREFIX:  fprintf(fp, "[%s^=%s]", p->name, p->attr_val); break;
            case CSS_SEL_ATTR_SUFFIX:  fprintf(fp, "[%s$=%s]", p->name, p->attr_val); break;
            case CSS_SEL_ATTR_SUBSTR:  fprintf(fp, "[%s*=%s]", p->name, p->attr_val); break;
            case CSS_SEL_PSEUDO_CLASS: {
                fprintf(fp, ":%s", p->name);
                if (p->pseudo_arg[0]) fprintf(fp, "(%s)", p->pseudo_arg);
                break;
            }
            case CSS_SEL_PSEUDO_ELEM:  fprintf(fp, "::%s", p->name); break;
            }
        }
    }
}

void css_dump(const CSSStyleSheet *sheet, FILE *fp) {
    if (!sheet || !fp) return;
    if (sheet->charset) fprintf(fp, "@charset \"%s\";\n\n", sheet->charset);

    for (int i = 0; i < sheet->at_rule_count; i++) {
        const CSSAtRule *at = &sheet->at_rules[i];
        fprintf(fp, "@at{type=%d prelude=\"%s\"}\n", at->type, at->prelude);
    }

    for (int i = 0; i < sheet->rule_count; i++) {
        const CSSRule *r = &sheet->rules[i];
        for (int si = 0; si < r->selector_count; si++) {
            if (si) fputs(", ", fp);
            dump_selector(&r->selectors[si], fp);
        }
        fputs(" {\n", fp);
        for (int di = 0; di < r->decl_count; di++) {
            fprintf(fp, "  %s: %s%s;\n",
                    r->decls[di].property,
                    r->decls[di].value,
                    r->decls[di].important ? " !important" : "");
        }
        fputs("}\n", fp);
    }
    fprintf(fp, "/* %d rules, %d @rules */\n", sheet->rule_count, sheet->at_rule_count);
}

#endif /* CSS_PARSER_IMPLEMENTATION */
