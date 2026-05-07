/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pager.h"

#include "kitty_graphics.h"
#include "renderer.h"
#include "sanitize.h"

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

static void draw_span(WINDOW *win, const styled_span_t *span)
{
    attr_t attrs = A_NORMAL;

    if (span->style & STYLE_BOLD) {
        attrs |= A_BOLD;
    }
    if (span->style & STYLE_ITALIC) {
        attrs |= A_ITALIC;
    }
    if (span->style & STYLE_STRIKETHROUGH) {
        attrs |= A_DIM;
    }
    if (span->style & STYLE_CODE) {
        attrs |= A_REVERSE;
    }
    if (span->style & STYLE_LINK) {
        attrs |= A_UNDERLINE;
    }

    wattron(win, COLOR_PAIR(span->color) | attrs);

    if (span->text) {
        waddstr(win, span->text);
    }

    wattroff(win, COLOR_PAIR(span->color) | attrs);
}

static void draw_status_bar(pager_state_t *state)
{
    int y = state->term_height - 1;

    attron(COLOR_PAIR(COLOR_STATUS_BAR));

    /* Fill the entire status bar line with spaces for consistent background */
    move(y, 0);
    for (int i = 0; i < state->term_width; i++) {
        addch(' ');
    }

    const char *name = state->filename ? state->filename : "[stdin]";

    int percent = 0;
    if (state->buf->line_count > 0) {
        size_t visible_end = state->scroll_offset + (size_t) (state->term_height - 1);
        if (visible_end >= state->buf->line_count) {
            percent = 100;
        } else if (state->scroll_offset == 0) {
            percent = 0;
        } else {
            percent = (int) (state->scroll_offset * 100 / state->buf->line_count);
        }
    }

    const char *health = "";
    if (state->lint) {
        if (state->lint->count == 0) {
            health = "  |  healthy";
        } else if (state->lint->count == 1) {
            health = "  |  1 issue";
        } else {
            health = "  |  issues";
        }
    }

    char left[256];
    if (state->lint && state->lint->count > 1) {
        snprintf(left,
                 sizeof(left),
                 " %s  |  Line %zu/%zu  |  %d%%  |  %zu issues",
                 name,
                 state->scroll_offset + 1,
                 state->buf->line_count,
                 percent,
                 state->lint->count);
    } else {
        snprintf(left,
                 sizeof(left),
                 " %s  |  Line %zu/%zu  |  %d%%%s",
                 name,
                 state->scroll_offset + 1,
                 state->buf->line_count,
                 percent,
                 health);
    }

    const char *hints_full  = "q:quit  /:search  t:toc  D:diagnose  r:reload  h:help";
    const char *hints_short = "h:help";
    int left_len            = (int) strlen(left);

    mvaddnstr(y, 0, left, state->term_width);

    /* Right-align help hints; fall back to short version on narrow terminals */
    int full_len  = (int) strlen(hints_full);
    int short_len = (int) strlen(hints_short);

    if (left_len + full_len + 2 < state->term_width) {
        mvaddnstr(y, state->term_width - full_len - 1, hints_full, full_len);
    } else if (left_len + short_len + 2 < state->term_width) {
        mvaddnstr(y, state->term_width - short_len - 1, hints_short, short_len);
    }

    attroff(COLOR_PAIR(COLOR_STATUS_BAR));
}

