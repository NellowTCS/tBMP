#include "tbmp_ui.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int g_ui_ci_mode = 0;
static int g_ui_use_color = 0;
static TBmpUIAccent g_ui_accent = TBMP_UI_ACCENT_CYAN;

#define ANSI_RESET "\x1b[0m"
#define ANSI_BOLD "\x1b[1m"
#define ANSI_CYAN "\x1b[36m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_YELLOW "\x1b[33m"
#define ANSI_MAGENTA "\x1b[35m"
#define ANSI_RED "\x1b[31m"

#define UI_TERM_MIN_WIDTH 40
#define UI_TERM_MAX_WIDTH 120

static char g_step_message[192] = {0};
static int g_step_active = 0;

typedef enum UIStatusKind {
    UI_STATUS_SUCCESS,
    UI_STATUS_INFO,
    UI_STATUS_WARN,
    UI_STATUS_ERROR,
} UIStatusKind;

static int ui_term_width(void) {
    struct winsize ws;
    int width = 80;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        width = (int)ws.ws_col;

    if (width < UI_TERM_MIN_WIDTH)
        width = UI_TERM_MIN_WIDTH;
    if (width > UI_TERM_MAX_WIDTH)
        width = UI_TERM_MAX_WIDTH;
    return width;
}

static int ui_box_width(void) {
    /* Border is rendered as '+' + N chars + '+'. */
    int width = ui_term_width() - 2;
    if (width < 32)
        width = 32;
    return width;
}

static int ui_inner_width(void) {
    int inner = ui_box_width() - 2;
    if (inner < 20)
        inner = 20;
    return inner;
}

static const char *ui_color(const char *ansi) {
    return g_ui_use_color ? ansi : "";
}

static const char *ui_accent_color(void) {
    switch (g_ui_accent) {
    case TBMP_UI_ACCENT_GREEN:
        return ui_color(ANSI_GREEN);
    case TBMP_UI_ACCENT_YELLOW:
        return ui_color(ANSI_YELLOW);
    case TBMP_UI_ACCENT_MAGENTA:
        return ui_color(ANSI_MAGENTA);
    case TBMP_UI_ACCENT_CYAN:
    default:
        return ui_color(ANSI_CYAN);
    }
}

static const char *ui_status_color(UIStatusKind kind) {
    switch (kind) {
    case UI_STATUS_SUCCESS:
        return ui_color(ANSI_GREEN);
    case UI_STATUS_WARN:
        return ui_color(ANSI_YELLOW);
    case UI_STATUS_ERROR:
        return ui_color(ANSI_RED);
    case UI_STATUS_INFO:
    default:
        return ui_accent_color();
    }
}

static const char *ui_status_tag(UIStatusKind kind) {
    switch (kind) {
    case UI_STATUS_SUCCESS:
        return "OK";
    case UI_STATUS_WARN:
        return "!";
    case UI_STATUS_ERROR:
        return "ERROR";
    case UI_STATUS_INFO:
    default:
        return "*";
    }
}

static void ui_status_vprint(UIStatusKind kind, const char *fmt, va_list ap) {
    if (!g_ui_ci_mode) {
        printf("%s%s[%s]%s ", ui_color(ANSI_BOLD), ui_status_color(kind),
               ui_status_tag(kind), ui_color(ANSI_RESET));
    } else {
        printf("[%s] ", ui_status_tag(kind));
    }

    vprintf(fmt, ap);
    printf("\n");
}

static void ui_box_border(void) {
    if (g_ui_ci_mode)
        return;

    int width = ui_box_width();
    printf("%s+", ui_accent_color());
    for (int i = 0; i < width; i++)
        putchar('-');
    printf("+%s\n", ui_color(ANSI_RESET));
}

int tbmp_ui_is_ci(void) {
    return g_ui_ci_mode;
}

void tbmp_ui_init(int ci_mode) {
    g_ui_ci_mode = ci_mode;
    g_ui_use_color = (!g_ui_ci_mode) && isatty(STDOUT_FILENO);
}

void tbmp_ui_set_accent(TBmpUIAccent accent) {
    g_ui_accent = accent;
}

