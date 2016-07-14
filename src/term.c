#include <termios.h>
#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <stdarg.h>

#include "utils/utils.h"
#include "utils/debug.h"
#include "utils/xmalloc.h"

#include "term.h"

#define term_get_byte_esc() __extension__ ({ \
    i32 tmp = term_get_byte(true); \
    if (tmp < 0) \
        return tmp == -ETIMEDOUT ? TERM_TIMED_OUT : tmp; \
    tmp; \
})

static struct termios term_old;
static struct termios term_new;
static u8 term_in_buf[16];
static size_t term_in_buf_len;
static size_t term_in_buf_pos;
static int term_escdelay = 500;
static i32 term_mouse_x;
static i32 term_mouse_y;
static u8 term_out_buf[4096];
static size_t term_out_buf_pos;

i32 term_get_mouse_x(void)
{
    return term_mouse_x;
}

i32 term_get_mouse_y(void)
{
    return term_mouse_y;
}

static void term_init_escdelay(void)
{
    char *end, *ed = getenv("ESCDELAY");
    if (!ed || *ed == 0)
        return;
    long tmp = strtol(ed, &end, 10);
    if (*end == 0)
        term_escdelay = (int)tmp;
}

static void term_init_termios(void)
{
    BUG_ON(tcgetattr(0, &term_old));
    memcpy(&term_new, &term_old, sizeof(term_new));

    term_new.c_lflag &= ~(tcflag_t)(ICANON | ECHO | ISIG);
    term_new.c_cc[VMIN] = 0;
    term_new.c_cc[VTIME] = 0;

    BUG_ON(tcsetattr(0, TCSANOW, &term_new));
}

void term_init(void)
{
    if (!isatty(0) || !isatty(1))
        BUG("oka only works in a terminal\n");

    term_init_escdelay();
    term_init_termios();
}

void term_exit(void)
{
    term_flush();
    tcsetattr(0, TCSANOW, &term_old);
}

static ssize_t term_in_buf_fill(void)
{
    term_in_buf_pos = 0;
    term_in_buf_len = 0;
    auto rc = read(0, term_in_buf, sizeof(term_in_buf));
    if (rc > 0)
        term_in_buf_len = (size_t)rc;
    else if (rc == 0)
        rc = -ETIMEDOUT;
    else
        rc = -errno;
    return rc;
}

static ssize_t term_in_buf_fill_esc(void)
{
    auto rc = term_in_buf_fill();
    if (rc < 0)
        return rc;
    if (rc == 0) {
        struct pollfd pfd = { .fd = 0, .events = POLLIN };
        rc = poll(&pfd, 1, term_escdelay);
        if (rc <= 0)
            return rc < 0 ? -errno : -ETIMEDOUT;
        rc = term_in_buf_fill();
    }
    return rc;
}

static i32 term_get_byte(bool esc)
{
    if (term_in_buf_pos == term_in_buf_len) {
        auto rc = esc ? term_in_buf_fill_esc() : term_in_buf_fill();
        if (rc < 0)
            return (i32)rc;
    }

    return term_in_buf[term_in_buf_pos++];
}

static i32 term_handle_ss3(void)
{
    auto byte = term_get_byte_esc();

    static const struct {
        i32 in;
        i32 out;
    } map[] = {
        { 'A', TERM_CURSOR_UP },
        { 'B', TERM_CURSOR_DOWN },
        { 'C', TERM_CURSOR_RIGHT },
        { 'D', TERM_CURSOR_LEFT },
        { 'a', TERM_CURSOR_UP | TERM_CTRL },
        { 'b', TERM_CURSOR_DOWN | TERM_CTRL },
        { 'c', TERM_CURSOR_RIGHT | TERM_CTRL },
        { 'd', TERM_CURSOR_LEFT | TERM_CTRL },
        { 'H', TERM_HOME },
        { 'F', TERM_END },
        { 'P', TERM_F1 },
        { 'Q', TERM_F2 },
        { 'R', TERM_F3 },
        { 'S', TERM_F4 },
    };

    for (size_t i = 0; i < N_ELEMENTS(map); i++) {
        if (byte == map[i].in)
            return map[i].out;
    }

    return TERM_UNKNOWN_ESC;
}

