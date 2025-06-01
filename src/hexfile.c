#include "hexfile.h"

#define MAX_LINE_LEN 520

static int from_hex(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('A' <= c && c <= 'F') return 10 + (c - 'A');
    if ('a' <= c && c <= 'f') return 10 + (c - 'a');
    return -1;
}

static int parse_byte(const char *s) {
    int hi = from_hex(s[0]), lo = from_hex(s[1]);
    if (hi < 0 || lo < 0) return -1;
    return (hi << 4) | lo;
}

int load_intel_hex(FILE *f, uint8_t *buf, size_t bufsize) {
    if (!f) return -1;

    char line[MAX_LINE_LEN];
    uint32_t base = 0;
    uint32_t max_addr = 0;

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len < 11 || line[0] != ':') continue;

        int byte_count = parse_byte(&line[1]);
        int address_hi = parse_byte(&line[3]);
        int address_lo = parse_byte(&line[5]);
        int record_type = parse_byte(&line[7]);

        if (byte_count < 0 || address_hi < 0 || address_lo < 0 || record_type < 0)
            continue;

        uint16_t address = (address_hi << 8) | address_lo;
        

        if (9 + byte_count * 2 > len) continue;

        // Checksum
        uint8_t sum = byte_count + address_hi + address_lo + record_type;
        for (int i = 0; i < byte_count; ++i) {
            int val = parse_byte(&line[9 + i * 2]);
            if (val < 0) break;
            sum += val;
        }
        int checksum = parse_byte(&line[9 + byte_count * 2]);
        if (((sum + checksum) & 0xFF) != 0) continue;

        if (record_type == 0x00) {
            for (int i = 0; i < byte_count; ++i) {
                int val = parse_byte(&line[9 + i * 2]);
                if (val < 0) break;
                uint32_t addr = base + address + i;
                if (addr >= bufsize) continue;
                buf[addr] = (uint8_t)val;
                if (addr + 1 > max_addr) max_addr = addr + 1;
            }
        } else if (record_type == 0x01) {
            break; // EOF
        } else if (record_type == 0x04) {
            // Extended linear address (upper 16 bits)
            int msb_hi = parse_byte(&line[9]);
            int msb_lo = parse_byte(&line[11]);
            if (msb_hi < 0 || msb_lo < 0) continue;
            base = ((msb_hi << 8) | msb_lo) << 16;
        }
    }

    return max_addr;
}