void tbmp_ui_banner(const char *title) {
    int width = ui_term_width();
    int inner = width - 2;
    int pad = 0;
    int title_len = (int)strlen(title);

    if (g_ui_ci_mode) {
        tbmp_ui_printlnf("== %s ==", title);
        return;
    }

    if (title_len < inner)
        pad = (inner - title_len) / 2;

    printf("%s%s", ui_color(ANSI_BOLD), ui_accent_color());
    printf("\n╔");
    for (int i = 0; i < inner; i++)
        fputs("═", stdout);
    printf("╗\n");

    printf("║");
    for (int i = 0; i < pad; i++)
        putchar(' ');
    printf("%s", title);
    for (int i = 0; i < inner - pad - title_len; i++)
        putchar(' ');
    printf("║\n");

    printf("╚");
    for (int i = 0; i < inner; i++)
        fputs("═", stdout);
    printf("╝\n%s", ui_color(ANSI_RESET));
}

void tbmp_ui_title(const char *title) {
    char banner[256];
    snprintf(banner, sizeof(banner), "tbmp :: %s", title);
    tbmp_ui_banner(banner);
}

void tbmp_ui_section(const char *title) {
    int width = ui_term_width();
    int title_len = (int)strlen(title);
    int side = (width - title_len - 2) / 2;

    if (g_ui_ci_mode) {
        tbmp_ui_printlnf("\n%s", title);
        return;
    }

    if (side < 2)
        side = 2;

    printf("%s%s", ui_color(ANSI_BOLD), ui_accent_color());
    for (int i = 0; i < side; i++)
        fputs("─", stdout);
    printf(" %s ", title);
    for (int i = 0; i < width - side - title_len - 2; i++)
        fputs("─", stdout);
    printf("%s\n", ui_color(ANSI_RESET));
}

void tbmp_ui_subsection(const char *title) {
    if (g_ui_ci_mode) {
        tbmp_ui_printlnf("- %s", title);
        return;
    }

    printf("%s%s▶ %s%s\n", ui_color(ANSI_BOLD), ui_accent_color(), title,
           ui_color(ANSI_RESET));
}

void tbmp_ui_printlnf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

void tbmp_ui_box_line(const char *text) {
    if (g_ui_ci_mode) {
        tbmp_ui_printlnf("%s", text);
        return;
    }

    int inner_width = ui_inner_width();
    char buf[256];
    size_t len = strlen(text);
    size_t pos = 0;

    if (len == 0) {
        snprintf(buf, sizeof(buf), "%-*s", inner_width, "");
        printf("%s| %s |%s\n", ui_accent_color(), buf, ui_color(ANSI_RESET));
        return;
    }

    while (pos < len) {
        size_t remain = len - pos;
        size_t chunk = remain;
        size_t next_pos = len;

        if (chunk > (size_t)inner_width) {
            size_t limit = pos + (size_t)inner_width;
            size_t split = limit;
            while (split > pos && !isspace((unsigned char)text[split - 1]))
                split--;

            if (split > pos) {
                chunk = split - pos;
                next_pos = split;
                while (next_pos < len && isspace((unsigned char)text[next_pos]))
                    next_pos++;
            } else {
                chunk = (size_t)inner_width;
                next_pos = pos + chunk;
            }
        }

        snprintf(buf, sizeof(buf), "%-*.*s", inner_width, (int)chunk,
                 text + pos);
        printf("%s| %s |%s\n", ui_accent_color(), buf, ui_color(ANSI_RESET));
        pos = next_pos;
    }
}