void pager_init_colors(void)
{
    start_color();
    use_default_colors();

    init_pair(COLOR_NORMAL, -1, -1);
    init_pair(COLOR_HEADING1, COLOR_MAGENTA, -1);
    init_pair(COLOR_HEADING2, COLOR_GREEN, -1);
    init_pair(COLOR_HEADING3, COLOR_CYAN, -1);
    init_pair(COLOR_HEADING4, COLOR_YELLOW, -1);
    init_pair(COLOR_HEADING5, COLOR_BLUE, -1);
    init_pair(COLOR_HEADING6, COLOR_WHITE, -1);
    init_pair(COLOR_CODE, COLOR_YELLOW, -1);
    init_pair(COLOR_CODE_BLOCK, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_BLOCKQUOTE, COLOR_CYAN, -1);
    init_pair(COLOR_LINK, COLOR_BLUE, -1);
    init_pair(COLOR_LINK_URL, COLOR_CYAN, -1);
    init_pair(COLOR_HR, COLOR_WHITE, -1);
    init_pair(COLOR_TABLE_BORDER, COLOR_WHITE, -1);
    init_pair(COLOR_SEARCH_HIT, COLOR_BLACK, COLOR_YELLOW);
    init_pair(COLOR_LIST_BULLET, COLOR_CYAN, -1);
    init_pair(COLOR_STATUS_BAR, COLOR_BLACK, COLOR_WHITE);
    init_pair(COLOR_SYN_KEYWORD, COLOR_MAGENTA, -1);
    init_pair(COLOR_SYN_STRING, COLOR_GREEN, -1);
    init_pair(COLOR_SYN_COMMENT, COLOR_CYAN, -1);
    init_pair(COLOR_SYN_NUMBER, COLOR_YELLOW, -1);
    init_pair(COLOR_SYN_TYPE, COLOR_GREEN, -1);
    init_pair(COLOR_SYN_FUNCTION, COLOR_BLUE, -1);
    init_pair(COLOR_SYN_PREPROC, COLOR_RED, -1);
}

int pager_init(pager_state_t *state, line_buffer_t *buf, toc_t *toc, const char *filename)
{
    if (!state || !buf) {
        return FAILURE;
    }

    memset(state, 0, sizeof(pager_state_t));

    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    curs_set(0);

    pager_init_colors();

    state->buf      = buf;
    state->toc      = toc;
    state->filename = filename;

    getmaxyx(stdscr, state->term_height, state->term_width);

    return SUCCESS;
}

void pager_shutdown(pager_state_t *state)
{
    (void) state;
    endwin();
}