static i32 term_get_mod(long arg)
{
    switch (arg) {
    case 2: return TERM_SHIFT;
    case 3: return TERM_ALT;
    case 4: return TERM_SHIFT | TERM_ALT;
    case 5: return TERM_CTRL;
    case 6: return TERM_SHIFT | TERM_CTRL;
    case 7: return TERM_ALT | TERM_CTRL;
    case 8: return TERM_SHIFT | TERM_ALT | TERM_CTRL;
    default: return 0;
    }
}

static i32 term_get_key(long arg)
{
    switch (arg) {
    case 1: return TERM_HOME;
    case 2: return TERM_INSERT;
    case 3: return TERM_DELETE;
    case 4: return TERM_END;
    case 5: return TERM_PAGE_UP;
    case 6: return TERM_PAGE_DOWN;
    case 7: return TERM_HOME;
    case 8: return TERM_END;
    case 11: return TERM_F1;
    case 12: return TERM_F2;
    case 13: return TERM_F3;
    case 14: return TERM_F4;
    case 15: return TERM_F5;
    case 17: return TERM_F6;
    case 18: return TERM_F7;
    case 19: return TERM_F8;
    case 20: return TERM_F9;
    case 21: return TERM_F10;
    case 23: return TERM_F11;
    case 24: return TERM_F12;
    default: return TERM_UNKNOWN_ESC;
    }
}

static i32 term_handle_csi_tilde(char *buf)
{
    char *end;
    long key_id = strtol(buf, &end, 10);
    if (end == buf || (*end != 0 && *end != ';'))
        return TERM_UNKNOWN_ESC;

    long mod_id = 0;
    if (*end == ';')
        mod_id = strtol(end + 1, NULL, 10);

    i32 key = term_get_key(key_id);
    if (key > TERM_ERR_MAX)
        key |= term_get_mod(mod_id);

    return key;
}

static i32 term_handle_csi_caret(char *buf)
{
    i32 res = term_handle_csi_tilde(buf);
    if (res > TERM_ERR_MAX)
        res |= TERM_CTRL;
    return res;
}

static i32 term_handle_csi_generic(char *buf, i32 base)
{
    char *semicolon = strchr(buf, ';');
    if (semicolon == NULL)
        return base;

    long mod_id = 0;
    if (semicolon)
        mod_id = strtol(semicolon + 1, NULL, 10);

    return base | term_get_mod(mod_id);
}

static i32 term_handle_mouse_1000_vals(i32 button, i32 x, i32 y)
{
    i32 keys[] = {
        TERM_MOUSE1_PRESS,
        TERM_MOUSE2_PRESS,
        TERM_MOUSE3_PRESS,
        TERM_MOUSE_RELEASE,
        TERM_SCROLL_UP,
        TERM_SCROLL_DOWN,
        TERM_SCROLL_LEFT,
        TERM_SCROLL_RIGHT,
    };

    i32 key = keys[(button & 3) | (button & 64 ? 4 : 0)];

    if (button & 4)
        key |= TERM_SHIFT;
    if (button & 8)
        key |= TERM_ALT;
    if (button & 16)
        key |= TERM_CTRL;

    term_mouse_x = x - 1;
    term_mouse_y = y - 1;

    return key;
}

static i32 term_handle_mouse_1000(void)
{
    i32 button = term_get_byte_esc();
    i32 x = term_get_byte_esc();
    i32 y = term_get_byte_esc();
    return term_handle_mouse_1000_vals(button, x, y);
}

static i32 term_get_mouse_1015_vals(char *buf, i32 *button, i32 *x, i32 *y)
{
    char *end;
    *button = (i32)strtol(buf, &end, 10);
    if (buf == end || *end != ';')
        return TERM_UNKNOWN_ESC;
    buf = end + 1;
    *x = (i32)strtol(buf, &end, 10);
    if (buf == end || *end != ';')
        return TERM_UNKNOWN_ESC;
    buf = end + 1;
    *y = (i32)strtol(buf, &end, 10);
    if (buf == end || *end != 0)
        return TERM_UNKNOWN_ESC;
    return 0;
}