void tbmp_ui_box_linef(const char *fmt, ...) {
    char text[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    tbmp_ui_box_line(text);
}

void tbmp_ui_box_kv(const char *key, const char *fmt, ...) {
    char value[192];
    char line[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(value, sizeof(value), fmt, ap);
    va_end(ap);

    snprintf(line, sizeof(line), "%-12s : %s", key, value);
    tbmp_ui_box_line(line);
}

void tbmp_ui_box_begin(const char *title) {
    if (g_ui_ci_mode) {
        tbmp_ui_printlnf("%s:", title);
        return;
    }

    {
        int inner_width = ui_inner_width();
        char buf[256];
        snprintf(buf, sizeof(buf), "%-*.*s", inner_width, inner_width, title);

        ui_box_border();
        printf("%s| %s%s%s |%s\n", ui_accent_color(), ui_color(ANSI_BOLD), buf,
               ui_color(ANSI_RESET), ui_color(ANSI_RESET));
        ui_box_border();
    }
}

void tbmp_ui_box_end(void) {
    ui_box_border();
}

void tbmp_ui_table_begin(const char *title) {
    tbmp_ui_box_begin(title);
}

void tbmp_ui_table_row(const char *key, const char *value) {
    tbmp_ui_box_kv(key, "%s", value);
}

void tbmp_ui_table_end(void) {
    tbmp_ui_box_end();
}

void tbmp_ui_status_success(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    ui_status_vprint(UI_STATUS_SUCCESS, fmt, ap);
    va_end(ap);
}

void tbmp_ui_status_info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    ui_status_vprint(UI_STATUS_INFO, fmt, ap);
    va_end(ap);
}

void tbmp_ui_status_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    ui_status_vprint(UI_STATUS_ERROR, fmt, ap);
    va_end(ap);
}

void tbmp_ui_status_ok(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    ui_status_vprint(UI_STATUS_SUCCESS, fmt, ap);
    va_end(ap);
}

void tbmp_ui_status_warn(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    ui_status_vprint(UI_STATUS_WARN, fmt, ap);
    va_end(ap);
}

void tbmp_ui_spinner_start(TBmpUISpinner *spinner, const char *message) {
    if (!spinner)
        return;

    spinner->active = 1;
    spinner->interactive = (!g_ui_ci_mode) && isatty(STDOUT_FILENO);
    spinner->message = message;
    spinner->frame_index = 0;

    if (!spinner->interactive)
        tbmp_ui_status_info("%s", message);
}

void tbmp_ui_spinner_tick(TBmpUISpinner *spinner) {
    static const char *frames[] = {"◐", "◓", "◑", "◒"};
    if (!spinner || !spinner->active || !spinner->interactive)
        return;

    printf("\r%s%s%s %s", ui_status_color(UI_STATUS_INFO),
           frames[spinner->frame_index % 4U], ui_color(ANSI_RESET),
           spinner->message ? spinner->message : "working");
    fflush(stdout);
    spinner->frame_index++;
}

static void ui_spinner_stop(TBmpUISpinner *spinner, UIStatusKind kind,
                            const char *final_message) {
    const char *msg;

    if (!spinner)
        return;

    msg = final_message ? final_message
                        : (spinner->message ? spinner->message : "done");

    if (spinner->interactive) {
        printf("\r\033[K");
        fflush(stdout);
    }

    spinner->active = 0;

    switch (kind) {
    case UI_STATUS_SUCCESS:
        tbmp_ui_status_success("%s", msg);
        break;
    case UI_STATUS_WARN:
        tbmp_ui_status_warn("%s", msg);
        break;
    case UI_STATUS_ERROR:
        tbmp_ui_status_error("%s", msg);
        break;
    case UI_STATUS_INFO:
    default:
        tbmp_ui_status_info("%s", msg);
        break;
    }
}

void tbmp_ui_spinner_stop_success(TBmpUISpinner *spinner,
                                  const char *final_message) {
    ui_spinner_stop(spinner, UI_STATUS_SUCCESS, final_message);
}

void tbmp_ui_spinner_stop_warn(TBmpUISpinner *spinner,
                               const char *final_message) {
    ui_spinner_stop(spinner, UI_STATUS_WARN, final_message);
}

void tbmp_ui_spinner_stop_error(TBmpUISpinner *spinner,
                                const char *final_message) {
    ui_spinner_stop(spinner, UI_STATUS_ERROR, final_message);
}

void tbmp_ui_step_begin(const char *message) {
    snprintf(g_step_message, sizeof(g_step_message), "%s",
             message ? message : "step");
    g_step_active = 1;
    tbmp_ui_status_info("%s...", g_step_message);
}

void tbmp_ui_step_end_ok(void) {
    if (!g_step_active)
        return;
    tbmp_ui_status_success("%s completed", g_step_message);
    g_step_active = 0;
}

void tbmp_ui_step_end_fail(void) {
    if (!g_step_active)
        return;
    tbmp_ui_status_error("%s failed", g_step_message);
    g_step_active = 0;
}