void pager_draw(pager_state_t *state)
{
    erase();

    int visible_lines = state->term_height - 1;

    for (int row = 0; row < visible_lines; row++) {
        size_t line_idx = state->scroll_offset + (size_t) row;

        if (line_idx >= state->buf->line_count) {
            mvaddch(row, PAGER_MARGIN, '~');
            continue;
        }

        rendered_line_t *line = &state->buf->lines[line_idx];

        move(row, PAGER_MARGIN);

        /* Draw indent */
        int col = PAGER_MARGIN;
        if (line->indent > 0) {
            bool is_blockquote = false;
            for (size_t s = 0; s < line->span_count; s++) {
                if (line->spans[s].style & STYLE_BLOCKQUOTE) {
                    is_blockquote = true;
                    break;
                }
            }

            for (int q = 0; q < line->indent / 2; q++) {
                if (is_blockquote && q == 0) {
                    attron(COLOR_PAIR(COLOR_BLOCKQUOTE) | A_BOLD);
                    addstr("\xe2\x94\x82 ");
                    attroff(COLOR_PAIR(COLOR_BLOCKQUOTE) | A_BOLD);
                } else {
                    addstr("  ");
                }
                col += 2;
            }

            if (line->indent % 2 == 1) {
                addch(' ');
                col++;
            }
        }

        /* Draw spans (skip image marker lines starting with \x01) */
        for (size_t s = 0; s < line->span_count; s++) {
            if (col >= state->term_width - PAGER_MARGIN) {
                break;
            }
            if (line->spans[s].text && line->spans[s].text[0] == '\x01') {
                continue;
            }
            draw_span(stdscr, &line->spans[s]);
            col += line->spans[s].display_width;
        }
    }

    draw_status_bar(state);
    refresh();

    /*
     * Post-draw pass: overlay raw escape sequences for features ncurses
     * can't handle (strikethrough SGR 9, OSC 8 hyperlinks). This runs
     * after refresh() so ncurses' output is fully flushed to the terminal.
     */
    for (int row = 0; row < visible_lines; row++) {
        size_t line_idx = state->scroll_offset + (size_t) row;
        if (line_idx >= state->buf->line_count) {
            break;
        }

        rendered_line_t *line = &state->buf->lines[line_idx];
        int col               = line->indent + PAGER_MARGIN;

        for (size_t s = 0; s < line->span_count; s++) {
            styled_span_t *span = &line->spans[s];
            bool needs_raw      = (span->style & STYLE_STRIKETHROUGH) || span->link_url;

            if (needs_raw && span->text && col >= 0 && col < state->term_width) {
                char *safe_text = sanitize_for_escape(span->text);
                if (!safe_text) {
                    col += span->display_width;
                    continue;
                }

                printf("\033[%d;%dH", row + 1, col + 1);

                if (span->link_url && is_safe_for_escape(span->link_url)) {
                    printf("\033]8;;%s\033\\", span->link_url);
                    printf("\033[4;34m");
                }
                if (span->style & STYLE_STRIKETHROUGH) {
                    printf("\033[9m");
                }

                printf("%s", safe_text);
                free(safe_text);

                if (span->style & STYLE_STRIKETHROUGH) {
                    printf("\033[29m");
                }
                if (span->link_url && is_safe_for_escape(span->link_url)) {
                    printf("\033[24;39m");
                    printf("\033]8;;\033\\");
                }

                printf("\033[0m");
            }

            col += span->display_width;
        }
    }

    /*
     * Post-draw: Kitty graphics images.
     * Delete old placements, then re-place. Images that have already
     * been transmitted are placed instantly without re-sending data.
     */
    if (kitty_graphics_supported()) {
        kitty_graphics_delete_placements();

        for (int row = 0; row < visible_lines; row++) {
            size_t line_idx = state->scroll_offset + (size_t) row;
            if (line_idx >= state->buf->line_count) {
                break;
            }

            rendered_line_t *line = &state->buf->lines[line_idx];
            if (line->span_count < 1 || !line->spans[0].text || strncmp(line->spans[0].text, "\x01IMAGE:", 7) != 0) {
                continue;
            }

            int img_max_width     = state->term_width - line->indent;
            int img_max_height    = 15;
            int rows_until_bottom = visible_lines - row;
            if (img_max_height > rows_until_bottom) {
                img_max_height = rows_until_bottom;
            }
            if (img_max_height < 1) {
                continue;
            }

            const char *img_path = line->spans[0].text + 7;
            int image_id         = (int) (line_idx % MAX_TRACKED_IMAGES) + 1;
            int idx              = image_id - 1;

            printf("\033[%d;%dH", row + 1, line->indent + PAGER_MARGIN + 1);

            if (!state->images_transmitted[idx]) {
                int actual_cols = 0;
                int actual_rows = 0;
                kitty_graphics_display(img_path, image_id, img_max_width, img_max_height, &actual_cols, &actual_rows);
                state->images_transmitted[idx] = true;
                state->images_cols[idx]        = actual_cols;
                state->images_rows[idx]        = actual_rows;
            } else {
                kitty_graphics_place(image_id, state->images_cols[idx], state->images_rows[idx]);
            }
        }
    }

    fflush(stdout);
}

void pager_scroll_down(pager_state_t *state, int lines)
{
    size_t max_offset = 0;
    if (state->buf->line_count > (size_t) (state->term_height - 1)) {
        max_offset = state->buf->line_count - (size_t) (state->term_height - 1);
    }

    state->scroll_offset += (size_t) lines;
    if (state->scroll_offset > max_offset) {
        state->scroll_offset = max_offset;
    }
}