static i32 term_handle_mouse_1015(char *buf)
{
    i32 button, x, y;
    auto rc = term_get_mouse_1015_vals(buf, &button, &x, &y);
    if (rc)
        return rc;
    return term_handle_mouse_1000_vals(button, x, y);
}

static i32 term_handle_mouse_1006(char *buf, i32 minor)
{
    i32 button, x, y;
    auto rc = term_get_mouse_1015_vals(buf, &button, &x, &y);
    if (rc)
        return rc;
    rc = term_handle_mouse_1000_vals(button, x, y);
    if (rc > TERM_ERR_MAX && minor == 'm')
        rc = (rc & ~TERM_SPECIAL_MASK) | TERM_MOUSE_RELEASE;
    return rc;
}

static i32 term_handle_mouse(char *buf, i32 minor)
{
    if (*buf == 0)
        return term_handle_mouse_1000();
    if (*buf == '<')
        return term_handle_mouse_1006(buf + 1, minor);
    return term_handle_mouse_1015(buf);
}

static i32 term_handle_csi(void)
{
    char buf[128];
    char last;
    size_t len = 0;
    while (1) {
        last = buf[len] = (char)term_get_byte_esc();
        if (last >= 0x40 && last < 0x7f)
            break;
        if (len == sizeof(buf))
            return TERM_UNKNOWN_ESC;
        len++;
    }
    buf[len] = 0;
    
    static const struct {
        i32 in;
        i32 out;
    } map[] = {
        { 'A', TERM_CURSOR_UP },
        { 'B', TERM_CURSOR_DOWN },
        { 'C', TERM_CURSOR_RIGHT },
        { 'D', TERM_CURSOR_LEFT },
        { 'a', TERM_CURSOR_UP | TERM_SHIFT },
        { 'b', TERM_CURSOR_DOWN | TERM_SHIFT },
        { 'c', TERM_CURSOR_RIGHT | TERM_SHIFT },
        { 'd', TERM_CURSOR_LEFT | TERM_SHIFT },
        { 'H', TERM_HOME },
        { 'F', TERM_END },
        { 'P', TERM_F1 },
        { 'Q', TERM_F2 },
        { 'R', TERM_F3 },
        { 'S', TERM_F4 },
        { 'Z', TERM_BACKTAB },
    };

    for (size_t i = 0; i < N_ELEMENTS(map); i++) {
        if (last == map[i].in)
            return term_handle_csi_generic(buf, map[i].out);
    }

    if (last == '~')
        return term_handle_csi_tilde(buf);

    if (last == '^')
        return term_handle_csi_caret(buf);

    if (last == 'M' || last == 'm')
        return term_handle_mouse(buf, last);

    return TERM_UNKNOWN_ESC;
}

static i32 term_handle_esc(void)
{
    auto byte = term_get_byte(true);
    if (byte < 0)
        return byte == -ETIMEDOUT ? TERM_ESC : byte;
    if (byte == '[')
        return term_handle_csi();
    if (byte == 'O')
        return term_handle_ss3();
    term_in_buf_pos--;
    return TERM_ESC;
}

static i32 term_handle_utf8(u8 first)
{
    size_t width = utils_utf8_width(first);
    i32 res = (u8)(first << width) >> width;
    for (size_t i = 0; i < width - 1; i++) {
        i32 byte = term_get_byte_esc();
        res = (res << 6) | (byte & ~0x80);
    }
    return res;
}

static i32 term_handle_c1(u8 c1)
{
    if (c1 == 0x8f)
        return term_handle_ss3();
    if (c1 == 0x9b)
        return term_handle_csi();
    return TERM_UNKNOWN_ESC;
}

i32 term_get_char(bool esc)
{
    auto byte = term_get_byte(esc);
    if (byte < 0)
        return byte == -ETIMEDOUT ? TERM_TIMED_OUT : byte;
    if (byte == TERM_ESC)
        return term_handle_esc();
    if (byte < 0x7f)
        return byte;
    if (byte == 0x7f)
        return '\b';
    if (byte < 0xc0)
        return term_handle_c1((u8)byte);
    return term_handle_utf8((u8)byte);
}

