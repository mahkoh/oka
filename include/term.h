#ifndef OKA_TERM_H
#define OKA_TERM_H

#include <stdbool.h>

#define TERM_CTRL(x) ((x) & 0x3F)

#define TERM_ESC 0x1b
#define TERM_CTRL_C TERM_CTRL('C')

#define TERM_SPECIAL_MIN 0x40000000
#define TERM_SPECIAL_MASK 0xffff
#define TERM_MODIFIER_SHIFT 16

enum {
    TERM_SHIFT = (1 << TERM_MODIFIER_SHIFT),
    TERM_CTRL  = (2 << TERM_MODIFIER_SHIFT),
    TERM_ALT   = (4 << TERM_MODIFIER_SHIFT),
};

enum {
    TERM_TIMED_OUT = TERM_SPECIAL_MIN,
    TERM_INVALID_UTF8,
    TERM_UNKNOWN_ESC,

#define TERM_ERR_MAX TERM_UNKNOWN_ESC

    TERM_CURSOR_UP,
    TERM_CURSOR_DOWN,
    TERM_CURSOR_RIGHT,
    TERM_CURSOR_LEFT,
    TERM_HOME,
    TERM_END,
    TERM_PAGE_UP,
    TERM_PAGE_DOWN,
    TERM_INSERT,
    TERM_DELETE,
    TERM_BACKTAB,
    TERM_F1,
    TERM_F2,
    TERM_F3,
    TERM_F4,
    TERM_F5,
    TERM_F6,
    TERM_F7,
    TERM_F8,
    TERM_F9,
    TERM_F10,
    TERM_F11,
    TERM_F12,

    TERM_MOUSE1_PRESS,
    TERM_MOUSE2_PRESS,
    TERM_MOUSE3_PRESS,
    TERM_MOUSE_RELEASE,
    TERM_SCROLL_UP,
    TERM_SCROLL_DOWN,
    TERM_SCROLL_LEFT,
    TERM_SCROLL_RIGHT,
};

void term_init(void);
void term_exit(void);
i32 term_get_char(bool esc);
i32 term_get_mouse_x(void);
i32 term_get_mouse_y(void);
const char *term_key_as_str(i32 key, bool *shift, bool *alt, bool *ctl);

int term_printf(const char *fmt, ...) PRINTF(1, 2);
int term_puts(const char *str);
int term_putsn(const char *str, size_t len);
int term_flush(void);

int term_enable_mouse(void);
int term_disable_mouse(void);

int term_set_foreground_color(int color);
int term_reset_foreground_color(void);
int term_set_background_color(int color);
int term_reset_background_color(void);

#endif

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
