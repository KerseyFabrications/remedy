/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "input.h"

#include <ncurses.h>

input_action_t input_get_action(void)
{
    int ch = getch();

    if (ch == ERR) {
        return ACTION_NONE;
    }

    switch (ch) {
        case 'q':
        case 'Q':
        case 27: /* ESC */
            return ACTION_QUIT;

        case 'j':
        case KEY_DOWN:
        case '\n':
        case '\r':
            return ACTION_SCROLL_DOWN;

        case 'k':
        case KEY_UP:
            return ACTION_SCROLL_UP;

        case 'f':
        case ' ':
        case KEY_NPAGE:
            return ACTION_PAGE_DOWN;

        case 'b':
        case KEY_PPAGE:
            return ACTION_PAGE_UP;

        case 'd':
        case 4: /* Ctrl-D */
            return ACTION_HALF_PAGE_DOWN;

        case 'u':
        case 21: /* Ctrl-U */
            return ACTION_HALF_PAGE_UP;

        case 'g':
        case KEY_HOME:
            return ACTION_HOME;

        case 'G':
        case KEY_END:
            return ACTION_END;

        case '/':
            return ACTION_SEARCH_FORWARD;

        case '?':
            return ACTION_SEARCH_BACKWARD;

        case 'n':
            return ACTION_SEARCH_NEXT;

        case 'N':
            return ACTION_SEARCH_PREV;

        case 't':
            return ACTION_TOGGLE_TOC;

        case 'D':
            return ACTION_DIAGNOSE;

        case 'h':
            return ACTION_HELP;

        case 12: /* Ctrl-L */
            return ACTION_REDRAW;

        case KEY_RESIZE:
            return ACTION_RESIZE;

        default:
            return ACTION_NONE;
    }
}
