/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "sanitize.h"

#include <stdlib.h>
#include <string.h>

bool is_safe_for_escape(const char *str)
{
    if (!str) {
        return false;
    }

    for (const char *p = str; *p; p++) {
        unsigned char c = (unsigned char) *p;
        if (c < 0x20 || c == 0x7F || c == 0x9C) {
            return false;
        }
    }

    return true;
}

char *sanitize_for_escape(const char *str)
{
    if (!str) {
        return NULL;
    }

    size_t len   = strlen(str);
    char *result = malloc(len + 1);
    if (!result) {
        return NULL;
    }

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char) str[i];
        if (c >= 0x20 && c != 0x7F && c != 0x9C) {
            result[j++] = str[i];
        }
    }

    result[j] = '\0';
    return result;
}
