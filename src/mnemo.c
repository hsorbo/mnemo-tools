#include "mnemo.h"

char CMD_GETDATA [1] = {0x43};

#define BL_CMD_GETVER 0x00
#define BL_CMD_FLSH_READ 0x01 // optional
#define BL_CMD_FLSH_WRITE 0x02
#define BL_CMD_FLSH_ERASE 0x03
#define BL_CMD_EEPROM_READ 0x04 // optional
#define BL_CMD_EEPROM_WRITE 0x05 // optional
#define BL_CMD_CONFIG_READ 0x06
#define BL_CMD_CONFIG_WRITE 0x07
#define BL_CMD_CHKSUM 0x08
#define BL_CMD_RESET 0x09

#define BL_RET_SUCCESS 0x01
#define BL_RET_ADDR_OOB 0xfe
#define BL_RET_NOT_SUPPORTED 0xff

#define BL_AUTOBAUD 0x55


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
        retry = (n <= 0) ? retry + 1 : 0;
        if(retry > 0) {
            usleep(100*1000);
            continue;
        }
        ondata(buf, n, userdata);
    }
}

/*
    Useful docs:
    - https://ww1.microchip.com/downloads/en/DeviceDoc/40001779B.pdf
    - AN851 (pretty close, missing 0x08 checksum)
    - AN1310 (different commands, 0x02 = crc)
*/

// static void hexdump(uint8_t *data, size_t len) {
//     for(size_t i = 0; i < len; i++) {
//         printf("%.2x", data[i]);
//     }
//     printf("\n");
// }

static int read_bytes(mnemo *dev, uint8_t * response, size_t count, int timeout) {
    if (dev == NULL) {
        return -2;
    }
    size_t bytes_read = 0;
    while (bytes_read < count) {
        int ret = poll(&dev->pfd, 1, timeout);
        if (ret == 0) {
            //printf("oopsie: \n");
            //hexdump(response, bytes_read);
            return -1;
        }
        int n = read(dev->fd, response+bytes_read, count - bytes_read);
        //printf("got %d bytes\n", n);
        bytes_read += n;
    }
    return bytes_read;
}


static ssize_t bl_write_command(
    mnemo *dev, 
    uint8_t cmd, 
    bool is_write,
    uint16_t size, 
    uint32_t addr) {
    uint8_t x [10] = {
        BL_AUTOBAUD, 
        cmd, 
        (size & 0xff), 
        (size & 0xff00) >> 8, 
        is_write ? 0x55 : 0x00, 
        is_write ? 0xaa : 0x00, 
        (addr & 0xff),
        (addr & 0xff00) >> 8, 
        (addr & 0xff0000) >> 16,
        0x00
    };
    return write(dev->fd, x, sizeof(x));
}


int bl_version(mnemo *dev, BLInfo *info) {
    size_t size = bl_write_command(dev, BL_CMD_GETVER, false, 0x00, 0x00);
    if (size <= 0) {
        return -2;
    }
    uint8_t response [100];
    int n = read_bytes(dev, response, (size_t)26, 1000);
    if (n < 0) {
        return n;
    }
    
    info->bl_version = (response[10] << 8) | response[11];
    info->max_packet_size = (response[12] << 8) | response[13];
    info->device_id = (response[16] << 8) | response[17];
    info->erase_row_size = response[20];
    info->write_latches = response[21];
    info->config_words = ((uint32_t)response[22] << 24) |
                            ((uint32_t)response[23] << 16) |
                            ((uint32_t)response[24] << 8)  |
                            (uint32_t)response[25];
    return 0;
}

int bl_flash_read(mnemo *dev, uint32_t addr, uint8_t * data, uint16_t len) {
    size_t size = bl_write_command(dev, BL_CMD_FLSH_READ, false, len, addr);
    if (size <= 0) {
        return -2;
    }
    uint8_t response [100];
    int n = read_bytes(dev, response, size+(size_t)len, 1000);
    if (n < 0) {
        return n;
    }
    return 0;
}

int bl_flash_write(mnemo *dev, uint32_t addr, uint8_t * data, uint16_t len) {
    size_t size = bl_write_command(dev, BL_CMD_FLSH_WRITE, true, len, addr);
    if (size <= 0) {
        return -2;
    }
    write(dev->fd, data, len);
    uint8_t response [100];
    int n = read_bytes(dev, response, size+1, 1000);
    if (n < 0) {
        return n;
    }
    if (response[n-1] != BL_RET_SUCCESS) {
        return -2;
    }
    return 0;
}


int bl_flash_erase(mnemo *dev, uint32_t addr, uint16_t len) {
    size_t size = bl_write_command(dev, BL_CMD_FLSH_ERASE, true, len, addr);
    if (size <= 0) {
        return -2;
    }
    uint8_t response [100];
    int n = read_bytes(dev, response, size+1, 5000);
    if (n < 0) {
        return n;
    }
    if (response[n-1] != BL_RET_SUCCESS) {
        return -2;
    }
    return 0;
}

int bl_checksum(mnemo *dev, uint32_t addr, uint16_t len) {
    size_t size = bl_write_command(dev, BL_CMD_CHKSUM, false, len, addr);
    if (size <= 0) {
        return -2;
    }
    uint8_t response [100];
    int n = read_bytes(dev, response, size+2, 1000);
    if (n < 0) {
        return n;
    }
    uint16_t crc = ((uint16_t)response[size] << 8) | response[size + 1];
    return (int)crc;
}

int bl_reset(mnemo *dev) {
    size_t size = bl_write_command(dev, BL_CMD_RESET, false, 0, 0);    
    if (size <= 0) {
        return -2;
    }
    // Doesnt seem to respond before rebooting
    return 0;
}

uint16_t bl_calc_cksum(const uint8_t *data, size_t len) {
    uint8_t a = 0, b = 0;

    for (size_t i = 0; i + 1 < len; i += 2) {
        uint16_t sum_a = a + data[i];
        uint8_t carry = (sum_a >= 256) ? 1 : 0;
        a = (uint8_t)(sum_a % 256);
        b = (uint8_t)((b + data[i + 1] + carry) % 256);
    }
    return ((uint16_t)a << 8) | b;
}
