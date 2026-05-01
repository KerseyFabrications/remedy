/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef KITTY_GRAPHICS_H
#define KITTY_GRAPHICS_H

#include "render_line.h"

#include <stdbool.h>

/**
 * @brief Check if the terminal supports the Kitty graphics protocol
 *
 * Checks TERM_PROGRAM environment variable for known supporting terminals.
 *
 * @return true if Kitty graphics is supported
 */
bool kitty_graphics_supported(void);

/**
 * @brief Render an image at the current cursor position
 *
 * Reads the image file, base64 encodes it, and transmits via the Kitty
 * graphics protocol. Falls back silently if the file doesn't exist.
 *
 * @param path Path to the image file (relative to the markdown file's directory)
 * @param image_id Unique ID for this image (1-based)
 * @param max_width Maximum display width in columns
 * @param max_height Maximum display height in rows
 * @param out_cols Actual display columns used (may be NULL)
 * @param out_rows Actual display rows used (may be NULL)
 * @return Number of terminal rows consumed, or 0 on failure
 */
int kitty_graphics_display(const char *path, int image_id, int max_width, int max_height, int *out_cols, int *out_rows);

/**
 * @brief Place a previously transmitted image at the current cursor position
 */
void kitty_graphics_place(int image_id, int max_width, int max_height);

/**
 * @brief Delete all image placements
 */
void kitty_graphics_delete_placements(void);

/**
 * @brief Render an image inline during markdown rendering
 *
 * If Kitty graphics is supported and the image file exists, appends
 * placeholder lines to the buffer to reserve space for the image.
 * The actual image is drawn during the post-draw pass.
 *
 * @param path Image file path
 * @param alt_text Alt text for fallback
 * @param base_dir Base directory for resolving relative paths
 * @param indent Current indent level
 * @param buf Line buffer to append to
 * @return SUCCESS or FAILURE
 */
int kitty_graphics_render(const char *path, const char *alt_text, const char *base_dir, int indent, line_buffer_t *buf);

#endif /* KITTY_GRAPHICS_H */
