/* Original Mercenaries shell text layout.
 *
 * Retail MW2SHELL.DLL: substitution 10055350, tokenization 1004f4f0,
 * layout 1004f9c0, line emission 1004f8b0, glyph widths 10050b20,
 * and text-object cursor controls 10040330. See the research note for the
 * original font IDs and the deliberately preserved wrapping quirks.
 */
#include "mercstext.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { TEXT_LIMIT = 1024 * 1024, LARGE_FONT = 6 };
typedef struct { char *s; size_t n, cap; } text_buffer;
typedef struct {
    const char *source;
    size_t index, length;
    int pending, value, action;
} text_parser;

static int lower_ascii(int c)
{ return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }

static int reserve(text_buffer *b, size_t more)
{
    size_t want, cap;
    char *p;
    if (more > TEXT_LIMIT || b->n > TEXT_LIMIT - more) return -1;
    want = b->n + more + 1;
    if (want <= b->cap) return 0;
    cap = b->cap ? b->cap : 128;
    while (cap < want) cap *= 2;
    p = realloc(b->s, cap);
    if (!p) return -1;
    b->s = p; b->cap = cap;
    if (!b->n) b->s[0] = 0;
    return 0;
}

static int append(text_buffer *b, const char *s, size_t n)
{
    if (reserve(b, n)) return -1;
    memcpy(b->s + b->n, s, n);
    b->n += n; b->s[b->n] = 0;
    return 0;
}

static void clear_buffer(text_buffer *b)
{ b->n = 0; if (b->s) b->s[0] = 0; }

static int expand_names(text_buffer *b, const char *raw, const char *pilot,
                        const char *unit, const char *extra)
{
    size_t i;
    if (reserve(b, 0)) return -1;
    for (i = 0; raw[i]; i++) {
        const char *value = NULL;
        if (raw[i] == '\\' && raw[i + 1]) {
            switch (lower_ascii((unsigned char)raw[i + 1])) {
            case 'p': value = pilot ? pilot : ""; break;
            case 'm': value = unit ? unit : ""; break;
            case 'q': value = extra ? extra : ""; break;
            case 'r': value = ""; break;
            default: break;
            }
            if (value) {
                if (append(b, value, strlen(value))) return -1;
                i++;
                continue;
            }
            /* Other escape pairs pass through substitution intact. */
            if (append(b, raw + i, 2)) return -1;
            i++;
        } else if (append(b, raw + i, 1)) return -1;
    }
    return 0;
}

static void skip_spaces(text_parser *p)
{ while (p->index < p->length && p->source[p->index] == ' ') p->index++; }

static int decimal(const char *s, size_t count)
{
    size_t i = 0;
    int sign = 1, value = 0;
    while (i < count && s[i] == ' ') i++;
    if (i < count && (s[i] == '-' || s[i] == '+')) {
        if (s[i] == '-') sign = -1;
        i++;
    }
    while (i < count && s[i] >= '0' && s[i] <= '9')
        value = value * 10 + s[i++] - '0';
    return value * sign;
}

/* Token IDs follow the original switch, including its end-of-input result.
 * Physical CR/LF are ordinary zero-width glyphs, not markup newlines. */
static int next_token(text_parser *p, text_buffer *token)
{
    clear_buffer(token);
    if (p->index >= p->length) return 0;
    if (p->pending) {
        char control[5];
        int value = p->value;
        control[0] = '\\'; control[1] = (char)p->pending;
        /* Authored controls have three decimal digits. Retain the original
         * bytes rather than inventing a wider control on malformed input. */
        if (value < 0 || value > 999) return -1;
        control[2] = (char)('0' + value / 100);
        control[3] = (char)('0' + value / 10 % 10);
        control[4] = (char)('0' + value % 10);
        p->pending = 0;
        return append(token, control, sizeof control) ? -1 : 4;
    }
    while (p->index < p->length) {
        unsigned char c = (unsigned char)p->source[p->index];
        if (c != ' ' && c != '\\') {
            char masked = (char)(c & 0x7f);
            if (append(token, &masked, 1)) return -1;
            p->index++;
            continue;
        }
        if (token->n) {
            size_t start = p->index;
            skip_spaces(p);
            return append(token, p->source + start, p->index - start) ? -1 : 4;
        }
        if (c == ' ') {
            /* The original tokenizer stalls on a leading standalone space.
             * Consume it safely; supplied resources put spaces after words
             * or controls, whose handling above is unchanged. */
            skip_spaces(p);
            continue;
        }
        if (p->index + 1 >= p->length) {
            p->index++;
            if (append(token, "\\", 1)) return -1;
            return 0;
        }
        c = (unsigned char)lower_ascii((unsigned char)p->source[p->index + 1]);
        switch (c) {
        case 'n': case 'c': case 's': case 't': case 'l': {
            int kind = c == 'n' ? 1 : c == 'c' ? 2 : c == 's' ? 3 : c == 't' ? 5 : 9;
            p->index += 2; skip_spaces(p);
            return kind;
        }
        case 'a': case 'b': case 'g': {
            size_t digits = c == 'a' ? 2 : 3;
            int value;
            if (p->length - p->index < digits + 2) return -1;
            value = decimal(p->source + p->index + 2, digits);
            p->index += digits + 2; skip_spaces(p);
            if (c == 'a') p->action = value;
            else { p->value = value; p->pending = c; }
            return c == 'a' ? 6 : c == 'b' ? 7 : 8;
        }
        default:
            if (append(token, "\\", 1)) return -1;
            p->index += 2;
            break;
        }
    }
    return 0;
}

