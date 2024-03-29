#pragma once

/**
 * Constants associated with the Hantek 6000 USB on-the-wire protocol
 */

#define HT6000_EP_OUT                       2
#define HT6000_EP_IN                        6

/**
 * Control transfer types
 */

/**
 * Control transfer sent at initialization time. This seems to consist of a 0xa byte transfer, from Host-to-Device.
 */
#define HT_REQUEST_INITIALIZE               0xb3

/**
 * Get the device information string. This returns a string formatted as follows:
 *
 * The length of this transfer is always 0x47, and the wValue is 0x1580. Always device-to-host of course.
 */
#define HT_REQUEST_GET_INFO                 0xa2
#define HT_VALUE_GET_INFO_STRING			0x1580
#define HT_VALUE_GET_CALIBRATION_DAT        0x1600
#define HT_MAX_INFO_STRING_LEN              0x47
#define HT_CALIBRATION_INFO_ENTRIES         (((12 * 12) * 4) + 1)

/**
 * Magic flag used to indicate calibration data is valid
 */
#define HT_CALIBRATION_NONZERO_FLAG         0xfbcf

/**
 * Requests related to the Flash
 */
#define HT_REQUEST_BITSTREAM_FLASH_ACCESS   0xf1
#define HT_VALUE_READ_WRITE_BITSTREAM_FLASH 0x1e00

/**
 * It seems that a control transfer of type 0xb2 always is sent before sending data to EP2.
 * This might be for some kind of flow control? This is always device-to-host
 */
#define HT_REQUEST_CHECK_READY              0xb2

/**
 * Message IDs
 */

/**
 * Channel Position Configuration. Each channel has a unique command to adjust its
 * location on the virtual graticule.
 * Length: 4 bytes
 *  - 2 bytes: Little Endian Command ID (one of the below for the 4 channels)
 *  - 2 bytes: Channel position, little endian.
 *
 * Calculating the channel position is a bit involved, but seems to require scaling
 * between 0-255, w.r.t. some calibration data pulled from the I2C EEPROM.
 */
#define HT_MSG_POSITION_CH0                 0x00
#define HT_MSG_POSITION_CH1                 0x01
#define HT_MSG_POSITION_CH2                 0x02
#define HT_MSG_POSITION_CH3                 0x04

#define HT_MSG_SEND_START_CAPTURE           0x03
#define HT_MSG_READBACK_BUFFER              0x05
#define HT_MSG_GET_STATUS                   0x06
#define HT_MSG_SET_TRIGGER_LEVEL            0x07

/**
 * Send up to 32 bits of data across the device's SPI bus
 * Length: 8 bytes
 *  - 2 bytes: Little Endian Command ID
 *  - 4 bytes: Bytes to clock out (big endian - last byte specified, first byte clocked)
 *  - 2 bytes: Chip Select/Device ID
 *  struct ht_msg_spi {
 *      uint16_t usb_msg;
 *      uint8_t spi_msg_be[4];
 *      uint16_t spi_cs; // documented below
 *  }
 */
#define HT_MSG_SEND_SPI                     0x08

#define HT_MSG_GET_HW_VERSION               0x09
/* missing 0x0a */
/* missing 0x0b */
#define HT_MSG_INITIALIZE                   0x0c
#define HT_MSG_BUFFER_STATUS                0x0d
#define HT_MSG_BUFFER_PREPARE_TRANSFER      0x0e

/**
 * USB Message to set time division
 * Length: 6 bytes
 *  - 2 bytes: Little Endian Command ID (0xf)
 *  - 4 bytes: time base, little endian, in nanoseconds - 1
 */
#define HT_MSG_SET_TIME_DIVISION            0x0f

/**
 * TODO
 * USB Message to set horizontal trigger position.
 * Length: 14 bytes
 *  - 2 bytes: Little Endian Command ID
 *  - 6 bytes: looks like buffer bytes before trigger?
 *  - 6 bytes: hysteresis of some sort? Hard to say?
 */
#define HT_MSG_SET_TRIG_HORIZ_POS           0x10

/**
 * USB Message to set trigger mode
 * Length: 6 bytes
 *  - 2 bytes: Little Endian Command ID
 *  - 1 byte: Trigger Mode
 *  - 1 byte: Trigger Slope (+/-)
 *  - 1 byte: Trigger Coupling
 *  - 1 byte: SBZ
 */
#define HT_MSG_CONFIGURE_TRIGGER            0x11
#define HT_MSG_SET_TRIGGER_SOURCE           0x12
/**
 * USB Message to set up a special trigger (i.e. pulse or video)
 *  - 2 bytes: Little Endian Command ID (0x13)
 *
 */
#define HT_MSG_SET_SPECIAL_TRIGGER          0x13

/**
 * Channel configuration byte bitshifts
 */
#define HT_CHAN_BW_LIMIT_SHIFT              7
#define HT_CHAN_GT1V_SHIFT                  6
#define HT_CHAN_LE1V_SHIFT                  5
#define HT_CHAN_GT100MV_SHIFT               4
#define HT_CHAN_LE100MV_SHIFT               3
#define HT_CHAN_COUPLING_SHIFT              2

/**
 * Trigger level magic numbers
 */
#define HT_TRIGGER_MAX_VALUE                0xe4

/**
 * Status bit values
 */
#define HT_STATUS_TRIGGERED                 (1 << 0)
#define HT_STATUS_DATA_READY                (1 << 1)
#define HT_STATUS_PACK_STATE                (1 << 3)
#define HT_STATUS_SDRAM_INIT                (1 << 4)

/**
 * SPI Chip Selects
 */

/**
 * Shift registers for front-end control
 */
#define HT_SPI_CS_SHIFT_REG                 0x1

/**
 * ADF4360-2 Control Latches
 */
#define HT_SPI_CS_ADF4360                   0x2

/**
 * HMCAD1511 Control Registers
 */
#define HT_SPI_CS_HMCAD1511                 0x4