void pager_scroll_up(pager_state_t *state, int lines)
{
    if ((size_t) lines > state->scroll_offset) {
        state->scroll_offset = 0;
    } else {
        state->scroll_offset -= (size_t) lines;
    }
}

void pager_page_down(pager_state_t *state)
{
    pager_scroll_down(state, state->term_height - 2);
}

void pager_page_up(pager_state_t *state)
{
    pager_scroll_up(state, state->term_height - 2);
}

void pager_half_page_down(pager_state_t *state)
{
    pager_scroll_down(state, (state->term_height - 1) / 2);
}

void pager_half_page_up(pager_state_t *state)
{
    pager_scroll_up(state, (state->term_height - 1) / 2);
}

void pager_home(pager_state_t *state)
{
    state->scroll_offset = 0;
}

void pager_end(pager_state_t *state)
{
    if (state->buf->line_count > (size_t) (state->term_height - 1)) {
        state->scroll_offset = state->buf->line_count - (size_t) (state->term_height - 1);
    } else {
        state->scroll_offset = 0;
    }
}

void pager_handle_resize(pager_state_t *state)
{
    endwin();
    refresh();
    getmaxyx(stdscr, state->term_height, state->term_width);

    renderer_strip_continuations(state->buf);
    renderer_word_wrap(state->buf, state->term_width - PAGER_MARGIN * 2);

    /* Force image retransmission since dimensions changed */
    memset(state->images_transmitted, 0, sizeof(state->images_transmitted));

    size_t max_offset = 0;
    if (state->buf->line_count > (size_t) (state->term_height - 1)) {
        max_offset = state->buf->line_count - (size_t) (state->term_height - 1);
    }
    if (state->scroll_offset > max_offset) {
        state->scroll_offset = max_offset;
    }
}

void pager_search_prompt(pager_state_t *state, bool forward)
{
    kitty_graphics_delete_placements();

    int y = state->term_height - 1;

    /* Draw search prompt on the status bar */
    attron(COLOR_PAIR(COLOR_STATUS_BAR));
    move(y, 0);
    for (int i = 0; i < state->term_width; i++) {
        addch(' ');
    }
    mvaddch(y, 0, forward ? '/' : '?');
    attroff(COLOR_PAIR(COLOR_STATUS_BAR));

    /* Enable cursor and echo for input */
    curs_set(1);
    nodelay(stdscr, FALSE);
    move(y, 1);

    char query[SEARCH_QUERY_MAX];
    int pos = 0;
    memset(query, 0, sizeof(query));

    while (1) {
        int ch = getch();

        if (ch == '\n' || ch == '\r') {
            break;
        } else if (ch == 27 || ch == ERR) {
            /* ESC — cancel search */
            pos = 0;
            break;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (pos > 0) {
                pos--;
                query[pos] = '\0';
                mvaddch(y, pos + 1, ' ');
                move(y, pos + 1);
            }
        } else if (pos < SEARCH_QUERY_MAX - 1 && ch >= 32 && ch < 127) {
            query[pos++] = (char) ch;
            query[pos]   = '\0';
            addch(ch);
        }
    }

    curs_set(0);
    nodelay(stdscr, TRUE);

    if (pos > 0) {
        /* Clear old highlights */
        search_clear_highlights(state->buf);

        strncpy(state->search.query, query, SEARCH_QUERY_MAX - 1);
        state->search.query[SEARCH_QUERY_MAX - 1] = '\0';
        state->search.active                      = true;
        state->search.forward                     = forward;

        /* Apply highlights */
        search_highlight(&state->search, state->buf);

        /* Jump to first match */
        if (search_find(&state->search, state->buf, state->scroll_offset, forward)) {
            state->scroll_offset = state->search.match_line;

            size_t max_offset = 0;
            if (state->buf->line_count > (size_t) (state->term_height - 1)) {
                max_offset = state->buf->line_count - (size_t) (state->term_height - 1);
            }
            if (state->scroll_offset > max_offset) {
                state->scroll_offset = max_offset;
            }
        }
    }
}

