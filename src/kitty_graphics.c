/*
 * remedy - A full-featured markdown pager for modern Linux terminals
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "kitty_graphics.h"

#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *base64_encode(const unsigned char *data, size_t len, size_t *out_len)
{
    size_t encoded_len = 4 * ((len + 2) / 3);
    char *encoded      = malloc(encoded_len + 1);
    if (!encoded) {
        return NULL;
    }

    size_t j = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t octet_a = data[i];
        uint32_t octet_b = (i + 1 < len) ? data[i + 1] : 0;
        uint32_t octet_c = (i + 2 < len) ? data[i + 2] : 0;
        uint32_t triple  = (octet_a << 16) | (octet_b << 8) | octet_c;

        encoded[j++] = b64_table[(triple >> 18) & 0x3F];
        encoded[j++] = b64_table[(triple >> 12) & 0x3F];
        encoded[j++] = (i + 1 < len) ? b64_table[(triple >> 6) & 0x3F] : '=';
        encoded[j++] = (i + 2 < len) ? b64_table[triple & 0x3F] : '=';
    }

    encoded[j] = '\0';
    if (out_len) {
        *out_len = j;
    }
    return encoded;
}

bool kitty_graphics_supported(void)
{
    const char *term_program = getenv("TERM_PROGRAM");
    if (term_program) {
        if (strcasecmp(term_program, "kitty") == 0) {
            return true;
        }
        if (strcasecmp(term_program, "ghostty") == 0) {
            return true;
        }
        if (strcasecmp(term_program, "WezTerm") == 0) {
            return true;
        }
    }

    const char *term = getenv("TERM");
    if (term && strstr(term, "kitty")) {
        return true;
    }

    return false;
}

static unsigned char *read_file_binary(const char *path, size_t *out_len)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0 || file_size > 10 * 1024 * 1024) {
        fclose(fp);
        return NULL;
    }

    unsigned char *data = malloc((size_t) file_size);
    if (!data) {
        fclose(fp);
        return NULL;
    }

    size_t bytes_read = fread(data, 1, (size_t) file_size, fp);
    fclose(fp);

    *out_len = bytes_read;
    return data;
}

static void compute_display_size(int img_w, int img_h, int max_width, int max_height, int *out_cols, int *out_rows)
{
    *out_cols = max_width;
    *out_rows = max_height;

    if (img_w > 0 && img_h > 0) {
        double aspect     = (double) img_w / (double) img_h;
        double cell_ratio = 2.0;

        int fit_by_width_cols = max_width;
        int fit_by_width_rows = (int) ((double) fit_by_width_cols / (aspect * cell_ratio));

        int fit_by_height_rows = max_height;
        int fit_by_height_cols = (int) ((double) fit_by_height_rows * aspect * cell_ratio);

        if (fit_by_width_rows <= max_height) {
            *out_cols = fit_by_width_cols;
            *out_rows = fit_by_width_rows;
        } else {
            *out_cols = fit_by_height_cols;
            *out_rows = fit_by_height_rows;
        }

        if (*out_cols > max_width) {
            *out_cols = max_width;
        }
        if (*out_rows > max_height) {
            *out_rows = max_height;
        }
        if (*out_cols < 1) {
            *out_cols = 1;
        }
        if (*out_rows < 1) {
            *out_rows = 1;
        }
    }
}

int kitty_graphics_display(const char *path, int image_id, int max_width, int max_height, int *out_cols, int *out_rows)
{
    if (!path || !kitty_graphics_supported()) {
        return 0;
    }

    if (max_width < 10) {
        max_width = 10;
    }
    if (max_height < 3) {
        max_height = 3;
    }

    size_t file_len          = 0;
    unsigned char *file_data = read_file_binary(path, &file_len);
    if (!file_data) {
        return 0;
    }

    /* Decode any supported image format to raw RGBA pixels */
    int img_w             = 0;
    int img_h             = 0;
    int channels          = 0;
    unsigned char *pixels = stbi_load_from_memory(file_data, (int) file_len, &img_w, &img_h, &channels, 4);
    free(file_data);

    if (!pixels || img_w <= 0 || img_h <= 0) {
        if (pixels) {
            stbi_image_free(pixels);
        }
        return 0;
    }

    int display_cols = 0;
    int display_rows = 0;
    compute_display_size(img_w, img_h, max_width, max_height, &display_cols, &display_rows);

    size_t rgba_len = (size_t) img_w * (size_t) img_h * 4;
    size_t b64_len  = 0;
    char *b64_data  = base64_encode(pixels, rgba_len, &b64_len);
    stbi_image_free(pixels);

    if (!b64_data) {
        return 0;
    }

    /* Transmit raw RGBA (f=32) with pixel dimensions (s=width, v=height) */
    size_t chunk_size = 4096;
    size_t offset     = 0;

    while (offset < b64_len) {
        size_t remaining  = b64_len - offset;
        size_t this_chunk = remaining < chunk_size ? remaining : chunk_size;
        int more          = (offset + this_chunk < b64_len) ? 1 : 0;

        if (offset == 0) {
            printf("\033_Gf=32,s=%d,v=%d,a=t,i=%d,t=d,q=2,m=%d;", img_w, img_h, image_id, more);
        } else {
            printf("\033_Gm=%d;", more);
        }

        fwrite(b64_data + offset, 1, this_chunk, stdout);
        printf("\033\\");

        offset += this_chunk;
    }

    free(b64_data);

    printf("\033_Ga=p,i=%d,c=%d,r=%d,q=2\033\\", image_id, display_cols, display_rows);
    fflush(stdout);

    if (out_cols) {
        *out_cols = display_cols;
    }
    if (out_rows) {
        *out_rows = display_rows;
    }

    return display_rows;
}