static int tab_stop(int x, int layout)
{
    int i;
    for (i = 0; i < 18; i++) if (x < i * 36) return i * 36;
    if (x < 640 || layout) return 640;
    return x;
}

static int add_run(mercs_text_layout *out, mercs_art *art, const char *s, size_t n,
                    int x, int y, int font, int ink)
{
    mercs_text_run *r;
    char *text;
    int width;
    if (!n) return 0;
    text = malloc(n + 1);
    if (!text) return -1;
    memcpy(text, s, n); text[n] = 0;
    width = mercs_art_width(art, font, text);
    if (!width) { free(text); return 0; }
    if (out->count == out->capacity) {
        size_t capacity = out->capacity ? out->capacity * 2 : 32;
        mercs_text_run *runs;
        if (capacity > TEXT_LIMIT || capacity > SIZE_MAX / sizeof *runs) { free(text); return -1; }
        runs = realloc(out->runs, capacity * sizeof *runs);
        if (!runs) { free(text); return -1; }
        out->runs = runs; out->capacity = capacity;
    }
    r = &out->runs[out->count++];
    r->x = x; r->y = y; r->font = font; r->width = width;
    r->height = mercs_art_height(art, font); r->ink = ink; r->text = text;
    if (r->y + r->height > out->height) out->height = r->y + r->height;
    return 0;
}

/* Decode controls retained in emitted line strings by the original driver.
 * The animated text object's x is relative to the line origin; a tab is not
 * a fixed number of spaces. Action coloring ends at a space or another escape.
 * Action hit rectangles are outside this read-only text-view interface. */
static int emit_line(mercs_text_layout *out, mercs_art *art,
                      const char *line, int normal_font, int large)
{
    size_t at = 0, start = 0, length = strlen(line);
    int font = large ? LARGE_FONT : normal_font;
    int base_ink = large ? 17 : -1, ink = base_ink;
    int x = 0, run_x = 0, y = out->rows * out->line_height;
    if (line[0] == '~') {
        x = -mercs_art_width(art, font, line + 1) / 2;
        start = at = 1; run_x = x;
    }
    while (at < length) {
        if (line[at] == '\\' || (line[at] == ' ' && ink != base_ink)) {
            if (add_run(out, art, line + start, at - start, run_x, y, font, ink)) return -1;
            ink = base_ink;
            if (line[at] == '\\') {
                int c;
                at++;
                if (at >= length) { start = at; break; }
                c = lower_ascii((unsigned char)line[at]);
                if (c == 't') { x = tab_stop(x, 0); at++; }
                else if ((c == 'b' || c == 'g') && length - at >= 4) {
                    int value = decimal(line + at + 1, 3);
                    x = c == 'b' ? x - value : value;
                    at += 4;
                } else if (c == 'a') { ink = 17; at++; }
                /* Unknown commands discard only the slash in the original
                 * display object; the following character is drawn next. */
                start = at; run_x = x;
                continue;
            }
            start = at; run_x = x;
            if (at >= length) break;
            if (line[at] == '\\') continue;
        }
        {
            char ch[2] = { line[at], 0 };
            x += mercs_art_width(art, font, ch);
        }
        at++;
    }
    if (add_run(out, art, line + start, at - start, run_x, y, font, ink)) return -1;
    out->rows++;
    if (out->rows * out->line_height > out->height) out->height = out->rows * out->line_height;
    return 0;
}

