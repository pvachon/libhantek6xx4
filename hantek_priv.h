#pragma once

#include <hantek.h>
#include <hantek_usb.h>
#include <libusb.h>
#include <stdint.h>

#define HANTEK_VID              0x04b5

#define HANTEK_PID_6254BD       0x6cde

#define HT_SERIAL_NUMBER_LEN    8

#ifdef HT_DEBUG
#include  <stdio.h>
#define DEBUG(x, ...) do { printf("DEBUG: " x " (%s @ %s:%d)\n", ##__VA_ARGS__, __FUNCTION__, __FILE__, __LINE__); } while (0)
#else
#define DEBUG(...)
#endif /* defined(HT_DEBUG) */

#define HASSERT_ARG(x) do { if (!((x))) { DEBUG("Bad Argument"); return H_ERR_BAD_ARGS; } } while (0)

#define HT_MAX_CHANNELS     4

struct hantek_channel {
    /**
     * Whether or not this channel is enabled
     */
    bool enabled;

    /**
     * Whether or not front-end bandwidth limiting is enabled
     */
    bool bw_limit;

    /**
     * Volts per division for this channel
     */
    enum hantek_volts_per_div vpd;

    /**
     * Coupling mode (AC or DC coupling)
     */
    enum hantek_coupling coupling;
};

struct hantek_device {
    struct libusb_device *dev;
    struct libusb_device_handle *hdl;

    /**
     * The PCB revision
     */
    int pcb_revision;

    /**
     * Serial number
     */
    char serial_number[HT_SERIAL_NUMBER_LEN + 1];

    /**
     * Length of the capture buffer, in bytes
     */
    size_t capture_buffer_len;

    /**
     * FPGA version, read back after reset
     */
    uint16_t fpga_version;

    /**
     * Configuration per channel
     */
    struct hantek_channel channels[HT_MAX_CHANNELS];

    /**
     * ID string read back from the device
     */
    char id_string[HT_MAX_INFO_STRING_LEN];

    /**
     * Calibration data for this device
     */
    uint16_t cal_data[HT_CALIBRATION_INFO_ENTRIES];
};

