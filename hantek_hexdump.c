#include <hantek.h>
#include <hantek_hexdump.h>

#include <ctype.h>
#include <stdio.h>

HRESULT hexdump_dumpf_hex(FILE* f, const void *buf, size_t length)
{
    HRESULT ret = H_OK;

    if (NULL == f) {
        return H_OK;
    }

    const uint8_t *ptr = buf;

    if (NULL == buf || 0 == length) {
        return H_ERR_BAD_ARGS;
    }

    fprintf(f, "Dumping %zu bytes at %p\n", length, buf);

    for (size_t i = 0; i < length; i+=16) {
        fprintf(f, "%16zx: ", i);
        for (int j = 0; j < 16; j++) {
            if (i + j < length) {
                fprintf(f, "%02x ", (unsigned)ptr[i + j]);
            } else {
                fprintf(f, "   ");
            }
        }
        fprintf(f, " |");
        for (int j = 0; j < 16; j++) {
            if (i + j < length) {
                fprintf(f, "%c", isprint(ptr[i + j]) ? (char)ptr[i + j] : '.');
            } else {
                fprintf(f, " ");
            }
        }
        fprintf(f, "|\n");
    }

    return ret;
}

HRESULT hexdump_dump_hex(const void *buf, size_t length)
{
    return hexdump_dumpf_hex(stdout, buf, length);
}
