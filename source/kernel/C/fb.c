/* Copyright (C) 2022-2023 mintsuki and contributors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stddef.h>
#include <stdint.h>

#include <fb.h>
#include <flanterm/flanterm.h>

void *memset(void *, int, size_t);
void *memcpy(void *, const void *, size_t);

static size_t ssfn_terminal_cell_width(ssfn_t *ssfn) {
    size_t widest = 0;

    for (uint32_t codepoint = 0x20; codepoint <= 0x7e; codepoint++) {
        char text[2] = { (char)codepoint, '\0' };
        int width = 0;

        if (ssfn_bbox(ssfn, text, &width, NULL, NULL, NULL) == SSFN_OK &&
            width > 0 && (size_t)width > widest) {
            widest = (size_t)width;
        }
    }

    return widest;
}

static size_t ssfn_encode_utf8(uint32_t codepoint, char output[5]) {
    if (codepoint <= 0x7f) {
        output[0] = (char)codepoint;
        return 1;
    }
    if (codepoint <= 0x7ff) {
        output[0] = (char)(0xc0 | (codepoint >> 6));
        output[1] = (char)(0x80 | (codepoint & 0x3f));
        return 2;
    }
    output[0] = (char)(0xe0 | (codepoint >> 12));
    output[1] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
    output[2] = (char)(0x80 | (codepoint & 0x3f));
    return 3;
}

#define FLANTERM_FB_SSFN_SS 4                 /* supersample factor: 3 or 4 */
#define FLANTERM_FB_SSFN_CACHE_SLOTS 261      /* 0..259 cacheable, 260 = scratch */
#define FLANTERM_FB_SSFN_SCRATCH 260
 
/*
 * Look knobs:
 *   WEIGHT  -8 (very bold) .. 0 (exactly as rendered) .. +8 (thin). Start at 0.
 */
#define FLANTERM_FB_SSFN_WEIGHT -6
#define FLANTERM_FB_SSFN_SOFTEN_KERNEL 3   /* 2 = 2x2 box, 3 = 3x3 [1 2 1] */
#define FLANTERM_FB_SSFN_SOFTEN_AMOUNT 3  /* 0 = off (crisp) .. 8 = full blur */
 
/* Remap coverage. 0 -> 0 and 255 -> 255 always; only the edges change. */
static inline uint32_t ssfn_curve(uint32_t a) {
    int w = FLANTERM_FB_SSFN_WEIGHT;
    if (w >= 0) {
        uint32_t thin = a * a / 255;                 /* a=128 -> 64 */
        return (a * (uint32_t)(8 - w) + thin * (uint32_t)w) / 8;
    } else {
        uint32_t bold = a * (510 - a) / 255;         /* a=128 -> 191 */
        return (a * (uint32_t)(8 + w) + bold * (uint32_t)(-w)) / 8;
    }
}
 
static inline uint32_t ssfn_mix(uint32_t bg, uint32_t fg, uint32_t a) {
    uint32_t out = 0;
    for (int s = 0; s <= 16; s += 8) {
        int b = (int)((bg >> s) & 0xff);
        int f = (int)((fg >> s) & 0xff);
        out |= (uint32_t)(b + ((f - b) * (int)a) / 255) << s;
    }
    return out;
}
 
static int ssfn_cache_slot(uint32_t u) {
    if (u < 0x100) return (int)u;
    switch (u) {
        case 0x2502: return 256;
        case 0x2500: return 257;
        case 0x2514: return 258;
        case 0x251c: return 259;
        default: return -1;
    }
}
 
/* Render one glyph at SS x size and box-filter it down to cw*ch coverage bytes. */
static void ssfn_render_coverage(struct flanterm_fb_context *ctx,
                                 uint32_t unicode, uint8_t *out) {
    const size_t SS = FLANTERM_FB_SSFN_SS;
    size_t cw = ctx->glyph_width, ch = ctx->glyph_height;
    size_t hw = cw * SS, hh = ch * SS;
    uint32_t *hi = ctx->ssfn_cell;
 
    for (size_t i = 0; i < hw * hh; i++)
        hi[i] = 0xff000000;                     /* opaque black = zero coverage */
 
    char encoded[5] = { 0 };
    ssfn_encode_utf8(unicode, encoded);

    /* center narrow glyphs in the cell (skip box-drawing so lines still join) */
    int gx = 0;
    if (unicode < 0x2500) {
        int gw = 0;
        if (ssfn_bbox(&ctx->ssfn, encoded, &gw, NULL, NULL, NULL) == SSFN_OK &&
            gw > 0 && gw < (int)hw)
            gx = ((int)hw - gw) / 2;
    }

    ssfn_buf_t target = {
        .ptr = (uint8_t *)hi,
        .w = (int)hw,
        .h = (int)hh,
        .p = (uint16_t)(hw * sizeof(uint32_t)),
        .x = gx,
        .y = (int)ctx->ssfn_baseline,
        .fg = 0xffffffff,
        .bg = 0,
    };
    (void)ssfn_render(&ctx->ssfn, &target, encoded);
 
    for (size_t y = 0; y < ch; y++) {
        for (size_t x = 0; x < cw; x++) {
            uint32_t sum = 0;
            for (size_t sy = 0; sy < SS; sy++) {
                const uint32_t *row = &hi[(y * SS + sy) * hw + x * SS];
                for (size_t sx = 0; sx < SS; sx++)
                    sum += (row[sx] >> 16) & 0xff;
            }
            out[y * cw + x] = (uint8_t)(sum / (SS * SS));
        }
    }
 
#if FLANTERM_FB_SSFN_SOFTEN_AMOUNT > 0
    uint8_t *tmp = (uint8_t *)hi;   /* hi-res scratch is free again */
    for (size_t i = 0; i < cw * ch; i++)
        tmp[i] = out[i];

    for (size_t y = 0; y < ch; y++) {
        for (size_t x = 0; x < cw; x++) {
            uint32_t blurred;

#if FLANTERM_FB_SSFN_SOFTEN_KERNEL == 2
            uint32_t s  = tmp[y * cw + x];
            uint32_t r  = (x + 1 < cw) ? tmp[y * cw + x + 1] : 0;
            uint32_t d  = (y + 1 < ch) ? tmp[(y + 1) * cw + x] : 0;
            uint32_t rd = (x + 1 < cw && y + 1 < ch)
                              ? tmp[(y + 1) * cw + x + 1] : 0;
            blurred = (s + r + d + rd) / 4;
#else
            uint32_t sum = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    long ny = (long)y + dy, nx = (long)x + dx;
                    if (ny < 0 || ny >= (long)ch || nx < 0 || nx >= (long)cw)
                        continue;
                    uint32_t k = (uint32_t)((dy == 0 ? 2 : 1) * (dx == 0 ? 2 : 1));
                    sum += tmp[(size_t)ny * cw + (size_t)nx] * k;
                }
            }
            blurred = sum / 16;
#endif

            uint32_t orig = tmp[y * cw + x];
            out[y * cw + x] = (uint8_t)((orig * (8 - FLANTERM_FB_SSFN_SOFTEN_AMOUNT) +
                                         blurred * FLANTERM_FB_SSFN_SOFTEN_AMOUNT) / 8);
        }
    }
