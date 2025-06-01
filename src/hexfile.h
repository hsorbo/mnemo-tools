#ifndef HEXFILE_H
#define HEXFILE_H

#include "mnemo.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int load_intel_hex(FILE *f, uint8_t *buf, size_t bufsize);

#endif