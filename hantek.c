#include <hantek.h>
#include <hantek_priv.h>
#include <libusb.h>

static
HRESULT _hantek_device_find(libusb_device **pdev)
{
    HRESULT ret = H_OK;

    libusb_device **devs = NULL;
    libusb_device *dev = NULL;
    ssize_t nr_devs = 0;

    if (NULL == pdev) {
        ret = H_ERR_BAD_ARGS;
        DEBUG("Bad arguments");
        goto done;
    }

    *pdev = NULL;

    libusb_init(NULL);

    if (0 >= (nr_devs = libusb_get_device_list(NULL, &devs))) {
        DEBUG("Failed to get libusb device list, aborting (reason: %zd).", nr_devs);
        ret = H_ERR_NOT_FOUND;
        goto done;
    }

    DEBUG("There are %zd devices connected to the system, walking the list.", nr_devs);

    for (size_t i = 0; i < (size_t)nr_devs; i++) {
        libusb_device *ndev = devs[i];
        struct libusb_device_descriptor desc;

        if (0 != libusb_get_device_descriptor(ndev, &desc)) {
            DEBUG("Failed to get device descriptor at device ID %zu, skipping", i);
            continue;
        }

        if (desc.idVendor != HANTEK_VID) {
            continue;
        }

        if (desc.idProduct != HANTEK_PID_6254BD) {
            DEBUG("Got a Hantek device, PID = %04x", desc.idProduct);
            continue;
        }

        DEBUG("Found a candidate device, %p, breaking the loop", ndev);
        dev = ndev;
        break;
    }

    if (NULL == dev) {
        DEBUG("No supported devices found, aborting.");
        ret = H_ERR_NOT_FOUND;
        goto done;
    }

    libusb_ref_device(dev);
    *pdev = dev;

done:
    if (NULL != devs) {
        libusb_free_device_list(devs, 1);
    }
    return ret;
}

HRESULT hantek_open_device(struct hantek_device **pdev)
{
    HRESULT ret = H_OK;

    libusb_device *dev = NULL;

    if (NULL == pdev) {
        ret = H_ERR_BAD_ARGS;
        DEBUG("Bad arguments");
        goto done;
    }

    if (H_FAILED(ret = _hantek_device_find(&dev))) {
        DEBUG("Failed to find device, aborting.");
        goto done;
    }

done:
    if (H_FAILED(ret)) {
        if (NULL != dev) {
            libusb_unref_device(dev);
        }
    }
    return ret;
}

