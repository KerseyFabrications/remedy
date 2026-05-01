/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SANITIZE_H
#define SANITIZE_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Check if a string is safe to embed in terminal escape sequences
 *
 * Returns false if the string contains any control characters (bytes < 0x20,
 * 0x7F, or 0x9C) that could be used for escape sequence injection.
 */
bool is_safe_for_escape(const char *str);

/**
 * @brief Create a sanitized copy of a string safe for terminal escape sequences
 *
 * Strips bytes < 0x20, 0x7F, and 0x9C. Caller must free the result.
 * Returns NULL on allocation failure.
 */
char *sanitize_for_escape(const char *str);

#endif /* SANITIZE_H */
