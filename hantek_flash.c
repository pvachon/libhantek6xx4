#include <hantek_flash.h>
#include <hantek_priv.h>
#include <hantek_usb.h>

#include <libusb.h>

#define HT_BITSTREAM_FLASH_IO_MAX_SIZE                0x40

HRESULT hantek_read_bitstream_flash(struct hantek_device *dev, uint8_t *target_buffer, size_t buffer_length)
{
    HRESULT ret = H_OK;

    int uret = 0;

    HASSERT_ARG(NULL != dev);
    HASSERT_ARG(NULL != target_buffer);
    HASSERT_ARG(HT_BITSTREAM_FLASH_SIZE == buffer_length);

    for (size_t i = 0; i < HT_BITSTREAM_FLASH_SIZE; i += HT_BITSTREAM_FLASH_IO_MAX_SIZE) {
        if (0 >= (uret = libusb_control_transfer(dev->hdl,
                        LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
                        HT_REQUEST_BITSTREAM_FLASH_ACCESS,
                        HT_VALUE_READ_WRITE_BITSTREAM_FLASH,
                        0x0,
                        target_buffer + i,
                        HT_BITSTREAM_FLASH_IO_MAX_SIZE,
                        0)))
        {
            DEBUG("Failed to read bitstream flash data at offset 0x%08zx, aborting. Reasion: %d", i, uret);
            ret = H_ERR_CONTROL_FAIL;
            goto done;
        }
    }

done:
    return ret;
}
