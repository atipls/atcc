#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>

static struct termios orig_termios;
static unsigned char *raw_mode_state = 0;

void SetRawModeState(unsigned char *state) {
    raw_mode_state = state;
}

void DisableRawMode(int fd) {
    if (!*raw_mode_state)
        return;

    tcsetattr(fd, TCSAFLUSH, &orig_termios);
    *raw_mode_state = 0;
}

static void AtExit(void) {
    DisableRawMode(STDIN_FILENO);
}

int EnableRawMode(int fd) {
    struct termios raw;

    if (*raw_mode_state) return 1;
    if (!isatty(STDIN_FILENO)) return 0;
    atexit(AtExit);

    if (tcgetattr(fd, &orig_termios) == -1) return 0;

    raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSAFLUSH, &raw) < 0) return 0;

    *raw_mode_state = 1;
    return 1;
}
