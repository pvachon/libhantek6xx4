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
radix: hexadecimal
	44 53 4F FF FF FF 36 30 30 30 FF FF FF FF 56 31
	2E 30 36 FF 44 31 2E 30 30 FF 4D 30 30 32 43 30
	31 31 36 34 44 30 33 34 39 31 32 30 31 38 31 31
	31 39 32 30 31 38 31 31 31 39 54 30 33 33 46 32
	C0 30 31 FF 07 FF FF
or
radix: ascii
DSO...6000....V1
.06.D1.00.M002C0
1164D03491201811
1920181119T033F2
.01....
 * The length of this transfer is always 0x47, and the wValue is 0x1580. Always device-to-host of course.
 */
#define HT_REQUEST_GET_INFO                 0xa2
#define HT_VALUE_GET_INFO_STRING			0x1580
#define HT_VALUE_GET_CALIBRATION_DAT        0x1600
#define HT_MAX_INFO_STRING_LEN              0x47
#define HT_CALIBRATION_INFO_ENTRIES         576

/**
 * It seems that a control transfer of type 0xb2 always is sent before sending data to EP2.
 * This might be for some kind of flow control? This is always device-to-host
 */
#define HT_REQUEST_CHECK_READY              0xb2

/**
 * Message IDs, to the best of our guesses
 */
#define HT_MSG_CONFIGURE_FRONTEND           0x08
#define HT_MSG_GET_HW_VERSION               0x09
#define HT_MSG_INITIALIZE                   0x0c
#define HT_MSG_SET_TIME_DIVISION            0x0f

/**
 * Channel configuration byte bitshifts
 */
#define HT_CHAN_BW_LIMIT_SHIFT              7
#define HT_CHAN_LT1V_SHIFT                  6
#define HT_CHAN_GE1V_SHIFT                  5
#define HT_CHAN_LT100MV_SHIFT               4
#define HT_CHAN_GE100MV_SHIFT               3
#define HT_CHAN_COUPLING_SHIFT              2