void pager_search_next(pager_state_t *state, bool forward)
{
    if (!state->search.active || !state->search.query[0]) {
        return;
    }

    size_t start = forward ? state->search.match_line + 1 : state->search.match_line;

    if (search_find(&state->search, state->buf, start, forward)) {
        state->scroll_offset = state->search.match_line;

        size_t max_offset = 0;
        if (state->buf->line_count > (size_t) (state->term_height - 1)) {
            max_offset = state->buf->line_count - (size_t) (state->term_height - 1);
        }
        if (state->scroll_offset > max_offset) {
            state->scroll_offset = max_offset;
        }
    }
}

void pager_show_toc(pager_state_t *state)
{
    if (!state->toc || state->toc->count == 0) {
        return;
    }

    kitty_graphics_delete_placements();

    int toc_width  = state->term_width * 3 / 4;
    int toc_height = (int) state->toc->count + 4;

    if (toc_width < 30) {
        toc_width = 30;
    }
    if (toc_height > state->term_height - 2) {
        toc_height = state->term_height - 2;
    }

    int start_y = (state->term_height - toc_height) / 2;
    int start_x = (state->term_width - toc_width) / 2;

    WINDOW *toc_win = newwin(toc_height, toc_width, start_y, start_x);
    if (!toc_win) {
        return;
    }

    keypad(toc_win, TRUE);
    nodelay(toc_win, FALSE);

    int cursor       = 0;
    int scroll       = 0;
    int content_rows = toc_height - 4;
    bool selected    = false;

    while (1) {
        werase(toc_win);
        box(toc_win, 0, 0);

        /* Title */
        wattron(toc_win, A_BOLD);
        mvwaddstr(toc_win, 1, 2, "Table of Contents");
        wattroff(toc_win, A_BOLD);
        mvwhline(toc_win, 2, 1, ACS_HLINE, toc_width - 2);

        /* Entries */
        for (int row = 0; row < content_rows; row++) {
            int entry_idx = scroll + row;
            if (entry_idx >= (int) state->toc->count) {
                break;
            }

            toc_entry_t *entry = &state->toc->entries[entry_idx];
            int indent_chars   = (entry->level - 1) * 2;

            if (entry_idx == cursor) {
                wattron(toc_win, A_REVERSE);
            }

            wmove(toc_win, row + 3, 1);
            for (int i = 0; i < toc_width - 2; i++) {
                waddch(toc_win, ' ');
            }

            wmove(toc_win, row + 3, 2 + indent_chars);
            int max_text_len = toc_width - 4 - indent_chars;
            if (max_text_len > 0) {
                waddnstr(toc_win, entry->text, max_text_len);
            }

            if (entry_idx == cursor) {
                wattroff(toc_win, A_REVERSE);
            }
        }

        wrefresh(toc_win);

        int ch = wgetch(toc_win);
        switch (ch) {
            case 'j':
            case KEY_DOWN:
                if (cursor < (int) state->toc->count - 1) {
                    cursor++;
                    if (cursor - scroll >= content_rows) {
                        scroll++;
                    }
                }
                break;

            case 'k':
            case KEY_UP:
                if (cursor > 0) {
                    cursor--;
                    if (cursor < scroll) {
                        scroll = cursor;
                    }
                }
                break;

            case '\n':
            case '\r':
                selected = true;
                goto toc_done;

            case 'q':
            case 27: /* ESC */
            case 't':
                goto toc_done;

            default:
                break;
        }
    }

toc_done:
    delwin(toc_win);

    if (selected && cursor < (int) state->toc->count) {
        state->scroll_offset = state->toc->entries[cursor].line_index;

        size_t max_offset = 0;
        if (state->buf->line_count > (size_t) (state->term_height - 1)) {
            max_offset = state->buf->line_count - (size_t) (state->term_height - 1);
        }
        if (state->scroll_offset > max_offset) {
            state->scroll_offset = max_offset;
        }
    }
}

