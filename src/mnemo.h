#ifndef MNEMO_H
#define MNEMO_H
#include <termios.h>
#include <fcntl.h>
#include <time.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>

enum mnemo_version
{
    MNEMO_VERSION_1,
    MNEMO_VERSION_2
};

typedef struct {
    int fd;
    struct termios * oldtio;
    enum mnemo_version version;
} mnemo;

mnemo *mnemo_open(char *tty, enum mnemo_version version);
void mnemo_close(mnemo *device);
void mnemo_getdata(mnemo *dev, void (*ondata)(char*, int, void*), void* userdata);
#endif