#endif
}
 
static void plot_ssfn_char(struct flanterm_fb_context *ctx,
                           struct flanterm_fb_char *c, size_t x, size_t y) {
    size_t origin_x = ctx->offset_x + x * ctx->glyph_width;
    size_t origin_y = ctx->offset_y + y * ctx->glyph_height;
    size_t cw = ctx->glyph_width, ch = ctx->glyph_height;
    bool draw = c->fg != 0xffffffff;
    const uint8_t *cov = NULL;
 
    if (draw) {
        uint32_t unicode = c->c;
 
        /* Flanterm stores these UTF-8 tree glyphs internally as CP437 bytes. */
        switch (unicode) {
            case 0xb3: unicode = 0x2502; break; /* │ */
            case 0xc4: unicode = 0x2500; break; /* ─ */
            case 0xc0: unicode = 0x2514; break; /* └ */
            case 0xc3: unicode = 0x251c; break; /* ├ */
            default: break;
        }
 
        size_t cells = cw * ch;
        size_t hi_px = cw * FLANTERM_FB_SSFN_SS * ch * FLANTERM_FB_SSFN_SS;
        uint8_t *cache = (uint8_t *)(ctx->ssfn_cell + hi_px);
        uint8_t *valid = cache + FLANTERM_FB_SSFN_CACHE_SLOTS * cells;
 
        int slot = ssfn_cache_slot(unicode);
        bool cacheable = slot >= 0;
        if (!cacheable)
            slot = FLANTERM_FB_SSFN_SCRATCH;
 
        uint8_t *entry = cache + (size_t)slot * cells;
        if (!cacheable || !valid[slot]) {
            ssfn_render_coverage(ctx, unicode, entry);
            if (cacheable)
                valid[slot] = 1;
        }
        cov = entry;
    }
 
    for (size_t py = 0; py < ch; py++) {
        volatile uint32_t *line = ctx->framebuffer + origin_x +
            (origin_y + py) * (ctx->pitch / sizeof(uint32_t));
        for (size_t px = 0; px < cw; px++) {
#ifndef FLANTERM_FB_DISABLE_CANVAS
            uint32_t bg = c->bg == 0xffffffff
                ? ctx->canvas[(origin_y + py) * ctx->width + origin_x + px]
                : c->bg;
#else
            uint32_t bg = c->bg == 0xffffffff ? ctx->default_bg : c->bg;
#endif
            uint32_t out = bg;
            if (cov) {
                uint32_t a = cov[py * cw + px];
                if (a) {
                    out = ssfn_mix(bg, c->fg, ssfn_curve(a));
                }
            }
            line[px] = out;
        }
    }
}


#ifndef FLANTERM_FB_DISABLE_BUMP_ALLOC

#ifndef FLANTERM_FB_BUMP_ALLOC_POOL_SIZE
#define FLANTERM_FB_BUMP_ALLOC_POOL_SIZE (64 * 1024 * 1024)
#endif

static uint8_t bump_alloc_pool[FLANTERM_FB_BUMP_ALLOC_POOL_SIZE];
static size_t bump_alloc_ptr = 0;

static void *bump_alloc(size_t s) {
    size_t next_ptr = bump_alloc_ptr + s;
    if (next_ptr > FLANTERM_FB_BUMP_ALLOC_POOL_SIZE) {
        return NULL;
    }
    void *ret = &bump_alloc_pool[bump_alloc_ptr];
    bump_alloc_ptr = next_ptr;
    return ret;
}

#endif

static void flanterm_fb_save_state(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;
    ctx->saved_state_text_fg = ctx->text_fg;
    ctx->saved_state_text_bg = ctx->text_bg;
    ctx->saved_state_cursor_x = ctx->cursor_x;
    ctx->saved_state_cursor_y = ctx->cursor_y;
}

static void flanterm_fb_restore_state(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;
    ctx->text_fg = ctx->saved_state_text_fg;
    ctx->text_bg = ctx->saved_state_text_bg;
    ctx->cursor_x = ctx->saved_state_cursor_x;
    ctx->cursor_y = ctx->saved_state_cursor_y;
}

