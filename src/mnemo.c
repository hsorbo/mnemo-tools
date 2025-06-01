#include "mnemo.h"

char CMD_GETDATA [1] = {0x43};

mnemo* mnemo_open(const char *tty, enum mnemo_version version, speed_t speed) {
    int fd = open(tty, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) {
        return NULL;
    }
    mnemo *device = malloc(sizeof(mnemo));
    device->fd = fd;
    device->oldtio = malloc(sizeof(struct termios));  
    device->version = version;  
    tcgetattr(device->fd, device->oldtio);
    struct termios settings;
    bzero(&settings, sizeof(settings));
    settings.c_cflag = CS8 | CLOCAL | CREAD;
    cfsetspeed(&settings, speed);
    tcflush(device->fd, TCIFLUSH);
    tcsetattr(device->fd, TCSANOW, &settings);
    
    device->pfd.fd = fd;
    device->pfd.events = POLLIN;
    device->pfd.revents = 0;

    return device;
}

void mnemo_close(mnemo *device) {
    tcsetattr(device->fd,TCSANOW,device->oldtio);  
    close(device->fd);
    free(device->oldtio);
    free(device);
}

void mnemo_getdata(mnemo *dev, void (*ondata)(char*, int, void*), void* userdata) {
    if (dev->version == MNEMO_VERSION_1) {
        time_t t = time(NULL);
        struct tm *info = localtime(&t);
        char header[5] = {
            (char)info->tm_year - 100,
            (char)info->tm_mon + 1,
            (char)info->tm_mday,
            (char)info->tm_hour,
            (char)info->tm_min,
        };
        write(dev->fd, CMD_GETDATA, 1);
        usleep(100*1000);
        write(dev->fd, header, 5);
    }    
    else if (dev->version == MNEMO_VERSION_2) {
        write(dev->fd, "getdata\n", 8);
    }

    int retry = 0;

    while (retry < 5) {
        char buf[1024];
        int n = read(dev->fd, buf, sizeof(buf));
        retry = (n == 0) ? retry + 1 : 0;
        if(retry > 0) {
            usleep(100*1000);
            continue;
        }
        ondata(buf, n, userdata);
    }
}