void pager_show_help(pager_state_t *state)
{
    kitty_graphics_delete_placements();

    /* clang-format off */
    static const char *help_lines[] = {
        "Navigation",
        "  j / Down / Enter    Scroll down one line",
        "  k / Up              Scroll up one line",
        "  f / Space / PgDn    Page down",
        "  b / PgUp            Page up",
        "  d / Ctrl-D          Half page down",
        "  u / Ctrl-U          Half page up",
        "  g / Home            Top of document",
        "  G / End             Bottom of document",
        "",
        "Search",
        "  /                   Search forward",
        "  ?                   Search backward",
        "  n                   Next match",
        "  N                   Previous match",
        "",
        "Features",
        "  t                   Table of contents",
        "  D                   Diagnose markdown",
        "  r                   Reload document",
        "  Ctrl-L              Redraw screen",
        "  h                   This help screen",
        "  q / ESC             Quit",
        NULL
    };
    /* clang-format on */

    int num_lines = 0;
    int max_width = 0;
    for (int i = 0; help_lines[i]; i++) {
        num_lines++;
        int len = (int) strlen(help_lines[i]);
        if (len > max_width) {
            max_width = len;
        }
    }

    int win_height = num_lines + 4;
    int win_width  = max_width + 6;

    if (win_height > state->term_height - 2) {
        win_height = state->term_height - 2;
    }
    if (win_width > state->term_width - 4) {
        win_width = state->term_width - 4;
    }

    int start_y = (state->term_height - win_height) / 2;
    int start_x = (state->term_width - win_width) / 2;

    WINDOW *help_win = newwin(win_height, win_width, start_y, start_x);
    if (!help_win) {
        return;
    }

    keypad(help_win, TRUE);

    werase(help_win);
    box(help_win, 0, 0);

    wattron(help_win, A_BOLD);
    mvwaddstr(help_win, 1, 3, "remedy - Keybindings");
    wattroff(help_win, A_BOLD);
    mvwhline(help_win, 2, 1, ACS_HLINE, win_width - 2);

    for (int i = 0; i < num_lines && i + 3 < win_height - 1; i++) {
        if (help_lines[i][0] != ' ' && help_lines[i][0] != '\0') {
            wattron(help_win, A_BOLD | COLOR_PAIR(COLOR_HEADING2));
            mvwaddnstr(help_win, i + 3, 3, help_lines[i], win_width - 6);
            wattroff(help_win, A_BOLD | COLOR_PAIR(COLOR_HEADING2));
        } else {
            mvwaddnstr(help_win, i + 3, 3, help_lines[i], win_width - 6);
        }
    }

    wrefresh(help_win);

    nodelay(help_win, FALSE);
    wgetch(help_win);

    delwin(help_win);
}