static void flanterm_fb_swap_palette(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;
    uint32_t tmp = ctx->text_bg;
    ctx->text_bg = ctx->text_fg;
    ctx->text_fg = tmp;
}

static void plot_char(struct flanterm_context *_ctx, struct flanterm_fb_char *c, size_t x, size_t y) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    if (x >= _ctx->cols || y >= _ctx->rows) {
        return;
    }

#ifdef FLANTERM_FB_DISABLE_CANVAS
    uint32_t default_bg = ctx->default_bg;
#endif

    x = ctx->offset_x + x * ctx->glyph_width;
    y = ctx->offset_y + y * ctx->glyph_height;

    if (ctx->using_ssfn) {
        plot_ssfn_char(ctx, c, (x - ctx->offset_x) / ctx->glyph_width,
                       (y - ctx->offset_y) / ctx->glyph_height);
        return;
    }

    bool *glyph = &ctx->font_bool[c->c * ctx->font_height * ctx->font_width];
    // naming: fx,fy for font coordinates, gx,gy for glyph coordinates
    for (size_t gy = 0; gy < ctx->glyph_height; gy++) {
        uint8_t fy = gy / ctx->font_scale_y;
        volatile uint32_t *fb_line = ctx->framebuffer + x + (y + gy) * (ctx->pitch / 4);

#ifndef FLANTERM_FB_DISABLE_CANVAS
        uint32_t *canvas_line = ctx->canvas + x + (y + gy) * ctx->width;
#endif

        for (size_t fx = 0; fx < ctx->font_width; fx++) {
            bool draw = glyph[fy * ctx->font_width + fx];
            for (size_t i = 0; i < ctx->font_scale_x; i++) {
                size_t gx = ctx->font_scale_x * fx + i;
#ifndef FLANTERM_FB_DISABLE_CANVAS
                uint32_t bg = c->bg == 0xffffffff ? canvas_line[gx] : c->bg;
                uint32_t fg = c->fg == 0xffffffff ? canvas_line[gx] : c->fg;
#else
                uint32_t bg = c->bg == 0xffffffff ? default_bg : c->bg;
                uint32_t fg = c->fg == 0xffffffff ? default_bg : c->fg;
#endif
                fb_line[gx] = draw ? fg : bg;
            }
        }
    }
}

static void plot_char_fast(struct flanterm_context *_ctx, struct flanterm_fb_char *old, struct flanterm_fb_char *c, size_t x, size_t y) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    if (x >= _ctx->cols || y >= _ctx->rows) {
        return;
    }

    if (ctx->using_ssfn) {
        plot_char(_ctx, c, x, y);
        return;
    }

    x = ctx->offset_x + x * ctx->glyph_width;
    y = ctx->offset_y + y * ctx->glyph_height;

#ifdef FLANTERM_FB_DISABLE_CANVAS
    uint32_t default_bg = ctx->default_bg;
#endif

    bool *new_glyph = &ctx->font_bool[c->c * ctx->font_height * ctx->font_width];
    bool *old_glyph = &ctx->font_bool[old->c * ctx->font_height * ctx->font_width];
    for (size_t gy = 0; gy < ctx->glyph_height; gy++) {
        uint8_t fy = gy / ctx->font_scale_y;
        volatile uint32_t *fb_line = ctx->framebuffer + x + (y + gy) * (ctx->pitch / 4);
#ifndef FLANTERM_FB_DISABLE_CANVAS
        uint32_t *canvas_line = ctx->canvas + x + (y + gy) * ctx->width;
#endif
        for (size_t fx = 0; fx < ctx->font_width; fx++) {
            bool old_draw = old_glyph[fy * ctx->font_width + fx];
            bool new_draw = new_glyph[fy * ctx->font_width + fx];
            if (old_draw == new_draw)
                continue;
            for (size_t i = 0; i < ctx->font_scale_x; i++) {
                size_t gx = ctx->font_scale_x * fx + i;
#ifndef FLANTERM_FB_DISABLE_CANVAS
                uint32_t bg = c->bg == 0xffffffff ? canvas_line[gx] : c->bg;
                uint32_t fg = c->fg == 0xffffffff ? canvas_line[gx] : c->fg;
#else
                uint32_t bg = c->bg == 0xffffffff ? default_bg : c->bg;
                uint32_t fg = c->fg == 0xffffffff ? default_bg : c->fg;
#endif
                fb_line[gx] = new_draw ? fg : bg;
            }
        }
    }
}

static inline bool compare_char(struct flanterm_fb_char *a, struct flanterm_fb_char *b) {
    return !(a->c != b->c || a->bg != b->bg || a->fg != b->fg);
}

static void push_to_queue(struct flanterm_context *_ctx, struct flanterm_fb_char *c, size_t x, size_t y) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    if (x >= _ctx->cols || y >= _ctx->rows) {
        return;
    }

    size_t i = y * _ctx->cols + x;

    struct flanterm_fb_queue_item *q = ctx->map[i];

    if (q == NULL) {
        if (compare_char(&ctx->grid[i], c)) {
            return;
        }
        q = &ctx->queue[ctx->queue_i++];
        q->x = x;
        q->y = y;
        ctx->map[i] = q;
    }

    q->c = *c;
}

static struct flanterm_fb_char *current_char(struct flanterm_fb_context *ctx, size_t i) {
    struct flanterm_fb_queue_item *q = ctx->map[i];
    return q != NULL ? &q->c : &ctx->grid[i];
}

