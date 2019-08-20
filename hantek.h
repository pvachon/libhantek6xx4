#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

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
#define H_ERR_INVAL_CHANNELS        H_ERR(H_SUB_NONE, 5)
#define H_ERR_INVAL_VOLTS_PER_DIV   H_ERR(H_SUB_NONE, 6)

#define H_ERR_NOT_FOUND             H_ERR(H_SUB_LIBUSB, 1)
#define H_ERR_CONTROL_FAIL          H_ERR(H_SUB_LIBUSB, 2)
#define H_ERR_CANT_OPEN             H_ERR(H_SUB_LIBUSB, 3)


/**
 * Supported time-per-dvision (for the virtical graticule)
 */
enum hantek_time_per_division {
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

enum hantek_volts_per_div {
    HT_VPD_2MV = 0,
    HT_VPD_5MV = 1,
    HT_VPD_10MV = 2,
    HT_VPD_20MV = 3,
    HT_VPD_50MV = 4,
    HT_VPD_100MV = 5,
    HT_VPD_200MV = 6,
    HT_VPD_500MV = 7,
    HT_VPD_1V = 8,
    HT_VPD_2V = 9,
    HT_VPD_5V = 10,
    HT_VPD_10V = 11,
};

enum hantek_coupling {
    HT_COUPLING_DC = 0,
    HT_COUPLING_AC = 1,
};

enum hantek_trigger_mode {
    HT_TRIGGER_EDGE = 0,
    HT_TRIGGER_PULSE = 1,
    HT_TRIGGER_VIDEO = 2,
    HT_TRIGGER_FORCE = 0x80,
};

enum hantek_trigger_slope {
    HT_TRIGGER_SLOPE_RISE = 0,
    HT_TRIGGER_SLOPE_FALL = 1,
};

enum hantek_capture_mode {
    HT_CAPTURE_AUTO = 0x0,
    HT_CAPTURE_ROLL = 0x1,
    HT_CAPTURE_SINGLE = 0x2,
};

/**
 * Open a Hantek 6xx4 device.
 */
HRESULT hantek_open_device(struct hantek_device **pdev, uint32_t capture_buffer_len);

/**
 * Close an open Hantek 6xx4 device.
 */
HRESULT hantek_close_device(struct hantek_device **pdev);

/**
 * Set the sampling rate for this device. Specified as spacing in ns between samples.
 */
HRESULT hantek_set_sampling_rate(struct hantek_device *dev, enum hantek_time_per_division sample_spacing);

/**
 * Configure a particular channel's front end parameters.
 */
HRESULT hantek_configure_channel_frontend(struct hantek_device *dev, unsigned channel_num, enum hantek_volts_per_div volts_per_div, enum hantek_coupling coupling, bool bw_limit, bool enable, unsigned chan_level);

/**
 * Configure trigger mode and level
 */
HRESULT hantek_configure_trigger(struct hantek_device *dev, unsigned channel_num, enum hantek_trigger_mode mode, enum hantek_trigger_slope slope, enum hantek_coupling coupling, uint8_t trig_vertical_level, uint32_t trig_horiz_offset);

/**
 * Configure the range of the ADC, dependent on the number of channels enabled
 */
HRESULT hantek_configure_adc_range_scaling(struct hantek_device *dev);

/**
 * Start capture
 */
HRESULT hantek_start_capture(struct hantek_device *dev, enum hantek_capture_mode mode);

/**
 * Get status from the oscilloscope
 */
HRESULT hantek_get_status(struct hantek_device *dev, bool *pdata_ready);

/**
 * Configure the ADC's routing, based on the current channel setup
 */
HRESULT hantek_configure_adc_routing(struct hantek_device *dev);

/**
 * Retrieve sample buffer, if one is ready
 */
HRESULT hantek_retrieve_buffer(struct hantek_device *dev, uint8_t *ch1, uint8_t *ch2, uint8_t *ch3, uint8_t *ch4);
