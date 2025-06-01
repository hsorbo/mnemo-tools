#include "mnemo.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "hexfile.h"

#define PROGRAM_VERSION "0.1"

struct import_ctx {
    int fd;
    bool raw;
    int imported_bytes;
} import_ctx;

void ondata(char *buf, int n, void *userdata){
    struct import_ctx * ctx = userdata;
    ctx->imported_bytes += n;
    printf("\r\033[KRead: %d bytes", n);
    for (int i = 0; i < n; i++) {
        dprintf(ctx->fd, "%d;", buf[i]);
    }
}

void usage_import(const char *progname) {
    fprintf(stderr,
        "Usage:\n"
        "  %s import [--raw] [--v2] <tty> <file.dmp>\n", progname);
    exit(1);
}

void usage_fwupdate(const char *progname) {
    fprintf(stderr,
        "Usage:\n"
        "  %s fwupdate [--baud <rate>] <tty> <file.hex>\n", progname);
    exit(1);
}

void usage(const char *progname) {
    fprintf(stderr,
        "Usage:\n"
        "  %s import   [--raw] [--v2] <tty> <file.dmp>\n"
        "  %s fwupdate [--baud <rate>] <tty> <file.hex>\n"
        "  %s --version\n"
        "  %s --help\n",
        progname, progname, progname, progname);
    exit(1);
}


static int flash(mnemo *dev, uint8_t * memory) {
    printf("Querying bootloader\n");
    BLInfo info;
    int result = bl_version(dev, &info);
    if (result < 0) {
        printf("Error getting bootloader version\n");
        mnemo_close(dev);
        return 1;
    }

    printf("Bootloader version: %x\n", info.bl_version);

    size_t start = 0x800;
    size_t end = 0x20000;
    size_t total = end - start;
    size_t erase_rows =  total / info.erase_row_size;

    printf("Erasing: (%ld rows of size %d)\n", erase_rows, info.erase_row_size);
    result = bl_flash_erase(dev, start, erase_rows);
    if (result < 0) {
        printf("Error erasing\n");
        return 1;
    }

    memory[end-1] = 0x55;

    for(size_t i = start; i < end; i += 0x80) {
        float percent = 100.0f * (i - start) / total;
        printf("\r\033[KWriting: 0x%.6zx (%.1f%%)", i, percent);
        fflush(stdout);
        result = bl_flash_write(dev, i, memory+i, 0x80);
        if (result < 0) {
            printf("\nError writing!\n");
            return 1;
        }
    }
    printf("\n");

    printf("Verifying: ");
    for(size_t i = start; i < end-2; i += 0xFFF0) {
        uint16_t s = i + 0xFFF0 >= end-2 ? end-i-2 : 0xFFF0;
        result = bl_checksum(dev, i, s);
        if (result < 0) {
            printf("Read error\n");
            return 1;
        }
        if (result != bl_calc_cksum(memory+i, s)) {
            printf("Mismatch!\n");
            return 1;           
        }
    }
    printf("Success\n");

    printf("Restarting\n");
    result = bl_reset(dev);
    if (result < 0) {
        printf("Error restarting device\n");
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    char * progname = argv[0];
    if (argc < 2) {
        usage(progname);
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage(progname);
    }

    if (strcmp(argv[1], "--version") == 0) {
        printf("mnemo version %s\n", PROGRAM_VERSION);
        return 0;
    }

    const char *cmd = argv[1];

    // Shift args so getopt sees correct subcommand arguments
    argc--;
    argv++;

    if (strcmp(cmd, "import") == 0) {
        int raw_mode = 0;
        bool version2 = false;

        struct option longopts[] = {
            {"raw",     no_argument,       0, 'r'},
            {"v2",      required_argument, 0, 'v'},
            {"help",    no_argument,       0, 'h'},
            {0, 0, 0, 0}
        };

        int opt;
        while ((opt = getopt_long(argc, argv, "rv:h", longopts, NULL)) != -1) {
            switch (opt) {
                case 'r':
                    raw_mode = 1;
                    break;
                case 'v':
                version2 = true;
                    break;
                case 'h':
                default:
                    usage_import(progname);
            }
        }

        if (optind + 2 != argc) {
            usage_import(progname);
        }

        const char *tty = argv[optind];
        const char *file = argv[optind + 1];

        int out = open(file, O_CREAT | O_WRONLY | O_TRUNC, 0666);
        if(out < 0) {
            perror(file);
            return -1;
        }

        mnemo *m = mnemo_open(tty, version2 ? MNEMO_VERSION_2 : MNEMO_VERSION_1, B9600);
        if(m == NULL) {
            perror(tty);
            return -1;
        }

        printf("Reading");

        struct import_ctx ctx = {
            .fd = out,
            .raw = raw_mode,
            .imported_bytes = 0
        };

        mnemo_getdata(m, ondata, (void*) &ctx);
        printf("\n");
        mnemo_close(m);
        close(out);
    } else if (strcmp(cmd, "fwupdate") == 0) {
        int baud_rate = 460800;

        struct option longopts[] = {
            {"baud", required_argument, 0, 'b'},
            {"help", no_argument,       0, 'h'},
            {0, 0, 0, 0}
        };

        int opt;
        while ((opt = getopt_long(argc, argv, "b:h", longopts, NULL)) != -1) {
            switch (opt) {
                case 'b':
                    baud_rate = atoi(optarg);
                    if (baud_rate <= 0) {
                        fprintf(stderr, "Invalid baud rate: %s\n", optarg);
                        return 1;
                    }
                    break;
                case 'h':
                default:
                    usage_fwupdate(progname);
            }
        }

        if (optind + 2 != argc) {
            usage_fwupdate(progname);
        }

        const char *tty = argv[optind];
        const char *file = argv[optind + 1];

        if (!strstr(file, ".hex")) {
            fprintf(stderr, "Warning: firmware file '%s' does not have .hex extension\n", file);
        }

        FILE *fw = fopen(file, "r");
        if (!fw) {
            perror("Failed to open firmware file");
            return 1;
        }
        
        if (access(tty, F_OK) != 0) {
            perror("TTY device not found");
            return 1;
        }

        #define MAX_MEMORY (1024 * 1024)  // 1 MB buffer
        uint8_t * memory = malloc(MAX_MEMORY);
        memset(memory, 0xff, MAX_MEMORY);

        int size = load_intel_hex(fw, memory, MAX_MEMORY);
        fclose(fw);

        if (size < 0) {
            printf("Error parsing hexfile\n");
            free(memory);
            return 1;
        }
        
        mnemo *dev = mnemo_open(tty, MNEMO_VERSION_1, baud_rate); // B460800
        int result = 0;
        if (dev != NULL) {
            result = flash(dev, memory);
        }
        else {
            printf("Error opening device\n");
            result = 1;
        }
         
        free(memory);
        mnemo_close(dev);
        return result;
    } else {
        fprintf(stderr, "Unknown subcommand: %s\n", cmd);
        usage(argv[0]);
    }

    return 0;
}