void pager_show_diagnose(pager_state_t *state)
{
    if (!state->lint) {
        return;
    }

    kitty_graphics_delete_placements();

    int win_width  = state->term_width * 3 / 4;
    int win_height = (int) state->lint->count + 6;

    if (win_width < 40) {
        win_width = 40;
    }
    if (win_height < 8) {
        win_height = 8;
    }
    if (win_height > state->term_height - 2) {
        win_height = state->term_height - 2;
    }
    if (win_width > state->term_width - 4) {
        win_width = state->term_width - 4;
    }

    int start_y = (state->term_height - win_height) / 2;
    int start_x = (state->term_width - win_width) / 2;

    WINDOW *win = newwin(win_height, win_width, start_y, start_x);
    if (!win) {
        return;
    }

    keypad(win, TRUE);

    int cursor       = 0;
    int scroll       = 0;
    int content_rows = win_height - 6;

    while (1) {
        werase(win);
        box(win, 0, 0);

        /* Title */
        wattron(win, A_BOLD);
        if (state->lint->count == 0) {
            mvwaddstr(win, 1, 3, "Diagnosis: healthy");
        } else {
            char title[64];
            snprintf(title, sizeof(title), "Diagnosis: %zu issue%s found", state->lint->count, state->lint->count == 1 ? "" : "s");
            mvwaddstr(win, 1, 3, title);
        }
        wattroff(win, A_BOLD);
        mvwhline(win, 2, 1, ACS_HLINE, win_width - 2);

        if (state->lint->count == 0) {
            wattron(win, COLOR_PAIR(COLOR_HEADING2));
            mvwaddstr(win, 4, 3, "No issues found. Your markdown looks good!");
            wattroff(win, COLOR_PAIR(COLOR_HEADING2));
        } else {
            /* Column headers */
            wattron(win, A_DIM);
            mvwaddstr(win, 3, 3, "Line  Sev  Description");
            wattroff(win, A_DIM);
            mvwhline(win, 4, 1, ACS_HLINE, win_width - 2);

            for (int row = 0; row < content_rows; row++) {
                int idx = scroll + row;
                if (idx >= (int) state->lint->count) {
                    break;
                }

                lint_issue_t *issue = &state->lint->issues[idx];

                if (idx == cursor) {
                    wattron(win, A_REVERSE);
                }

                wmove(win, row + 5, 1);
                for (int i = 0; i < win_width - 2; i++) {
                    waddch(win, ' ');
                }

                /* Line number */
                char line_str[8];
                snprintf(line_str, sizeof(line_str), "%4d", issue->line);
                mvwaddstr(win, row + 5, 3, line_str);

                /* Severity */
                if (issue->severity == LINT_ERROR) {
                    wattron(win, COLOR_PAIR(COLOR_HEADING1) | A_BOLD);
                    mvwaddstr(win, row + 5, 9, "ERR");
                    wattroff(win, COLOR_PAIR(COLOR_HEADING1) | A_BOLD);
                } else {
                    wattron(win, COLOR_PAIR(COLOR_HEADING4));
                    mvwaddstr(win, row + 5, 9, "WRN");
                    wattroff(win, COLOR_PAIR(COLOR_HEADING4));
                }

                /* Message */
                int max_msg = win_width - 16;
                if (max_msg > 0 && issue->message) {
                    mvwaddnstr(win, row + 5, 14, issue->message, max_msg);
                }

                if (idx == cursor) {
                    wattroff(win, A_REVERSE);
                }
            }
        }

        wrefresh(win);

        nodelay(win, FALSE);
        int ch = wgetch(win);

        switch (ch) {
            case 'j':
            case KEY_DOWN:
                if (cursor < (int) state->lint->count - 1) {
                    cursor++;
                    if (cursor - scroll >= content_rows) {
                        scroll++;
                    }
                }
                break;

            case 'k':
            case KEY_UP:
                if (cursor > 0) {
                    cursor--;
                    if (cursor < scroll) {
                        scroll = cursor;
                    }
                }
                break;

            case '\n':
            case '\r':
                /* Jump to the issue's source line */
                if (cursor < (int) state->lint->count) {
                    int target_line = state->lint->issues[cursor].line;
                    size_t target   = target_line > 1 ? (size_t) (target_line - 1) : 0;
                    if (target >= state->buf->line_count) {
                        target = state->buf->line_count > 0 ? state->buf->line_count - 1 : 0;
                    }
                    state->scroll_offset = target;

                    size_t max_offset = 0;
                    if (state->buf->line_count > (size_t) (state->term_height - 1)) {
                        max_offset = state->buf->line_count - (size_t) (state->term_height - 1);
                    }
                    if (state->scroll_offset > max_offset) {
                        state->scroll_offset = max_offset;
                    }
                }
                goto diag_done;

            case 'q':
            case 27:
            case 'd':
                goto diag_done;

            default:
                break;
        }
    }

diag_done:
    delwin(win);
}
