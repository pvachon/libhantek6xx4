#pragma once

#include <hantek.h>
#include <libusb.h>

#define HANTEK_VID          0x04b5

#define HANTEK_PID_6254BD   0x6cde

#include  <stdio.h>
#define DEBUG(x, ...) do { printf("DEBUG: " x " (%s @ %s:%d)\n", ##__VA_ARGS__, __FUNCTION__, __FILE__, __LINE__); } while (0)

struct hantek_device {
    struct libusb_device *dev;
};