static void scrollback_save_top_line(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    /* Only full-screen scrolling creates terminal output history. */
    if (ctx->scrollback == NULL || _ctx->scroll_top_margin != 0 ||
        _ctx->scroll_bottom_margin != _ctx->rows) {
        return;
    }

    size_t line = (ctx->scrollback_start + ctx->scrollback_count) % ctx->scrollback_size;
    if (ctx->scrollback_count == ctx->scrollback_size) {
        ctx->scrollback_start = (ctx->scrollback_start + 1) % ctx->scrollback_size;
        line = (ctx->scrollback_start + ctx->scrollback_count - 1) % ctx->scrollback_size;
    } else {
        ctx->scrollback_count++;
    }

    for (size_t x = 0; x < _ctx->cols; x++)
        ctx->scrollback[line * _ctx->cols + x] = *current_char(ctx, x);

    /* Keep the same content visible if output arrives while reviewing logs. */
    if (ctx->scrollback_offset != 0 && ctx->scrollback_offset < ctx->scrollback_count)
        ctx->scrollback_offset++;
}

static void scrollback_redraw(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;
    size_t first_line = ctx->scrollback_count - ctx->scrollback_offset;

    for (size_t y = 0; y < _ctx->rows; y++) {
        size_t source = first_line + y;
        struct flanterm_fb_char *line;
        if (source < ctx->scrollback_count) {
            size_t history_line = (ctx->scrollback_start + source) % ctx->scrollback_size;
            line = &ctx->scrollback[history_line * _ctx->cols];
        } else {
            line = &ctx->grid[(source - ctx->scrollback_count) * _ctx->cols];
        }
        for (size_t x = 0; x < _ctx->cols; x++)
            plot_char(_ctx, &line[x], x, y);
    }
}

void flanterm_fb_scrollback_up(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;
    if (ctx == NULL || ctx->scrollback_offset == ctx->scrollback_count)
        return;

    ctx->scrollback_offset++;
    scrollback_redraw(_ctx);
}

void flanterm_fb_scrollback_down(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;
    if (ctx == NULL || ctx->scrollback_offset == 0)
        return;

    ctx->scrollback_offset--;
    if (ctx->scrollback_offset == 0)
        _ctx->full_refresh(_ctx);
    else
        scrollback_redraw(_ctx);
}

static void flanterm_fb_revscroll(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    for (size_t i = (_ctx->scroll_bottom_margin - 1) * _ctx->cols - 1;
        i >= _ctx->scroll_top_margin * _ctx->cols; i--) {
        if (i == (size_t)-1) {
            break;
        }
        struct flanterm_fb_char *c;
        struct flanterm_fb_queue_item *q = ctx->map[i];
        if (q != NULL) {
            c = &q->c;
        } else {
            c = &ctx->grid[i];
        }
        push_to_queue(_ctx, c, (i + _ctx->cols) % _ctx->cols, (i + _ctx->cols) / _ctx->cols);
    }

    // Clear the first line of the screen.
    struct flanterm_fb_char empty;
    empty.c = ' ';
    empty.fg = ctx->text_fg;
    empty.bg = ctx->text_bg;
    for (size_t i = 0; i < _ctx->cols; i++) {
        push_to_queue(_ctx, &empty, i, _ctx->scroll_top_margin);
    }
}

static void flanterm_fb_scroll(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    scrollback_save_top_line(_ctx);

    for (size_t i = (_ctx->scroll_top_margin + 1) * _ctx->cols;
        i < _ctx->scroll_bottom_margin * _ctx->cols; i++) {
        struct flanterm_fb_char *c;
        struct flanterm_fb_queue_item *q = ctx->map[i];
        if (q != NULL) {
            c = &q->c;
        } else {
            c = &ctx->grid[i];
        }
        push_to_queue(_ctx, c, (i - _ctx->cols) % _ctx->cols, (i - _ctx->cols) / _ctx->cols);
    }

    // Clear the last line of the screen.
    struct flanterm_fb_char empty;
    empty.c = ' ';
    empty.fg = ctx->text_fg;
    empty.bg = ctx->text_bg;
    for (size_t i = 0; i < _ctx->cols; i++) {
        push_to_queue(_ctx, &empty, i, _ctx->scroll_bottom_margin - 1);
    }
}

static void flanterm_fb_clear(struct flanterm_context *_ctx, bool move) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    struct flanterm_fb_char empty;
    empty.c = ' ';
    empty.fg = ctx->text_fg;
    empty.bg = ctx->text_bg;
    for (size_t i = 0; i < _ctx->rows * _ctx->cols; i++) {
        push_to_queue(_ctx, &empty, i % _ctx->cols, i / _ctx->cols);
    }

    if (move) {
        ctx->cursor_x = 0;
        ctx->cursor_y = 0;
    }
}

static void flanterm_fb_set_cursor_pos(struct flanterm_context *_ctx, size_t x, size_t y) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    if (x >= _ctx->cols) {
        if ((int)x < 0) {
            x = 0;
        } else {
            x = _ctx->cols - 1;
        }
    }
    if (y >= _ctx->rows) {
        if ((int)y < 0) {
            y = 0;
        } else {
            y = _ctx->rows - 1;
        }
    }
    ctx->cursor_x = x;
    ctx->cursor_y = y;
}

static void flanterm_fb_get_cursor_pos(struct flanterm_context *_ctx, size_t *x, size_t *y) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    *x = ctx->cursor_x >= _ctx->cols ? _ctx->cols - 1 : ctx->cursor_x;
    *y = ctx->cursor_y >= _ctx->rows ? _ctx->rows - 1 : ctx->cursor_y;
}

