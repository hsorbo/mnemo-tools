#ifndef MNEMO_H
#define MNEMO_H
#include <termios.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum mnemo_version
{
    MNEMO_VERSION_1,
    MNEMO_VERSION_2
};

typedef struct {
    int fd;
    struct termios * oldtio;
    enum mnemo_version version;
    struct pollfd pfd;
} mnemo;

mnemo *mnemo_open(const char *tty, enum mnemo_version version, speed_t speed);
void mnemo_close(mnemo *device);
void mnemo_getdata(mnemo *dev, void (*ondata)(char*, int, void*), void* userdata);

//bootloader
typedef struct {
    uint16_t bl_version;
    uint16_t max_packet_size;
    uint16_t device_id;
    uint8_t erase_row_size;
    uint8_t write_latches;
    uint32_t config_words;
} BLInfo;

int bl_version(mnemo *dev, BLInfo *info);
int bl_flash_read(mnemo *dev, uint32_t addr, uint8_t * data, uint16_t len);
int bl_flash_write(mnemo *dev, uint32_t addr, uint8_t * data, uint16_t len);
int bl_flash_erase(mnemo *dev, uint32_t addr, uint16_t len);
int bl_checksum(mnemo *dev, uint32_t addr, uint16_t len);
int bl_reset(mnemo *dev);
uint16_t bl_calc_cksum(const uint8_t *data, size_t len);
#endif
