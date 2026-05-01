/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "input.h"
#include "md_parser.h"
#include "pager.h"
#include "remedy.h"
#include "render_line.h"
#include "renderer.h"

#include <curses.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static sig_atomic_t volatile g_running = 1;

static void signal_handler(int sig)
{
    (void) sig;
    g_running = 0;
}

static int get_terminal_width(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return ws.ws_col;
    }
    return 80;
}

static void print_usage(const char *program_name)
{
    fprintf(stderr, "Usage: %s [options] [file.md]\n", program_name);
    fprintf(stderr, "  A full-featured markdown pager for the terminal.\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h, --help       Show this help message\n");
    fprintf(stderr, "  -v, --version    Show version\n\n");
    fprintf(stderr, "If no file is given, reads from stdin.\n");
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    const char *filename = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("remedy %s\n", REMEDY_VERSION);
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            filename = argv[i];
        }
    }

    char *text      = NULL;
    size_t text_len = 0;
    int ret;

    if (filename) {
        ret = md_parser_read_file(filename, &text, &text_len);
        if (ret != SUCCESS) {
            fprintf(stderr, "Error: could not read file '%s'\n", filename);
            return 1;
        }
    } else if (!isatty(STDIN_FILENO)) {
        ret = md_parser_read_stdin(&text, &text_len);
        if (ret != SUCCESS) {
            fprintf(stderr, "Error: could not read from stdin\n");
            return 1;
        }
    } else {
        fprintf(stderr, "Error: no input file and stdin is a terminal\n");
        print_usage(argv[0]);
        return 1;
    }

    if (text_len == 0) {
        fprintf(stderr, "Error: empty input\n");
        free(text);
        return 1;
    }

    cmark_node *doc = md_parser_parse(text, text_len);
    if (!doc) {
        fprintf(stderr, "Error: failed to parse markdown\n");
        free(text);
        return 1;
    }

    int term_width = get_terminal_width();

    line_buffer_t buf;
    toc_t toc;

    if (line_buffer_init(&buf) != SUCCESS || toc_init(&toc) != SUCCESS) {
        fprintf(stderr, "Error: memory allocation failed\n");
        md_parser_free(doc);
        free(text);
        return 1;
    }

    /* Resolve base directory for relative image paths */
    char *base_dir = NULL;
    if (filename) {
        base_dir = strdup(filename);
        if (base_dir) {
            char *last_slash = strrchr(base_dir, '/');
            if (last_slash) {
                *last_slash = '\0';
            } else {
                free(base_dir);
                base_dir = strdup(".");
            }
        }
    }

    ret = renderer_render(doc, term_width, &buf, &toc, base_dir);
    if (ret != SUCCESS) {
        fprintf(stderr, "Error: rendering failed\n");
        line_buffer_destroy(&buf);
        toc_destroy(&toc);
        md_parser_free(doc);
        free(text);
        return 1;
    }

    renderer_word_wrap(&buf, term_width);

    /* For stdin pipe mode, reopen /dev/tty for ncurses input */
    FILE *tty_fp = NULL;
    if (!filename && !isatty(STDIN_FILENO)) {
        tty_fp = fopen("/dev/tty", "r");
        if (!tty_fp) {
            fprintf(stderr, "Error: cannot open /dev/tty for keyboard input\n");
            line_buffer_destroy(&buf);
            toc_destroy(&toc);
            md_parser_free(doc);
            free(text);
            return 1;
        }

        /* Redirect stdin to /dev/tty so ncurses reads from the real terminal */
        dup2(fileno(tty_fp), STDIN_FILENO);
    }

    pager_state_t pager;

    ret = pager_init(&pager, &buf, &toc, filename);
    if (ret != SUCCESS) {
        fprintf(stderr, "Error: failed to initialize pager\n");
        if (tty_fp) {
            fclose(tty_fp);
        }
        line_buffer_destroy(&buf);
        toc_destroy(&toc);
        md_parser_free(doc);
        free(text);
        return 1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    pager_draw(&pager);

    while (g_running) {
        input_action_t action = input_get_action();

        switch (action) {
            case ACTION_QUIT:
                g_running = 0;
                break;

            case ACTION_SCROLL_DOWN:
                pager_scroll_down(&pager, 1);
                pager_draw(&pager);
                break;

            case ACTION_SCROLL_UP:
                pager_scroll_up(&pager, 1);
                pager_draw(&pager);
                break;

            case ACTION_PAGE_DOWN:
                pager_page_down(&pager);
                pager_draw(&pager);
                break;

            case ACTION_PAGE_UP:
                pager_page_up(&pager);
                pager_draw(&pager);
                break;

            case ACTION_HALF_PAGE_DOWN:
                pager_half_page_down(&pager);
                pager_draw(&pager);
                break;

            case ACTION_HALF_PAGE_UP:
                pager_half_page_up(&pager);
                pager_draw(&pager);
                break;

            case ACTION_HOME:
                pager_home(&pager);
                pager_draw(&pager);
                break;

            case ACTION_END:
                pager_end(&pager);
                pager_draw(&pager);
                break;

            case ACTION_SEARCH_FORWARD:
                pager_search_prompt(&pager, true);
                pager_draw(&pager);
                break;

            case ACTION_SEARCH_BACKWARD:
                pager_search_prompt(&pager, false);
                pager_draw(&pager);
                break;

            case ACTION_SEARCH_NEXT:
                pager_search_next(&pager, true);
                pager_draw(&pager);
                break;

            case ACTION_SEARCH_PREV:
                pager_search_next(&pager, false);
                pager_draw(&pager);
                break;

            case ACTION_TOGGLE_TOC:
                pager_show_toc(&pager);
                pager_draw(&pager);
                break;

            case ACTION_HELP:
                pager_show_help(&pager);
                pager_draw(&pager);
                break;

            case ACTION_RESIZE:
                pager_handle_resize(&pager);
                pager_draw(&pager);
                break;

            case ACTION_REDRAW:
                clearok(stdscr, TRUE);
                pager_draw(&pager);
                break;

            case ACTION_NONE:
                napms(20);
                break;

            default:
                break;
        }
    }

    pager_shutdown(&pager);

    if (tty_fp) {
        fclose(tty_fp);
    }

    line_buffer_destroy(&buf);
    toc_destroy(&toc);
    md_parser_free(doc);
    free(text);
    free(base_dir);

    return 0;
}