int term_flush(void)
{
    int rc = 0;
    u8 *base = term_out_buf;
    while (term_out_buf_pos > 0) {
        auto written = write(1, base, term_out_buf_pos);
        if (written < 0) {
            rc = -errno;
            break;
        }
        term_out_buf_pos -= (size_t)written;
        base += written;
    }
    term_out_buf_pos = 0;
    return rc;
}

int term_putsn(const char *str, size_t len)
{
    int rc = 0;
    while (1) {
        size_t left = sizeof(term_out_buf) - term_out_buf_pos;
        size_t copy = min(len, left);
        memcpy(term_out_buf + term_out_buf_pos, str, copy);
        len -= copy;
        term_out_buf_pos += copy;
        if (len > 0) {
            rc = term_flush();
            if (rc)
                break;
            str += copy;
        } else
            break;
    }
    return rc;
}

int term_puts(const char *str)
{
    return term_putsn(str, strlen(str));
}

int term_printf(const char *fmt, ...)
{
    char buf[4096];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    return term_puts(buf);
}

int term_enable_mouse(void)
{
    return term_puts("\x1b[?1000h\x1b[?1015h\x1b[?1006h");
}

int term_disable_mouse(void)
{
    return term_puts("\x1b[?1000l");
}

int term_set_foreground_color(int color)
{
    char buf[64];
    if (color < 8)
        snprintf(buf, sizeof(buf), "\x1b[3%dm", color);
    else if (color < 16)
        snprintf(buf, sizeof(buf), "\x1b[9%dm", color - 8);
    else
        snprintf(buf, sizeof(buf), "\x1b[38;5;%dm", color);
    return term_puts(buf);
}

int term_reset_foreground_color(void)
{
    return term_puts("\x1b[39m");
}

int term_set_background_color(int color)
{
    char buf[64];
    if (color < 8)
        snprintf(buf, sizeof(buf), "\x1b[4%dm", color);
    else if (color < 16)
        snprintf(buf, sizeof(buf), "\x1b[10%dm", color - 8);
    else
        snprintf(buf, sizeof(buf), "\x1b[48;5;%dm", color);
    return term_puts(buf);
}

int term_reset_background_color(void)
{
    return term_puts("\x1b[49m");
}

const char *term_key_as_str(i32 key, bool *shift, bool *alt, bool *ctl)
{
    *shift = (key & TERM_SHIFT) != 0;
    *alt = (key & TERM_ALT) != 0;
    *ctl = (key & TERM_CTRL) != 0;

    key &= TERM_SPECIAL_MASK;

#define to_str(x) if (key == (x & TERM_SPECIAL_MASK)) return #x;
    to_str(TERM_TIMED_OUT);
    to_str(TERM_INVALID_UTF8);
    to_str(TERM_UNKNOWN_ESC);
    to_str(TERM_CURSOR_UP);
    to_str(TERM_CURSOR_DOWN);
    to_str(TERM_CURSOR_RIGHT);
    to_str(TERM_CURSOR_LEFT);
    to_str(TERM_HOME);
    to_str(TERM_END);
    to_str(TERM_PAGE_UP);
    to_str(TERM_PAGE_DOWN);
    to_str(TERM_INSERT);
    to_str(TERM_DELETE);
    to_str(TERM_BACKTAB);
    to_str(TERM_F1);
    to_str(TERM_F2);
    to_str(TERM_F3);
    to_str(TERM_F4);
    to_str(TERM_F5);
    to_str(TERM_F6);
    to_str(TERM_F7);
    to_str(TERM_F8);
    to_str(TERM_F9);
    to_str(TERM_F10);
    to_str(TERM_F11);
    to_str(TERM_F12);
    to_str(TERM_MOUSE1_PRESS);
    to_str(TERM_MOUSE2_PRESS);
    to_str(TERM_MOUSE3_PRESS);
    to_str(TERM_MOUSE_RELEASE);
    to_str(TERM_SCROLL_UP);
    to_str(TERM_SCROLL_DOWN);
    to_str(TERM_SCROLL_LEFT);
    to_str(TERM_SCROLL_RIGHT);
#undef to_str

    return "unknown";
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
