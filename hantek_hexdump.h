#pragma once

#include <stdio.h>

HRESULT hexdump_dumpf_hex(FILE* f, const void *buf, size_t length);
HRESULT hexdump_dump_hex(const void *buf, size_t length);

