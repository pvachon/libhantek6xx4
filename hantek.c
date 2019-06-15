#include <hantek.h>
#include <hantek_priv.h>
#include <hantek_usb.h>

#include <hantek_hexdump.h>

#include <libusb.h>

#include <stdbool.h>
#include <string.h>

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

static
HRESULT _hantek_check_ready(libusb_device_handle *dev, bool *pready)
{
	HRESULT ret = H_OK;

    int uret = 0;
    uint8_t raw[10] = { 0 };

    if (NULL == pready) {
        DEBUG("Bad arguments");
        ret = H_ERR_BAD_ARGS;
        goto done;
    }
    *pready = false;

    if (0 >= (uret = libusb_control_transfer(dev,
                    LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
                    HT_REQUEST_CHECK_READY,
                    0x0,
                    0x0,
                    raw,
                    sizeof(raw),
                    0)))
    {
        DEBUG("Failed to request if EP2/3 are ready, aborting. (reason = %d)", uret);
        ret = H_ERR_CONTROL_FAIL;
        goto done;
    }

    if (uret >= 1) {
        if (raw[0] == 0x1) {
            *pready = true;
        }
    }

done:
    return ret;
}

static
HRESULT _hantek_get_id_string(libusb_device_handle *dev, char *id_string, size_t len)
{
    HRESULT ret = H_OK;

    int uret = 0;
    uint8_t raw[HT_MAX_INFO_STRING_LEN] = { 0 };
    size_t outlen = HT_MAX_INFO_STRING_LEN > len ? len : HT_MAX_INFO_STRING_LEN;

    if (NULL == id_string || 0 == len) {
        DEBUG("Bad arguments");
        ret = H_ERR_BAD_ARGS;
        goto done;
    }

    if (0 >= (uret = libusb_control_transfer(dev,
                    LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
                    HT_REQUEST_GET_INFO,
                    HT_VALUE_GET_INFO_STRING,
                    0x0,
                    raw,
                    sizeof(raw),
                    0)))
    {
        DEBUG("Failed to request device ID string, (uret = %d), aborting.", uret);
        ret = H_ERR_CONTROL_FAIL;
        goto done;
    }

    memcpy(id_string, raw, outlen);

done:
    return ret;
}

HRESULT hantek_open_device(struct hantek_device **pdev)
{
    HRESULT ret = H_OK;

    libusb_device *dev = NULL;
    libusb_device_handle *hdl = NULL;
    struct hantek_device *nhdev = NULL;
    bool ready = false;
    int uret = -1;
    char id_str[HT_MAX_INFO_STRING_LEN] = { 0 };

    if (NULL == pdev) {
        ret = H_ERR_BAD_ARGS;
        DEBUG("Bad arguments");
        goto done;
    }

    if (H_FAILED(ret = _hantek_device_find(&dev))) {
        DEBUG("Failed to find device, aborting.");
        goto done;
    }

    if (0 != (uret = libusb_open(dev, &hdl))) {
        DEBUG("Failed to open USB device, aborting. Uret = %u)", (unsigned)uret);
        ret = H_ERR_CANT_OPEN;
        goto done;
    }

    /* Claim interface 0 */
    if (0 != (uret = libusb_claim_interface(hdl, 0))) {
        DEBUG("Failed to claim interface 0, aborting. uret = %u", (unsigned)uret);
        ret = H_ERR_CANT_OPEN;
        goto done;
    }

    /* Given the libUSB device handle, let's interrogate it a bit */
    ready = false;
    if (H_FAILED(ret = _hantek_check_ready(hdl, &ready))) {
        DEBUG("Failed to check if device is not ready.");
        goto done;
    }

    if (false == ready) {
        DEBUG("Device is not ready, aborting.");
        ret = H_ERR_NOT_READY;
        goto done;
    }

    if (H_FAILED(ret = _hantek_get_id_string(hdl, id_str, sizeof(id_str)))) {
        goto done;
    }

#ifdef HT_DEBUG
    hexdump_dump_hex(id_str, sizeof(id_str));
#endif

    /* TODO: parse out what we need from the ID string */

done:
    if (H_FAILED(ret)) {
        if (NULL != hdl) {
            libusb_close(hdl);
        }

        if (NULL != dev) {
            libusb_unref_device(dev);
        }
    }
    return ret;
}

