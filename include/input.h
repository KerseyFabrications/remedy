/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INPUT_H
#define INPUT_H

typedef enum {
    ACTION_NONE,
    ACTION_QUIT,
    ACTION_SCROLL_DOWN,
    ACTION_SCROLL_UP,
    ACTION_PAGE_DOWN,
    ACTION_PAGE_UP,
    ACTION_HALF_PAGE_DOWN,
    ACTION_HALF_PAGE_UP,
    ACTION_HOME,
    ACTION_END,
    ACTION_SEARCH_FORWARD,
    ACTION_SEARCH_BACKWARD,
    ACTION_SEARCH_NEXT,
    ACTION_SEARCH_PREV,
    ACTION_TOGGLE_TOC,
    ACTION_DIAGNOSE,
    ACTION_HELP,
    ACTION_RELOAD,
    ACTION_REDRAW,
    ACTION_RESIZE,
} input_action_t;

/**
 * @brief Read a keypress and return the corresponding action
 *
 * Uses ncurses getch() in non-blocking mode.
 *
 * @return The action corresponding to the key pressed, or ACTION_NONE
 */
input_action_t input_get_action(void);

#endif /* INPUT_H */
