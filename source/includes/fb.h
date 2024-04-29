/**
 * @file flanterm_fb.h
 *
 * @brief This file contains the header for a framebuffer-based terminal emulator.
 * It provides structures and functions for rendering text on a framebuffer.
 *
 * @author Mintsuki and Pradosh
 * @copyright Copyright (C) 2022-2023 mintsuki and contributors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _FLANTERM_FB_H
#define _FLANTERM_FB_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <flanterm/flanterm.h>

#include <unifont.h>

#define FLANTERM_FB_FONT_GLYPHS 256

extern const uint8_t builtin_font[];
extern const uint8_t thin_font[];

/**
 * @struct flanterm_fb_char
 * @brief Represents a character with its foreground and background colors.
 */
struct flanterm_fb_char {
    uint32_t c; /**< The character code. */
    uint32_t fg; /**< The foreground color. */
    uint32_t bg; /**< The background color. */
};

/**
 * @struct flanterm_fb_queue_item
 * @brief Represents an item in the rendering queue.
 */
struct flanterm_fb_queue_item {
    size_t x, y; /**< The position of the character in the framebuffer. */
    struct flanterm_fb_char c; /**< The character and its colors. */
};

/**
 * @struct flanterm_fb_context
 * @brief Represents the context for framebuffer-based terminal rendering.
 */
struct flanterm_fb_context {
    struct flanterm_context term; /**< The terminal context. */

    size_t font_width; /**< The width of the font. */
    size_t font_height; /**< The height of the font. */
    size_t glyph_width; /**< The width of a font glyph. */
    size_t glyph_height; /**< The height of a font glyph. */

    size_t font_scale_x; /**< The horizontal scaling factor for the font. */
    size_t font_scale_y; /**< The vertical scaling factor for the font. */

    size_t offset_x, offset_y; /**< The offset for rendering. */

    volatile uint32_t *framebuffer; /**< The framebuffer to render on. */
    size_t pitch; /**< The pitch of the framebuffer. */
    size_t width; /**< The width of the framebuffer. */
    size_t height; /**< The height of the framebuffer. */
    size_t bpp; /**< Bits per pixel. */

    size_t font_bits_size; /**< The size of font bits. */
    uint8_t *font_bits; /**< The font bits data. */
    size_t font_bool_size; /**< The size of font boolean data. */
    bool *font_bool; /**< The font boolean data. */

    uint32_t ansi_colours[8]; /**< ANSI color palette. */
    uint32_t ansi_bright_colours[8]; /**< ANSI bright color palette. */
    uint32_t default_fg, default_bg; /**< Default foreground and background colors. */
    uint32_t default_fg_bright, default_bg_bright; /**< Default bright foreground and background colors. */

#ifndef FLANTERM_FB_DISABLE_CANVAS
    size_t canvas_size; /**< The size of the canvas. */
    uint32_t *canvas; /**< The canvas for rendering. */
#endif

    size_t grid_size; /**< The size of the grid. */
    size_t queue_size; /**< The size of the rendering queue. */
    size_t map_size; /**< The size of the mapping structure. */

    struct flanterm_fb_char *grid; /**< The grid for rendering. */

    struct flanterm_fb_queue_item *queue; /**< The rendering queue. */
    size_t queue_i; /**< The current index in the queue. */

    struct flanterm_fb_queue_item **map; /**< The mapping structure. */

    uint32_t text_fg; /**< The foreground color for text. */
    uint32_t text_bg; /**< The background color for text. */
    size_t cursor_x; /**< The current cursor X position. */
    size_t cursor_y; /**< The current cursor Y position. */

    uint32_t saved_state_text_fg; /**< Saved state of text foreground color. */
    uint32_t saved_state_text_bg; /**< Saved state of text background color. */
    size_t saved_state_cursor_x; /**< Saved state of cursor X position. */
    size_t saved_state_cursor_y; /**< Saved state of cursor Y position. */

    size_t old_cursor_x; /**< Old cursor X position. */
    size_t old_cursor_y; /**< Old cursor Y position. */
};

/**
 * @brief Initialize a framebuffer-based terminal context.
 *
 * @param _malloc Pointer to memory allocation function.
 * @param _free Pointer to memory deallocation function.
 * @param framebuffer Pointer to the framebuffer.
 * @param width Width of the framebuffer.
 * @param height Height of the framebuffer.
 * @param pitch Pitch of the framebuffer.
 * @param canvas Pointer to the canvas (optional, set to NULL if not used).
 * @param ansi_colours ANSI color palette.
 * @param ansi_bright_colours ANSI bright color palette.
 * @param default_fg Default foreground color.
 * @param default_bg Default background color.
 * @param default_fg_bright Default bright foreground color.
 * @param default_bg_bright Default bright background color.
 * @param font Pointer to the font data.
 * @param font_width Width of the font.
 * @param font_height Height of the font.
 * @param font_spacing Spacing between font characters.
 * @param font_scale_x Horizontal font scaling factor.
 * @param font_scale_y Vertical font scaling factor.
 * @param margin Margin for rendering.
 *
 * @return A pointer to the initialized framebuffer-based terminal context.
 */
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
    size_t margin
);

#ifndef FLANTERM_FB_DISABLE_BUMP_ALLOC
/**
 * @brief Initialize a framebuffer-based terminal context with simple settings.
 *
 * @param framebuffer Pointer to the framebuffer.
 * @param width Width of the framebuffer.
 * @param height Height of the framebuffer.
 * @param pitch Pitch of the framebuffer.
 *
 * @return A pointer to the initialized framebuffer-based terminal context.
 */
static inline struct flanterm_context *flanterm_fb_simple_init(
    uint32_t *framebuffer, size_t width, size_t height, size_t pitch
) {
    return flanterm_fb_init(
        NULL,
        NULL,
        framebuffer, width, height, pitch,
#ifndef FLANTERM_FB_DISABLE_CANVAS
        NULL,
#endif
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, 0, 0, 1,
        1, 1,
        0
    );
}
#endif

#ifdef __cplusplus
}
#endif

#endif
