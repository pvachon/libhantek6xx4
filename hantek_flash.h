#pragma once

#include <hantek.h>

#include <stdlib.h>

#define HT_BITSTREAM_FLASH_SIZE               0x80000

HRESULT hantek_read_bitstream_flash(struct hantek_device *dev, uint8_t *target_buffer, size_t buffer_length);