static void flanterm_fb_move_character(struct flanterm_context *_ctx, size_t new_x, size_t new_y, size_t old_x, size_t old_y) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    if (old_x >= _ctx->cols || old_y >= _ctx->rows || new_x >= _ctx->cols || new_y >= _ctx->rows) {
        return;
    }

    size_t i = old_x + old_y * _ctx->cols;

    struct flanterm_fb_char *c;
    struct flanterm_fb_queue_item *q = ctx->map[i];
    if (q != NULL) {
        c = &q->c;
    } else {
        c = &ctx->grid[i];
    }

    push_to_queue(_ctx, c, new_x, new_y);
}

static void flanterm_fb_set_text_fg(struct flanterm_context *_ctx, size_t fg) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_fg = ctx->ansi_colours[fg];
}

static void flanterm_fb_set_text_bg(struct flanterm_context *_ctx, size_t bg) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_bg = ctx->ansi_colours[bg];
}

static void flanterm_fb_set_text_fg_bright(struct flanterm_context *_ctx, size_t fg) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_fg = ctx->ansi_bright_colours[fg];
}

static void flanterm_fb_set_text_bg_bright(struct flanterm_context *_ctx, size_t bg) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_bg = ctx->ansi_bright_colours[bg];
}

static void flanterm_fb_set_text_fg_rgb(struct flanterm_context *_ctx, uint32_t fg) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_fg = fg;
}

static void flanterm_fb_set_text_bg_rgb(struct flanterm_context *_ctx, uint32_t bg) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_bg = bg;
}

static void flanterm_fb_set_text_fg_default(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_fg = ctx->default_fg;
}

static void flanterm_fb_set_text_bg_default(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_bg = 0xffffffff;
}

static void flanterm_fb_set_text_fg_default_bright(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_fg = ctx->default_fg_bright;
}

static void flanterm_fb_set_text_bg_default_bright(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    ctx->text_bg = ctx->default_bg_bright;
}

static void draw_cursor(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    if (ctx->cursor_x >= _ctx->cols || ctx->cursor_y >= _ctx->rows) {
        return;
    }

    size_t i = ctx->cursor_x + ctx->cursor_y * _ctx->cols;

    struct flanterm_fb_char c;
    struct flanterm_fb_queue_item *q = ctx->map[i];
    if (q != NULL) {
        c = q->c;
    } else {
        c = ctx->grid[i];
    }
    uint32_t tmp = c.fg;
    c.fg = c.bg;
    c.bg = tmp;
    plot_char(_ctx, &c, ctx->cursor_x, ctx->cursor_y);
    if (q != NULL) {
        ctx->grid[i] = q->c;
        ctx->map[i] = NULL;
    }
}

static void flanterm_fb_double_buffer_flush(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    /* Update the live terminal state without replacing the scrollback view. */
    if (ctx->scrollback_offset != 0) {
        for (size_t i = 0; i < ctx->queue_i; i++) {
            struct flanterm_fb_queue_item *q = &ctx->queue[i];
            size_t offset = q->y * _ctx->cols + q->x;
            if (ctx->map[offset] != NULL) {
                ctx->grid[offset] = q->c;
                ctx->map[offset] = NULL;
            }
        }
        ctx->queue_i = 0;
        return;
    }

    if (_ctx->cursor_enabled) {
        draw_cursor(_ctx);
    }

    for (size_t i = 0; i < ctx->queue_i; i++) {
        struct flanterm_fb_queue_item *q = &ctx->queue[i];
        size_t offset = q->y * _ctx->cols + q->x;
        if (ctx->map[offset] == NULL) {
            continue;
        }
        struct flanterm_fb_char *old = &ctx->grid[offset];
        if (q->c.bg == old->bg && q->c.fg == old->fg) {
            plot_char_fast(_ctx, old, &q->c, q->x, q->y);
        } else {
            plot_char(_ctx, &q->c, q->x, q->y);
        }
        ctx->grid[offset] = q->c;
        ctx->map[offset] = NULL;
    }

    if ((ctx->old_cursor_x != ctx->cursor_x || ctx->old_cursor_y != ctx->cursor_y) || _ctx->cursor_enabled == false) {
        if (ctx->old_cursor_x < _ctx->cols && ctx->old_cursor_y < _ctx->rows) {
            plot_char(_ctx, &ctx->grid[ctx->old_cursor_x + ctx->old_cursor_y * _ctx->cols], ctx->old_cursor_x, ctx->old_cursor_y);
        }
    }

    ctx->old_cursor_x = ctx->cursor_x;
    ctx->old_cursor_y = ctx->cursor_y;

    ctx->queue_i = 0;
}

static void flanterm_fb_raw_putchar(struct flanterm_context *_ctx, uint8_t c) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    if (ctx->cursor_x >= _ctx->cols && (ctx->cursor_y < _ctx->scroll_bottom_margin - 1 || _ctx->scroll_enabled)) {
        ctx->cursor_x = 0;
        ctx->cursor_y++;
        if (ctx->cursor_y == _ctx->scroll_bottom_margin) {
            ctx->cursor_y--;
            flanterm_fb_scroll(_ctx);
        }
        if (ctx->cursor_y >= _ctx->cols) {
            ctx->cursor_y = _ctx->cols - 1;
        }
    }

    struct flanterm_fb_char ch;
    ch.c = c;
    ch.fg = ctx->text_fg;
    ch.bg = ctx->text_bg;
    push_to_queue(_ctx, &ch, ctx->cursor_x++, ctx->cursor_y);
}

static void flanterm_fb_full_refresh(struct flanterm_context *_ctx) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

#ifdef FLANTERM_FB_DISABLE_CANVAS
    uint32_t default_bg = ctx->default_bg;
