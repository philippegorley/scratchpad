#include <libavutil/error.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    int err;
    char a, b, c, d;
    char buf[64];

    err = -1 * atoi(argv[1]);
    a = (err & 0xFF);
    b = (err & 0xFF00) >> 8;
    c = (err & 0xFF0000) >> 16;
    d = (err & 0xFF000000) >> 24;
    av_strerror(-err, buf, sizeof(buf));
    printf("FFERRTAG('%c', '%c', '%c', '%c'): %s\n", a, b, c, d, buf);

    return 0;
}
