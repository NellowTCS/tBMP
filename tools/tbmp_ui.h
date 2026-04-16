#ifndef TBMP_UI_H
#define TBMP_UI_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum TBmpUIAccent {
    TBMP_UI_ACCENT_CYAN = 0,
    TBMP_UI_ACCENT_GREEN = 1,
    TBMP_UI_ACCENT_YELLOW = 2,
    TBMP_UI_ACCENT_MAGENTA = 3,
} TBmpUIAccent;

typedef struct TBmpUISpinner {
    int active;
    int interactive;
    const char *message;
    unsigned frame_index;
} TBmpUISpinner;

void tbmp_ui_init(int ci_mode);
int tbmp_ui_is_ci(void);
void tbmp_ui_set_accent(TBmpUIAccent accent);

void tbmp_ui_banner(const char *title);
void tbmp_ui_title(const char *title);
void tbmp_ui_section(const char *title);
void tbmp_ui_subsection(const char *title);

void tbmp_ui_box_begin(const char *title);
void tbmp_ui_box_end(void);
void tbmp_ui_box_line(const char *text);
void tbmp_ui_box_linef(const char *fmt, ...);
void tbmp_ui_box_kv(const char *key, const char *fmt, ...);

void tbmp_ui_table_begin(const char *title);
void tbmp_ui_table_row(const char *key, const char *value);
void tbmp_ui_table_end(void);

void tbmp_ui_status_success(const char *fmt, ...);
void tbmp_ui_status_info(const char *fmt, ...);
void tbmp_ui_status_error(const char *fmt, ...);
void tbmp_ui_status_ok(const char *fmt, ...);
void tbmp_ui_status_warn(const char *fmt, ...);

void tbmp_ui_spinner_start(TBmpUISpinner *spinner, const char *message);
void tbmp_ui_spinner_tick(TBmpUISpinner *spinner);
void tbmp_ui_spinner_stop_success(TBmpUISpinner *spinner,
                                  const char *final_message);
void tbmp_ui_spinner_stop_warn(TBmpUISpinner *spinner,
                               const char *final_message);
void tbmp_ui_spinner_stop_error(TBmpUISpinner *spinner,
                                const char *final_message);

void tbmp_ui_step_begin(const char *message);
void tbmp_ui_step_end_ok(void);
void tbmp_ui_step_end_fail(void);

void tbmp_ui_printlnf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* TBMP_UI_H */
