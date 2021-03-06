#include <pthread.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>

#include "utils/utils.h"
#include "utils/signals.h"
#include "utils/thread.h"
#include "utils/xmalloc.h"
#include "utils/diag.h"
#include "utils/loop.h"

#include "main.h"
#include "worker.h"
#include "globals.h"
#include "term.h"
#include "plugins.h"
#include "player.h"
#include "decoder.h"

struct worker *worker;
struct diag *main_diag;

static int main_winch_fd;
static struct loop *main_loop;
static struct loop_watch *main_stdin_watch;
static struct loop_watch *main_winch_watch;

static void main_delegate(struct delegate *d)
{
    loop_delegate(main_loop, d);
}

static void main_delegate_sync(struct delegate *d)
{
    loop_delegate_sync(main_loop, d);
}

struct main_diag_delegate {
    struct delegate d;
    char *msg;
};

static void main_diag_err_delegate(struct delegate *d)
{
    auto_free auto m = container_of(d, struct main_diag_delegate, d);
    fprintf(stderr, "error: %s\n", m->msg);
    free(m->msg);
}

static void main_diag_err(char *msg)
{
    auto m = xnew_uninit(struct main_diag_delegate);
    m->d.run = main_diag_err_delegate;
    m->msg = msg;
    main_delegate(&m->d);
}

noreturn static void main_diag_fatal_delegate(struct delegate *d)
{
    auto_free auto m = container_of(d, struct main_diag_delegate, d);
    fprintf(stderr, "fatal error: %s\n", m->msg);
    free(m->msg);
    exit(1);
}

static void main_diag_fatal(char *msg)
{
    auto m = xnew_uninit(struct main_diag_delegate);
    m->d.run = main_diag_fatal_delegate;
    m->msg = msg;
    main_delegate(&m->d);
}

static void main_diag_info_delegate(struct delegate *d)
{
    auto_free auto m = container_of(d, struct main_diag_delegate, d);
    fprintf(stderr, "info: %s\n", m->msg);
    free(m->msg);
}

static void main_diag_info(char *msg)
{
    auto m = xnew_uninit(struct main_diag_delegate);
    m->d.run = main_diag_info_delegate;
    m->msg = msg;
    main_delegate(&m->d);
}

static void main_diag_init(void)
{
    static const struct diag_api main_diag_api = {
        .error = main_diag_err,
        .fatal = main_diag_fatal,
        .info = main_diag_info,
    };

    main_diag = diag_new(&main_diag_api);
}

static void main_diag_exit(void)
{
    diag_free(main_diag);
}

static void main_worker_init(void)
{
    worker = worker_new();
}

static void main_worker_exit(void)
{
    worker_free(worker);
}

static void main_handle_winch(struct loop_watch *w_, void *opaque, int fd, u32 events)
{
    (void)w_;
    (void)fd;
    (void)opaque;
    (void)events;

    struct signalfd_siginfo si;
    BUG_ON(read(main_winch_fd, &si, sizeof(si)) == -1);

    struct winsize w = { 0 };
    ioctl(0, TIOCGWINSZ, &w);

    term_printf("width: %d, height: %d\n", w.ws_col, w.ws_row);
    term_flush();
}

static void main_winch_init(void)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGWINCH);
    thread_sigmask(SIG_BLOCK, &set, NULL);

    main_winch_fd = signalfd(-1, &set, SFD_NONBLOCK | SFD_CLOEXEC);
    BUG_ON(main_winch_fd == -1);

    main_winch_watch = loop_watch_new(main_loop, main_handle_winch, NULL);
    loop_watch_set(main_winch_watch, main_winch_fd, EPOLLIN);
}

static void main_winch_exit(void)
{
    loop_watch_free(main_winch_watch);
    close(main_winch_fd);
}

static void main_signals_init(void)
{
    sigset_t set;
    sigemptyset(&set);
    // nothing
    thread_sigmask(SIG_BLOCK, &set, NULL);
}

static void main_loop_init(void)
{
    main_loop = loop_new();
}

static void main_loop_exit(void)
{
    loop_free(main_loop);
}