#endif

    for (size_t y = 0; y < ctx->height; y++) {
        for (size_t x = 0; x < ctx->width; x++) {
#ifndef FLANTERM_FB_DISABLE_CANVAS
            ctx->framebuffer[y * (ctx->pitch / sizeof(uint32_t)) + x] = ctx->canvas[y * ctx->width + x];
#else
            ctx->framebuffer[y * (ctx->pitch / sizeof(uint32_t)) + x] = default_bg;
#endif
        }
    }

    for (size_t i = 0; i < (size_t)_ctx->rows * _ctx->cols; i++) {
        size_t x = i % _ctx->cols;
        size_t y = i / _ctx->cols;

        plot_char(_ctx, &ctx->grid[i], x, y);
    }

    if (_ctx->cursor_enabled) {
        draw_cursor(_ctx);
    }
}

static void flanterm_fb_deinit(struct flanterm_context *_ctx, void (*_free)(void *, size_t)) {
    struct flanterm_fb_context *ctx = (void *)_ctx;

    if (_free == NULL) {
        return;
    }

    if (ctx->ssfn_cell != NULL) {
        _free(ctx->ssfn_cell, ctx->ssfn_cell_size);
    }

    _free(ctx->font_bits, ctx->font_bits_size);
    _free(ctx->font_bool, ctx->font_bool_size);
    _free(ctx->grid, ctx->grid_size);
    _free(ctx->scrollback, ctx->scrollback_size * _ctx->cols * sizeof(struct flanterm_fb_char));
    _free(ctx->queue, ctx->queue_size);
    _free(ctx->map, ctx->map_size);

#ifndef FLANTERM_FB_DISABLE_CANVAS
    _free(ctx->canvas, ctx->canvas_size);
#endif

    _free(ctx, sizeof(struct flanterm_fb_context));
}