void kitty_graphics_place(int image_id, int max_width, int max_height)
{
    if (max_width < 1) {
        max_width = 1;
    }
    if (max_height < 1) {
        max_height = 1;
    }

    printf("\033_Ga=p,i=%d,c=%d,r=%d,q=2\033\\", image_id, max_width, max_height);
    fflush(stdout);
}

void kitty_graphics_delete_placements(void)
{
    printf("\033_Ga=d,q=2\033\\");
    fflush(stdout);
}

int kitty_graphics_render(const char *path, const char *alt_text, const char *base_dir, int indent, line_buffer_t *buf)
{
    if (!buf) {
        return FAILURE;
    }

    /* Build full path */
    char full_path[4096];
    if (base_dir && path && path[0] != '/') {
        snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, path);
    } else if (path) {
        snprintf(full_path, sizeof(full_path), "%s", path);
    } else {
        full_path[0] = '\0';
    }

    bool can_display = false;
    if (kitty_graphics_supported() && full_path[0]) {
        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode)) {
            can_display = true;
        }
    }

    if (can_display) {
        int image_rows = 15;

        /* Add a marker line — the \x01 prefix tells pager_draw to skip display */
        rendered_line_t *line = line_buffer_append_line(buf);
        if (line) {
            line->indent = indent;
            char marker[4096 + 32];
            snprintf(marker, sizeof(marker), "\x01IMAGE:%s", full_path);

            styled_span_t span = styled_span_create(marker, STYLE_NORMAL, COLOR_NORMAL);
            rendered_line_append_span(line, &span);
        }

        /* Reserve blank rows for the image to occupy */
        for (int i = 0; i < image_rows - 1; i++) {
            line_buffer_append_line(buf);
        }
    } else {
        rendered_line_t *line = line_buffer_append_line(buf);
        if (line) {
            line->indent = indent;

            const char *display = (alt_text && alt_text[0]) ? alt_text : path;
            styled_span_t open  = styled_span_create("[image: ", STYLE_LINK, COLOR_LINK);
            rendered_line_append_span(line, &open);

            if (display) {
                styled_span_t text = styled_span_create(display, STYLE_LINK, COLOR_LINK);
                rendered_line_append_span(line, &text);
            }

            styled_span_t close = styled_span_create("]", STYLE_LINK, COLOR_LINK);
            rendered_line_append_span(line, &close);
        }
    }

    return SUCCESS;
}
