#pragma once

#include <stdint.h>

struct hantek_device;

typedef int32_t HRESULT;

#define H_ERR(s, x)     (0x80000000 | ((s) << 16) | (x))
#define H_FAILED(x)     ((x) & 0x80000000)

#define H_SUB_NONE      0x0
#define H_SUB_LIBUSB    0x2

#define H_OK            0x0
#define H_ERR_BAD_ARGS  H_ERR(H_SUB_NONE, 1)
#define H_ERR_NOT_FOUND H_ERR(H_SUB_LIBUSB, 1)

HRESULT hantek_open_device(struct hantek_device **pdev);