struct flanterm_context *flanterm_fb_init(
    void *(*_malloc)(size_t),
    void (*_free)(void *, size_t),
    uint32_t *framebuffer, size_t width, size_t height, size_t pitch,
#ifndef FLANTERM_FB_DISABLE_CANVAS
    uint32_t *canvas,
#endif
    uint32_t *ansi_colours, uint32_t *ansi_bright_colours,
    uint32_t *default_bg, uint32_t *default_fg,
    uint32_t *default_bg_bright, uint32_t *default_fg_bright,
    void *font, size_t font_width, size_t font_height, size_t font_spacing,
    size_t font_scale_x, size_t font_scale_y,
    size_t margin,
    const void *ssfn_font, size_t ssfn_font_size) {
#ifndef FLANTERM_FB_DISABLE_BUMP_ALLOC
    size_t orig_bump_alloc_ptr = bump_alloc_ptr;
#endif

    if (_malloc == NULL) {
#ifndef FLANTERM_FB_DISABLE_BUMP_ALLOC
        _malloc = bump_alloc;
#else
        return NULL;
#endif
    }

    struct flanterm_fb_context *ctx = NULL;
    ctx = _malloc(sizeof(struct flanterm_fb_context));
    if (ctx == NULL) {
        goto fail;
    }

    struct flanterm_context *_ctx = (void *)ctx;

    memset(ctx, 0, sizeof(struct flanterm_fb_context));

    if (ansi_colours != NULL) {
        memcpy(ctx->ansi_colours, ansi_colours, sizeof(ctx->ansi_colours));
    } else {
        ctx->ansi_colours[0] = 0x00000000; // black
        ctx->ansi_colours[1] = 0x00aa0000; // red
        ctx->ansi_colours[2] = 0x0000aa00; // green
        ctx->ansi_colours[3] = 0x00aa5500; // brown
        ctx->ansi_colours[4] = 0x000000aa; // blue
        ctx->ansi_colours[5] = 0x00aa00aa; // magenta
        ctx->ansi_colours[6] = 0x0000aaaa; // cyan
        ctx->ansi_colours[7] = 0x00aaaaaa; // grey
    }

    if (ansi_bright_colours != NULL) {
        memcpy(ctx->ansi_bright_colours, ansi_bright_colours, sizeof(ctx->ansi_bright_colours));
    } else {
        ctx->ansi_bright_colours[0] = 0x00555555; // black
        ctx->ansi_bright_colours[1] = 0x00ff5555; // red
        ctx->ansi_bright_colours[2] = 0x0055ff55; // green
        ctx->ansi_bright_colours[3] = 0x00ffff55; // brown
        ctx->ansi_bright_colours[4] = 0x005555ff; // blue
        ctx->ansi_bright_colours[5] = 0x00ff55ff; // magenta
        ctx->ansi_bright_colours[6] = 0x0055ffff; // cyan
        ctx->ansi_bright_colours[7] = 0x00ffffff; // grey
    }

    if (default_bg != NULL) {
        ctx->default_bg = *default_bg;
    } else {
        ctx->default_bg = 0x00000000; // background (black)
    }

    if (default_fg != NULL) {
        ctx->default_fg = *default_fg;
    } else {
        ctx->default_fg = 0x00aaaaaa; // foreground (grey)
    }

    if (default_bg_bright != NULL) {
        ctx->default_bg_bright = *default_bg_bright;
    } else {
        ctx->default_bg_bright = 0x00555555; // background (black)
    }

    if (default_fg_bright != NULL) {
        ctx->default_fg_bright = *default_fg_bright;
    } else {
        ctx->default_fg_bright = 0x00ffffff; // foreground (grey)
    }

    ctx->text_fg = ctx->default_fg;
    ctx->text_bg = 0xffffffff;

    ctx->framebuffer = (void *)framebuffer;
    ctx->width = width;
    ctx->height = height;
    ctx->pitch = pitch;

    if (ssfn_font != NULL) {
        const ssfn_font_t *ssfn_header = ssfn_font;

        if (ssfn_font_size < sizeof(*ssfn_header) || ssfn_header->width == 0 ||
            ssfn_header->height == 0 || ssfn_header->baseline > ssfn_header->height ||
            FLANTERM_FB_SSFN_FONT_HEIGHT < 8 ||
            FLANTERM_FB_SSFN_FONT_HEIGHT > SSFN_SIZE_MAX ||
            ssfn_header->width > UINT16_MAX / sizeof(uint32_t) ||
            ssfn_load(&ctx->ssfn, ssfn_font) != SSFN_OK ||
            ssfn_select(&ctx->ssfn, SSFN_FAMILY_ANY, NULL,
                        SSFN_STYLE_NOCACHE | SSFN_STYLE_ABS_SIZE,
                        FLANTERM_FB_SSFN_FONT_HEIGHT) != SSFN_OK) {
            goto fail;
        }

        ctx->using_ssfn = true;

        /*
        * Use the widest actual printable glyph as the terminal-cell width.
        * This removes the large gap between characters.
        */
        ctx->font_width = ssfn_terminal_cell_width(&ctx->ssfn);
        if (ctx->font_width == 0) {
            goto fail;
        }

        ctx->font_height = FLANTERM_FB_SSFN_FONT_HEIGHT;
                ctx->ssfn_baseline = (ssfn_header->baseline *
                              FLANTERM_FB_SSFN_FONT_HEIGHT * FLANTERM_FB_SSFN_SS +
                              ssfn_header->height - 1) / ssfn_header->height;
 
        ctx->font_scale_x = 1;
        ctx->font_scale_y = 1;
        ctx->glyph_width = ctx->font_width;
        ctx->glyph_height = ctx->font_height;
 
        {
            size_t hi_px = ctx->glyph_width * FLANTERM_FB_SSFN_SS *
                           ctx->glyph_height * FLANTERM_FB_SSFN_SS;
            size_t cache_bytes = (size_t)FLANTERM_FB_SSFN_CACHE_SLOTS *
                                 ctx->glyph_width * ctx->glyph_height;
 
            /* hi-res scratch + coverage cache + valid flags, one allocation so
             * the existing ssfn_cell free()/fail paths keep working. */
            ctx->ssfn_cell_size = hi_px * sizeof(uint32_t) + cache_bytes +
                                  FLANTERM_FB_SSFN_CACHE_SLOTS;
            ctx->ssfn_cell = _malloc(ctx->ssfn_cell_size);
            if (ctx->ssfn_cell == NULL) {
                goto fail;
            }
            /* only the valid flags must start zeroed */
            memset((uint8_t *)(ctx->ssfn_cell + hi_px) + cache_bytes, 0,
                   FLANTERM_FB_SSFN_CACHE_SLOTS);
        }
 
        /* From now on every render is SS x bigger; plot_ssfn_char downsamples. */
        if (ssfn_select(&ctx->ssfn, SSFN_FAMILY_ANY, NULL,
                        SSFN_STYLE_NOCACHE | SSFN_STYLE_ABS_SIZE,
                        FLANTERM_FB_SSFN_FONT_HEIGHT * FLANTERM_FB_SSFN_SS) != SSFN_OK) {
            goto fail;
        }

} else {
#define FONT_BYTES ((font_width * font_height * FLANTERM_FB_FONT_GLYPHS) / 8)
    if (font != NULL) {
        ctx->font_width = font_width;
        ctx->font_height = font_height;
        ctx->font_bits_size = FONT_BYTES;
        ctx->font_bits = _malloc(ctx->font_bits_size);
        if (ctx->font_bits == NULL) {
            goto fail;
        }
        memcpy(ctx->font_bits, font, ctx->font_bits_size);
    } else {
        ctx->font_width = font_width = 8;
        ctx->font_height = font_height = 16;
        ctx->font_bits_size = FONT_BYTES;
        font_spacing = 1;
        ctx->font_bits = _malloc(ctx->font_bits_size);
        if (ctx->font_bits == NULL) {
            goto fail;
        }
        memcpy(ctx->font_bits, unifont, ctx->font_bits_size);
        // second_font: memcpy(ctx->font_bits, builtin_font, ctx->font_bits_size);
    }

#undef FONT_BYTES

    ctx->font_width += font_spacing;

    ctx->font_bool_size = FLANTERM_FB_FONT_GLYPHS * font_height * ctx->font_width * sizeof(bool);
    ctx->font_bool = _malloc(ctx->font_bool_size);
    if (ctx->font_bool == NULL) {
        goto fail;
    }

    for (size_t i = 0; i < FLANTERM_FB_FONT_GLYPHS; i++) {
        uint8_t *glyph = &ctx->font_bits[i * font_height];

        for (size_t y = 0; y < font_height; y++) {
            // NOTE: the characters in VGA fonts are always one byte wide.
            // 9 dot wide fonts have 8 dots and one empty column, except
            // characters 0xC0-0xDF replicate column 9.
            for (size_t x = 0; x < 8; x++) {
                size_t offset = i * font_height * ctx->font_width + y * ctx->font_width + x;

                if ((glyph[y] & (0x80 >> x))) {
                    ctx->font_bool[offset] = true;
                } else {
                    ctx->font_bool[offset] = false;
                }
            }
            // fill columns above 8 like VGA Line Graphics Mode does
            for (size_t x = 8; x < ctx->font_width; x++) {
                size_t offset = i * font_height * ctx->font_width + y * ctx->font_width + x;

                if (i >= 0xc0 && i <= 0xdf) {
                    ctx->font_bool[offset] = (glyph[y] & 1);
                } else {
                    ctx->font_bool[offset] = false;
                }
            }
        }
    }

    ctx->font_scale_x = font_scale_x;
    ctx->font_scale_y = font_scale_y;

    ctx->glyph_width = ctx->font_width * font_scale_x;
    ctx->glyph_height = font_height * font_scale_y;
    }

    _ctx->cols = (ctx->width - margin * 2) / ctx->glyph_width;
    _ctx->rows = (ctx->height - margin * 2) / ctx->glyph_height;

    ctx->offset_x = margin + ((ctx->width - margin * 2) % ctx->glyph_width) / 2;
    ctx->offset_y = margin + ((ctx->height - margin * 2) % ctx->glyph_height) / 2;

    ctx->grid_size = _ctx->rows * _ctx->cols * sizeof(struct flanterm_fb_char);
    ctx->grid = _malloc(ctx->grid_size);
    if (ctx->grid == NULL) {
        goto fail;
    }
    for (size_t i = 0; i < _ctx->rows * _ctx->cols; i++) {
        ctx->grid[i].c = ' ';
        ctx->grid[i].fg = ctx->text_fg;
        ctx->grid[i].bg = ctx->text_bg;
    }

    ctx->scrollback_size = FLANTERM_FB_SCROLLBACK_LINES;
    ctx->scrollback = _malloc(ctx->scrollback_size * _ctx->cols * sizeof(struct flanterm_fb_char));
    if (ctx->scrollback == NULL) {
        goto fail;
    }

    ctx->queue_size = _ctx->rows * _ctx->cols * sizeof(struct flanterm_fb_queue_item);
    ctx->queue = _malloc(ctx->queue_size);
    if (ctx->queue == NULL) {
        goto fail;
    }
    ctx->queue_i = 0;
    memset(ctx->queue, 0, ctx->queue_size);

    ctx->map_size = _ctx->rows * _ctx->cols * sizeof(struct flanterm_fb_queue_item *);
    ctx->map = _malloc(ctx->map_size);
    if (ctx->map == NULL) {
        goto fail;
    }
    memset(ctx->map, 0, ctx->map_size);

#ifndef FLANTERM_FB_DISABLE_CANVAS
    ctx->canvas_size = ctx->width * ctx->height * sizeof(uint32_t);
    ctx->canvas = _malloc(ctx->canvas_size);
    if (ctx->canvas == NULL) {
        goto fail;
    }
    if (canvas != NULL) {
        memcpy(ctx->canvas, canvas, ctx->canvas_size);
    } else {
        for (size_t i = 0; i < ctx->width * ctx->height; i++) {
            ctx->canvas[i] = ctx->default_bg;
        }
    }
#endif

    _ctx->raw_putchar = flanterm_fb_raw_putchar;
    _ctx->clear = flanterm_fb_clear;
    _ctx->set_cursor_pos = flanterm_fb_set_cursor_pos;
    _ctx->get_cursor_pos = flanterm_fb_get_cursor_pos;
    _ctx->set_text_fg = flanterm_fb_set_text_fg;
    _ctx->set_text_bg = flanterm_fb_set_text_bg;
    _ctx->set_text_fg_bright = flanterm_fb_set_text_fg_bright;
    _ctx->set_text_bg_bright = flanterm_fb_set_text_bg_bright;
    _ctx->set_text_fg_rgb = flanterm_fb_set_text_fg_rgb;
    _ctx->set_text_bg_rgb = flanterm_fb_set_text_bg_rgb;
    _ctx->set_text_fg_default = flanterm_fb_set_text_fg_default;
    _ctx->set_text_bg_default = flanterm_fb_set_text_bg_default;
    _ctx->set_text_fg_default_bright = flanterm_fb_set_text_fg_default_bright;
    _ctx->set_text_bg_default_bright = flanterm_fb_set_text_bg_default_bright;
    _ctx->move_character = flanterm_fb_move_character;
    _ctx->scroll = flanterm_fb_scroll;
    _ctx->revscroll = flanterm_fb_revscroll;
    _ctx->swap_palette = flanterm_fb_swap_palette;
    _ctx->save_state = flanterm_fb_save_state;
    _ctx->restore_state = flanterm_fb_restore_state;
    _ctx->double_buffer_flush = flanterm_fb_double_buffer_flush;
    _ctx->full_refresh = flanterm_fb_full_refresh;
    _ctx->deinit = flanterm_fb_deinit;

    flanterm_context_reinit(_ctx);
    flanterm_fb_full_refresh(_ctx);

    return _ctx;

fail:
#ifndef FLANTERM_FB_DISABLE_BUMP_ALLOC
    if (_malloc == bump_alloc) {
        bump_alloc_ptr = orig_bump_alloc_ptr;
        return NULL;
    }
#endif

    if (_free == NULL) {
        return NULL;
    }

#ifndef FLANTERM_FB_DISABLE_CANVAS
    if (ctx->canvas != NULL) {
        _free(ctx->canvas, ctx->canvas_size);
    }
#endif
    if (ctx->map != NULL) {
        _free(ctx->map, ctx->map_size);
    }
    if (ctx->queue != NULL) {
        _free(ctx->queue, ctx->queue_size);
    }
    if (ctx->grid != NULL) {
        _free(ctx->grid, ctx->grid_size);
    }
    if (ctx->font_bool != NULL) {
        _free(ctx->font_bool, ctx->font_bool_size);
    }
    if (ctx->font_bits != NULL) {
        _free(ctx->font_bits, ctx->font_bits_size);
    }
    if (ctx != NULL) {
        _free(ctx, sizeof(struct flanterm_fb_context));
    }

    return NULL;
}
