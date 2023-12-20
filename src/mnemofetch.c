#include <getopt.h>
#include <stdio.h>
#include "mnemo.h"

void ondata(char *buf, int n, void *userdata)
{
    printf(".");
    int fd = (int)(size_t) userdata;
    for (int i = 0; i < n; i++) {
        dprintf(fd, "%d;", buf[i]);
    }
}

int main(int argc, char *argv[]) {
    int v2 = 0;
    char *tty = NULL;
    char *output_file = NULL;
    int opt;

    struct option long_options[] = {
        {"v2", no_argument, &v2, 1},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "i:o:", long_options, NULL)) != -1) {
        switch (opt) {
            case 0:
                break;
            case 'i':
                tty = optarg;
                break;
            case 'o':
                output_file = optarg;
                break;
            case '?':
                fprintf(stderr, "Usage: %s [--v2] -i tty -o output_file\n", argv[0]);
                return -1;
        }
    }

    if (tty == NULL || output_file == NULL) {
        fprintf(stderr, "Usage: %s [--v2] -i tty -o output_file\n", argv[0]);
        return -1;
    }

    mnemo *m = mnemo_open(tty, v2 ? MNEMO_VERSION_2 : MNEMO_VERSION_1);
    if(m == NULL) {
        perror(tty);
        return -1;
    }

    int out = open(output_file, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if(out < 0) {
        perror(output_file);
        mnemo_close(m);
        return -1;
    }
    printf("Reading");
    mnemo_getdata(m, ondata, (void*)(size_t) out);
    printf("\n");
    mnemo_close(m);
    close(out);
}