static void main_handle_stdin(struct loop_watch *w, void *opaque, int fd, u32 events)
{
    (void)w;
    (void)fd;
    (void)opaque;
    (void)events;

    i32 i;
    while ((i = term_get_char(false)) != TERM_TIMED_OUT) {
        if (i < 0)
            printf("err: %s\n", utils_strerr(-i));
        else if (i >= TERM_SPECIAL_MIN) {
            bool shift, alt, ctrl;
            const char *s = term_key_as_str(i, &shift, &alt, &ctrl);
            term_puts("key: ");
            if (shift)
                term_puts("s-");
            if (alt)
                term_puts("a-");
            if (ctrl)
                term_puts("c-");
            term_puts(s);
            term_puts("\n");
        }
        else if (i == TERM_CTRL_C) {
            loop_stop(main_loop, 0);
            break;
        } else {
            if (i == 'c')
                player_toggle_pause();
            if (i == 'm')
                player_toggle_mute();
            if (i == 'h')
                player_seek(-5000);
            if (i == 'l')
                player_seek(+5000);
            if (i == 'n')
                player_goto_next();
            term_printf("char: %x\n", i);
        }
    }
    term_flush();
}

static void main_stdin_init(void)
{
    main_stdin_watch = loop_watch_new(main_loop, main_handle_stdin, NULL);
    loop_watch_set(main_stdin_watch, 0, EPOLLIN);
}

static void main_stdin_exit(void)
{
    loop_watch_free(main_stdin_watch);
}

static void main_init(void)
{
    main_diag_init();
    main_signals_init();
    main_loop_init();
    main_stdin_init();
    main_winch_init();
    main_worker_init();

    term_init();
    player_init();
    plugins_init();
}

static void main_exit(void)
{
    player_exit();
    plugins_exit();
    term_exit();

    main_worker_exit();
    main_winch_exit();
    main_stdin_exit();
    main_loop_exit();
    main_diag_exit();
}

int main(void)
{
    main_init();

    auto yo = plugins_open("/home/julian/ylia/01.mp3");
    player_set_input(yo, (struct main_track_cookie *)"ayo");

    loop_run(main_loop);

    main_exit();
}

struct main_sink_info_changed {
    struct delegate d;
    struct sink_info i;
};

static void main_volume_changed_delegate(struct delegate *d)
{
    auto_free auto v = container_of(d, struct main_sink_info_changed, d);
    auto i = &v->i;
    printf("sink info changed: stopped: %d, paused: %d, mute: %d, vol_left: %u,"
            " vol_right: %u\n", i->stopped, i->paused, i->mute, i->vol_l, i->vol_r);
}

void main_sink_info_changed(struct sink_info *i)
{
    auto v = xnew_uninit(struct main_sink_info_changed);
    v->d.run = main_volume_changed_delegate;
    v->i = *i;
    main_delegate(&v->d);
}

static _Atomic u32 main_pos;

static void main_position_changed_delegate(struct delegate *d)
{
    (void)d;
    printf("position changed: %"PRIu32"\n", main_pos);
}

void main_position_changed(u32 pos)
{
    static struct delegate d = { main_position_changed_delegate };
    main_pos = pos;
    main_delegate(&d);
}

static struct main_track_cookie * _Atomic main_track;

static void main_track_changed_delegate(struct delegate *d)
{
    (void)d;
    printf("track changed: %s\n", (char *)main_track);
}

void main_track_changed(struct main_track_cookie *c)
{
    static struct delegate d = { main_track_changed_delegate };
    main_track = c;
    main_delegate(&d);
}

struct main_get_next_track {
    struct delegate d;
    struct decoder_stream *s;
    struct main_track_cookie *c;
};

static void main_get_next_track_delegate(struct delegate *d)
{
    static bool bla;
    static char *t2 = "/home/julian/dragons/02.mp3";
    static char *t3 = "/home/julian/dragons/03.mp3";
    char *track;
    if (!bla) {
        track = t2;
        bla = true;
    } else {
        track = t3;
    }
    auto v = container_of(d, struct main_get_next_track, d);
    v->s = plugins_open(track);
    v->c = (struct main_track_cookie *)track;
}

void main_get_next_track_SYNC(struct decoder_stream **s, struct main_track_cookie **c)
{
    struct main_get_next_track v = { .d.run = main_get_next_track_delegate };
    main_delegate_sync(&v.d);
    *s = v.s;
    *c = v.c;
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