static int layout_text(mercs_text_layout *out, mercs_art *art, int font)
{
    text_parser parser;
    text_buffer token = {0}, line = {0}, prefixed = {0};
    int kind, current_width = 0, pending_tabs = 0, center = 0, large = 0, result = -1;
    memset(&parser, 0, sizeof parser);
    parser.source = out->expanded; parser.length = strlen(parser.source);
    parser.action = parser.value = -1;
    if (reserve(&token, 0) || reserve(&line, 0) || reserve(&prefixed, 0)) goto done;
    for (;;) {
        int raw_width, candidate, i;
        kind = next_token(&parser, &token);
        if (kind < 0) goto done;
        switch (kind) {
        case 1:
            if (center) {
                int measure_font = large ? LARGE_FONT : font;
                int measured = large ? mercs_art_width(art, measure_font, line.s) : current_width;
                int space = mercs_art_width(art, measure_font, " ");
                int padding = space > 0 ? ((out->width - measured) / 2) / space : 0;
                if (padding > 0) {
                    if (reserve(&line, (size_t)padding)) goto done;
                    memmove(line.s + padding, line.s, line.n + 1);
                    memset(line.s, ' ', (size_t)padding); line.n += (size_t)padding;
                }
                center = 0;
            }
            if (emit_line(out, art, line.s, font, large)) goto done;
            clear_buffer(&line); current_width = 0; large = 0; parser.action = -1;
            break;
        case 2: center = 1; break;
        case 3:
            out->stopped = 1; out->next_offset = parser.index;
            result = 0; goto done;
        case 4:
            raw_width = mercs_art_width(art, font, token.s);
            clear_buffer(&prefixed);
            for (i = 0; i < pending_tabs; i++) if (append(&prefixed, "\\t", 2)) goto done;
            pending_tabs = 0;
            if (parser.action != -1) {
                if (append(&prefixed, "\\a", 2)) goto done;
                out->unsupported_controls |= 1u; /* clickable action, not just its ink */
            }
            if (append(&prefixed, token.s, token.n)) goto done;
            if (parser.value != -1) { current_width -= parser.value; parser.value = -1; }
            candidate = mercs_art_width(art, font, prefixed.s);
            if (out->width < current_width + candidate) {
                int had_line = line.n != 0;
                if (emit_line(out, art, line.s, font, large)) goto done;
                clear_buffer(&line); current_width = 0; large = 0;
                /* 1004f8b0 returns early on an empty previous line, before
                 * copying the next token. Preserve this authored-layout rule. */
                if (had_line) {
                    if (append(&line, prefixed.s, prefixed.n)) goto done;
                    current_width = candidate;
                }
            } else {
                if (append(&line, prefixed.s, prefixed.n)) goto done;
                current_width += raw_width;
            }
            parser.action = -1;
            break;
        case 5: pending_tabs++; current_width = tab_stop(current_width, 1); break;
        case 6: case 7: case 8: break;
        case 9: large = 1; break;
        default:
            /* The original driver ignores an unterminated final token when
             * the tokenizer returns EOF. Authored paragraphs end with \\n. */
            if (emit_line(out, art, line.s, font, large)) goto done;
            out->next_offset = parser.index;
            result = 0; goto done;
        }
    }
done:
    free(token.s); free(line.s); free(prefixed.s);
    return result;
}

int mercs_text_prepare_names(mercs_text_layout *out, mercs_art *art,
                             const char *raw, const char *pilot,
                             const char *unit, const char *extra,
                             int font, int width)
{
    mercs_text_layout next = {0};
    text_buffer expanded = {0};
    int height;
    if (!out || !art || !raw || width <= 0 || width > 640) return -1;
    height = mercs_art_height(art, font);
    if (height <= 0 || height > 128) return -1;
    if (expand_names(&expanded, raw, pilot, unit, extra)) { free(expanded.s); return -1; }
    next.expanded = expanded.s; next.width = width; next.line_height = height + 2;
    if (layout_text(&next, art, font)) { mercs_text_free(&next); return -1; }
    mercs_text_free(out); *out = next;
    return 0;
}

int mercs_text_prepare(mercs_text_layout *out, mercs_art *art,
                       const char *raw, const char *pilot, int font, int width)
{ return mercs_text_prepare_names(out, art, raw, pilot, "", "", font, width); }

void mercs_text_draw_rows(const mercs_text_layout *layout, mercs_art *art,
                           int x, int y, int height, int scroll_pixels, int ink,
                           int visible_rows)
{
    size_t i;
    if (!layout || !art || height <= 0) return;
    if (scroll_pixels < 0) scroll_pixels = 0;
    for (i = 0; i < layout->count; i++) {
        const mercs_text_run *r = &layout->runs[i];
        int at = r->y - scroll_pixels;
        if (visible_rows >= 0 && (layout->line_height <= 0 ||
            r->y / layout->line_height >= visible_rows)) continue;
        if (at < 0 || at > height || r->height > height - at) continue;
        mercs_art_text(art, r->font, x + r->x, y + at, r->text, r->ink >= 0 ? r->ink : ink);
    }
}

void mercs_text_draw(const mercs_text_layout *layout, mercs_art *art,
                      int x, int y, int height, int scroll_pixels, int ink)
{ mercs_text_draw_rows(layout, art, x, y, height, scroll_pixels, ink, -1); }

void mercs_text_free(mercs_text_layout *layout)
{
    size_t i;
    if (!layout) return;
    for (i = 0; i < layout->count; i++) free(layout->runs[i].text);
    free(layout->runs); free(layout->expanded);
    memset(layout, 0, sizeof *layout);
}

int mercs_text_rows(const mercs_text_layout *layout)
{ return layout ? layout->rows : 0; }
