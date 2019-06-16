#pragma once

#include <stdint.h>

struct hantek_device;

typedef int32_t HRESULT;

#define H_ERR(s, x)                 (0x80000000 | ((s) << 16) | (x))
#define H_FAILED(x)                 ((x) & 0x80000000)

#define H_SUB_NONE                  0x0
#define H_SUB_LIBUSB                0x2

#define H_OK                        0x0
#define H_ERR_BAD_ARGS              H_ERR(H_SUB_NONE, 1)
#define H_ERR_NOT_READY             H_ERR(H_SUB_NONE, 2)
#define H_ERR_NO_MEM                H_ERR(H_SUB_NONE, 3)
#define H_ERR_BAD_SAMPLE_RATE       H_ERR(H_SUB_NONE, 4)

#define H_ERR_NOT_FOUND             H_ERR(H_SUB_LIBUSB, 1)
#define H_ERR_CONTROL_FAIL          H_ERR(H_SUB_LIBUSB, 2)
#define H_ERR_CANT_OPEN             H_ERR(H_SUB_LIBUSB, 3)

/**
 * Supported sample spacing times, in nanseconds.
 *
 * To calculate the sampling frequency, invert this value (in seconds).
 */
enum hantek_sample_times {
    HT_ST_1NS = 1,
    HT_ST_2NS = 2,
    HT_ST_5NS = 5,
    HT_ST_10NS = 10,
    HT_ST_25NS = 25,
    HT_ST_50NS = 50,
    HT_ST_100NS = 100,
    HT_ST_250NS = 250,
    HT_ST_500NS = 500,
    HT_ST_1000NS = 1000,
    HT_ST_2500NS = 2500,
    HT_ST_5000NS = 5000,
    HT_ST_10US = 10000,
    HT_ST_25US = 25000,
    HT_ST_50US = 50000,
    HT_ST_100US = 100000,
    HT_ST_250US = 250000,
    HT_ST_500US = 500000,
    HT_ST_1000US = 1000000,
    HT_ST_2500US = 2500000,
    HT_ST_5000US = 5000000,
    HT_ST_10MS = 10000000,
    HT_ST_25MS = 25000000,
    HT_ST_50MS = 50000000,
    HT_ST_100MS = 100000000,
    HT_ST_250MS = 250000000,
    HT_ST_500MS = 500000000,
};

/**
 * Open a Hantek 6000B device.
 */
HRESULT hantek_open_device(struct hantek_device **pdev);

/**
 * Close an open Hantek 6000B device.
 */
HRESULT hantek_close_device(struct hantek_device **pdev);

/**
 * Set the sampling rate for this device. Specified as spacing in ns between samples.
 */
HRESULT hantek_set_sampling_rate(struct hantek_device *dev, enum hantek_sample_times sample_spacing);
