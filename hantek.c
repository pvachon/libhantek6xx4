#include <hantek.h>
#include <hantek_priv.h>
#include <hantek_usb.h>

#include <hantek_hexdump.h>

#include <libusb.h>

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <hmcad1511.h>

/**
 * Table mapping volts/division to ADC coarse gain values for the HMCAD1511
 */
static
uint8_t _hantek_vpd_gain_mapping[12] = {
    0xd,
    0xa,
    0x7,
    0x5,
    0x2,
    0x0,
    0x5,
    0x2,
    0x0,
    0x5,
    0x2,
    0x0,
};

static inline
uint8_t __hantek_device_chan_mask(struct hantek_device *dev)
{
    uint8_t mask = 0;
    struct hantek_channel *chans = dev->channels;

    for (size_t i = 0; i < 4; i++) {
        mask |= ((chans[i].enabled == true) << i);
    }

    return mask;
}

static inline
uint8_t __hantek_device_nr_active_chans(struct hantek_device *dev)
{
    uint8_t chan_cnt = 0;
    struct hantek_channel *chans = dev->channels;

    for (size_t i = 0; i < 4; i++) {
        chan_cnt += (chans[i].enabled == true);
    }

    return chan_cnt;
}

static
HRESULT _hantek_device_find(libusb_device **pdev)
{
    HRESULT ret = H_OK;

    libusb_device **devs = NULL;
    libusb_device *dev = NULL;
    ssize_t nr_devs = 0;

    HASSERT_ARG(NULL != pdev);

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
HRESULT _hantek_check_usb_1(libusb_device_handle *dev, bool *pready)
{
	HRESULT ret = H_OK;

    int uret = 0;
    uint8_t raw[10] = { 0 };

    HASSERT_ARG(NULL != dev);
    HASSERT_ARG(NULL != pready);

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
        DEBUG("Failed to request if we are ready for commands, aborting. (reason = %d)", uret);
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
HRESULT _hantek_bulk_cmd_out(libusb_device_handle *dev, uint8_t *data, size_t len, size_t *ptransferred)
{
    HRESULT ret = H_OK;

    uint8_t cmd_start[10] = { 0x0f, 0x03, 0x03, 0x03 };
    int uret = -1,
        transferred = 0;
    bool ready = false;

    HASSERT_ARG(NULL != dev);
    HASSERT_ARG(NULL != data);
    HASSERT_ARG(0 != len);
    HASSERT_ARG(NULL != ptransferred);

    *ptransferred = 0;

    /* Send the magical "start of transaction" command */
    if (0 >= (uret = libusb_control_transfer(dev,
                    LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
                    HT_REQUEST_INITIALIZE,
                    0x0,
                    0x0,
                    cmd_start,
                    sizeof(cmd_start),
                    0)))
    {
        DEBUG("Failed to request if data is ready, aborting. (reason = %d)", uret);
        ret = H_ERR_CONTROL_FAIL;
        goto done;
    }

    /* Check if the device is in USB 1 mode */
    if (H_FAILED(ret = _hantek_check_usb_1(dev, &ready))) {
        DEBUG("Was unable to check if in USB 1 mode, aborting.");
        goto done;
    }

    if (false == ready) {
        DEBUG("Device is not in USB 2.0 mode");
        ret = H_ERR_CONTROL_FAIL;
        goto done;
    }

    /* Send the actual bulk command */
    if (0 != (uret = libusb_bulk_transfer(dev,
                                          HT6000_EP_OUT,
                                          data,
                                          len,
                                          &transferred,
                                          0)))
    {
        DEBUG("Failure during bulk OUT transfer: %d", uret);
        ret = H_ERR_NOT_READY;
        goto done;
    }

    *ptransferred = transferred;

done:
    return ret;
}

static
HRESULT _hantek_bulk_in(libusb_device_handle *hdl, uint8_t *data, size_t buf_len, size_t *ptransferred)
{
    HRESULT ret = H_OK;

    int transferred = 0,
        uret = 0;

    HASSERT_ARG(NULL != hdl);
    HASSERT_ARG(NULL != data);
    HASSERT_ARG(0 != buf_len);
    HASSERT_ARG(NULL != ptransferred);

    if (0 != (uret = libusb_bulk_transfer(hdl,
                                          HT6000_EP_IN | (1 << 7),
                                          data,
                                          buf_len,
                                          &transferred,
                                          0)))
    {
        DEBUG("Failure during bulk IN transfer: %d", uret);
        ret = H_ERR_NOT_READY;
        goto done;
    }

    *ptransferred = transferred;
done:
    return ret;
}

static
HRESULT _hantek_bulk_read_in(libusb_device_handle *hdl, uint8_t *dst_buf, size_t dst_len)
{
    HRESULT ret = H_OK;

    size_t transferred = 0,
           to_transfer = 0x40;
    bool usb2 = false;
    uint8_t rx_buf[512];

    HASSERT_ARG(NULL != hdl);
    HASSERT_ARG(NULL != dst_buf);
    HASSERT_ARG(0 != dst_len);

    if (H_FAILED(ret = _hantek_check_usb_1(hdl, &usb2))) {
        DEBUG("Failed to check if device is in USB 1.0 mode");
        goto done;
    }

    if (true == usb2) {
        /* You transfers are 512 bytes at a time in USB 2.0 */
        to_transfer = 512;
    }

    if (H_FAILED(ret = _hantek_bulk_in(hdl, rx_buf, to_transfer, &transferred))) {
        DEBUG("Failure to do bulk read, aborting.");
        goto done;
    }

    if (transferred != to_transfer) {
        DEBUG("Expected to receive %zu, got %zu, aborting.", to_transfer, transferred);
        goto done;
    }

    memcpy(dst_buf, rx_buf, dst_len);

done:
    return ret;
}

static
HRESULT _hantek_read_config_attrs(libusb_device_handle *hdl, uint16_t value, void *dst, size_t len_bytes)
{
    HRESULT ret = H_OK;

    int uret = 0;

    HASSERT_ARG(NULL != hdl);
    HASSERT_ARG(NULL != dst);
    HASSERT_ARG(0 != len_bytes);

    if (0 >= (uret = libusb_control_transfer(hdl,
                    LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
                    HT_REQUEST_GET_INFO,
                    value,
                    0x0,
                    dst,
                    len_bytes,
                    0)))
    {
        DEBUG("Failed to request device attribute %04x, (uret = %d), aborting.", value, uret);
        ret = H_ERR_CONTROL_FAIL;
        goto done;
    }

    DEBUG("! Read back %04x: %d bytes (expected = %zu)", value, uret, len_bytes);

done:
    return ret;
}

static
HRESULT _hantek_get_id_string(libusb_device_handle *dev, char *id_string, size_t len)
{
    HRESULT ret = H_OK;

    HASSERT_ARG(NULL != id_string);
    HASSERT_ARG(0 != len);

    if (H_FAILED(ret = _hantek_read_config_attrs(dev, HT_VALUE_GET_INFO_STRING, id_string, len))) {
        DEBUG("Failed to read back ID string, aborting.");
        goto done;
    }

done:
    return ret;
}

static
HRESULT _hantek_device_reset(libusb_device_handle *hdl)
{
    HRESULT ret = H_OK;

    static uint8_t reset_cmd[2] = { HT_MSG_INITIALIZE, 0x0 };
    size_t transferred = 0;

    DEBUG("----> Sending device reset");

    HASSERT_ARG(NULL != hdl);

    if (H_FAILED(ret = _hantek_bulk_cmd_out(hdl, reset_cmd, sizeof(reset_cmd), &transferred))) {
        DEBUG("Failed to send the reset command");
        goto done;
    }

    if (2 != transferred) {
        DEBUG("Transferred %zu bytes, expected 2", transferred);
        goto done;
    }

done:
    return ret;
}

static
HRESULT _hantek_wake_device(libusb_device_handle *hdl)
{
    HRESULT ret = H_OK;

    static
    uint8_t wake_cmds[5][8] = {
        { HT_MSG_SEND_SPI, 0x00, 0x00, 0x77, 0x47, HMCAD1511_REG_LVDS_TERM,     HT_SPI_CS_HMCAD1511, 0x00 },
        { HT_MSG_SEND_SPI, 0x00, 0x00, 0x03, 0x00, HMCAD1511_REG_GAIN_CONTROL,  HT_SPI_CS_HMCAD1511, 0x00 },
        { HT_MSG_SEND_SPI, 0x00, 0x00, 0x65, 0x00, 0x30, HT_SPI_CS_ADF4360, 0x00 },
        { HT_MSG_SEND_SPI, 0x00, 0x00, 0x28, 0xF1, 0x0F, HT_SPI_CS_ADF4360, 0x00 },
        { HT_MSG_SEND_SPI, 0x00, 0x00, 0x12, 0x38, 0x01, HT_SPI_CS_ADF4360, 0x00 }
    };

    HASSERT_ARG(NULL != hdl);

    for (size_t i = 0; i < 5; i++) {
        size_t transferred = 0;

        if (H_FAILED(ret = _hantek_bulk_cmd_out(hdl, wake_cmds[i], 8, &transferred))) {
            DEBUG("Failure while transmitting wakeup command %zu", i);
            goto done;
        }

        if (transferred != 8) {
            DEBUG("Transferred %zu bytes, expected 8 for a wakeup command", transferred);
            goto done;
        }
    }

done:
    return ret;
}

static
HRESULT _hantek_get_fpga_version(libusb_device_handle *hdl, uint16_t *pfpga_ver)
{
    HRESULT ret = H_OK;

    size_t transferred = 0,
           to_transfer = 0x40;
    bool ready = false;
    uint8_t rx_buf[512];

    HASSERT_ARG(NULL != hdl);
    HASSERT_ARG(NULL != pfpga_ver);

    *pfpga_ver = 0;

    if (H_FAILED(ret = _hantek_check_usb_1(hdl, &ready))) {
        goto done;
    }

    if (true == ready) {
        DEBUG("Using a large buffer to get the FPGA version");
        to_transfer = 512;
    }

    if (H_FAILED(ret = _hantek_bulk_in(hdl, rx_buf, to_transfer, &transferred))) {
        DEBUG("Failure while getting attributes from the oscilloscope, aborting.");
        goto done;
    }

    if (transferred != to_transfer) {
        DEBUG("Expected to receive %zu, got %zu, aborting.", to_transfer, transferred);
        goto done;
    }

    /* Extract the FPGA version ID */
    *pfpga_ver = (uint16_t)rx_buf[1] << 8 | rx_buf[0];

    DEBUG("FPGA version: %04x", *pfpga_ver);

done:
    return ret;
}

static
HRESULT _hantek_get_hardware_rev(libusb_device_handle *hdl, uint32_t *phw_rev)
{
    HRESULT ret = H_OK;

    uint8_t msg[2] = { HT_MSG_GET_HW_VERSION, 0x00 };
    size_t transferred = 0;
    uint32_t version = 0;

    HASSERT_ARG(NULL != hdl);
    HASSERT_ARG(NULL != phw_rev);

    *phw_rev = 0;

    if (H_FAILED(ret = _hantek_bulk_cmd_out(hdl, msg, sizeof(msg), &transferred))) {
        DEBUG("Failed to send get hardware revision command.");
        goto done;
    }

    if (H_FAILED(ret = _hantek_bulk_read_in(hdl, (void *)&version, sizeof(version)))) {
        DEBUG("Failed to read back hardware revision code, aborting.");
        goto done;
    }

    DEBUG("Hardware revision: %08x", version);

    *phw_rev = version;

done:
    return ret;
}

static
HRESULT _hantek_get_calibration_data(libusb_device_handle *hdl, uint16_t *cal_data, size_t nr_cal_vals)
{
    HRESULT ret = H_OK;

    HASSERT_ARG(NULL != hdl);
    HASSERT_ARG(NULL != cal_data);
    HASSERT_ARG(HT_CALIBRATION_INFO_ENTRIES == nr_cal_vals);

    if (H_FAILED(ret = _hantek_read_config_attrs(hdl, HT_VALUE_GET_CALIBRATION_DAT, cal_data, sizeof(uint16_t) * nr_cal_vals))) {
        DEBUG("Failed to read back calibration values, aborting.");
        goto done;
    }

    if (HT_CALIBRATION_NONZERO_FLAG != cal_data[nr_cal_vals - 1]) {
        DEBUG("!!! Calibration data is not set. Flag: %04x.", (unsigned)cal_data[nr_cal_vals - 1]);
        ret = H_ERR_NOT_READY;
        goto done;
    }

done:
    return ret;
}

HRESULT hantek_open_device(struct hantek_device **pdev, uint32_t capture_buffer_len)
{
    HRESULT ret = H_OK;

    libusb_device *dev = NULL;
    libusb_device_handle *hdl = NULL;
    struct hantek_device *nhdev = NULL;
    int uret = -1;

    HASSERT_ARG(NULL != pdev);
    HASSERT_ARG(0 < capture_buffer_len && (1 << 16) >= capture_buffer_len);

    *pdev = NULL;

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

    if (NULL == (nhdev = calloc(1, sizeof(struct hantek_device)))) {
        DEBUG("Out of memory");
        ret = H_ERR_NO_MEM;
        goto done;
    }

    nhdev->dev = dev;
    nhdev->hdl = hdl;
    nhdev->capture_buffer_len = capture_buffer_len;

    /* Send what looks to be a reset command */
    if (H_FAILED(ret = _hantek_device_reset(hdl))) {
        goto done;
    }

    /* Read back the FPGA version */
    if (H_FAILED(ret = _hantek_get_fpga_version(hdl, &nhdev->fpga_version))) {
        DEBUG("Failed to get FPGA version, aborting.");
        goto done;
    }

    /* Send the wakeup sequence (seems to be all magical from static analysis) */
    if (H_FAILED(ret = _hantek_wake_device(hdl))) {
        DEBUG("Failure while trying to wake the device, aborting.");
        goto done;
    }

    /* Get the ID string */
    if (H_FAILED(ret = _hantek_get_id_string(hdl, nhdev->id_string, HT_MAX_INFO_STRING_LEN))) {
        goto done;
    }

    /* Parse out the PCB revision - this is bonkers but the only place we can get it */
    for (size_t i = 0; i < 5; i++) {
        int norm = nhdev->id_string[14 + i] - '0';

        if (norm > 9 || norm < 0) {
            continue;
        }

        nhdev->pcb_revision *= 10;
        nhdev->pcb_revision += norm;
    }

    /* Parse out the device serial number */
    memcpy(nhdev->serial_number, &nhdev->id_string[20], 8);
    nhdev->serial_number[8] = '\0';

    DEBUG("PCB Revision: %d", nhdev->pcb_revision);
    DEBUG("    Serial Number: %s", nhdev->serial_number);

#ifdef HT_DEBUG
    hexdump_dump_hex(nhdev->id_string, HT_MAX_INFO_STRING_LEN);
#endif

    /* Grab the calibration data for this device */
    if (H_FAILED(ret = _hantek_get_calibration_data(hdl, nhdev->cal_data, HT_CALIBRATION_INFO_ENTRIES))) {
        DEBUG("Failed to get device calibration data, aborting.");
        goto done;
    }

    /* Get the hardware revision */
    if (H_FAILED(ret = _hantek_get_hardware_rev(hdl, &nhdev->hardware_rev))) {
        DEBUG("Failed to get hardware revision, aborting.");
        goto done;
    }


#ifdef HT_DEBUG
    printf("Calibration data:\n");
    hexdump_dump_hex(nhdev->cal_data, HT_CALIBRATION_INFO_ENTRIES * sizeof(uint16_t));
#endif

    *pdev = nhdev;
done:
    if (H_FAILED(ret)) {
        if (NULL != nhdev) {
            free(nhdev);
            nhdev = NULL;
        }

        if (NULL != hdl) {
            libusb_close(hdl);
        }

        if (NULL != dev) {
            libusb_unref_device(dev);
        }
    }
    return ret;
}

HRESULT hantek_close_device(struct hantek_device **pdev)
{
    HRESULT ret = H_OK;

    struct hantek_device *hdev = NULL;

    HASSERT_ARG(NULL != pdev);
    HASSERT_ARG(NULL != *pdev);

    hdev = *pdev;

    if (NULL != hdev->hdl) {
        libusb_close(hdev->hdl);
        hdev->hdl = NULL;
    }

    if (NULL != hdev->dev) {
        libusb_unref_device(hdev->dev);
        hdev->dev = NULL;
    }

    return ret;
}

HRESULT hantek_get_status(struct hantek_device *dev, bool *pdata_ready)
{
    HRESULT ret =  H_OK;

    uint8_t message[2] = { HT_MSG_GET_STATUS, 0x0 };
    uint8_t status = 0;
    size_t transferred = 0;

    HASSERT_ARG(NULL != dev);

    if (H_FAILED(_hantek_bulk_cmd_out(dev->hdl, message, sizeof(message), &transferred))) {
        DEBUG("Failed to send status message, aborting.");
        ret = H_ERR_NOT_READY;
        goto done;
    }

    if (sizeof(message) != transferred) {
        DEBUG("Sent less data than expected");
        ret = H_ERR_NOT_READY;
        goto done;
    }

    if (H_FAILED(_hantek_bulk_read_in(dev->hdl, &status, 1))) {
        DEBUG("Failed ot read back status, aborting.");
        ret = H_ERR_NOT_READY;
        goto done;
    }

    DEBUG("*** STATUS BYTE: 0x%02x ***", status);

    if (NULL != pdata_ready) {
        *pdata_ready = !!(status & HT_STATUS_DATA_READY);
    }

done:
    return ret;
}

static
uint8_t _hantek_channel_setup(enum hantek_volts_per_div volts_per_div, enum hantek_coupling coupling, bool bandwidth_limit)
{
    uint8_t state = 0;

    state |= (bandwidth_limit == true) << HT_CHAN_BW_LIMIT_SHIFT;
    state |= (volts_per_div > HT_VPD_1V) << HT_CHAN_GT1V_SHIFT;
    state |= (volts_per_div <= HT_VPD_1V) << HT_CHAN_LE1V_SHIFT;
    state |= (volts_per_div > HT_VPD_100MV) << HT_CHAN_GT100MV_SHIFT;
    state |= (volts_per_div <= HT_VPD_100MV) << HT_CHAN_LE100MV_SHIFT;
    state |= (coupling == HT_COUPLING_DC) << HT_CHAN_COUPLING_SHIFT;
    state |= 1 << 1;

    return state;
}

static
HRESULT _hantek_commit_frontend_config(struct hantek_device *dev)
{
    HRESULT ret = H_OK;

    uint8_t message[8] = { HT_MSG_SEND_SPI, 0x0, 0x0, 0x0, 0x0, 0x0, HT_SPI_CS_SHIFT_REG, 0x0 };
    size_t transferred = 0;

    HASSERT_ARG(NULL != dev);

    /* Fill in the settings for each channel */
    for (size_t i = 0; i < HT_MAX_CHANNELS; i++) {
        struct hantek_channel *chan = &dev->channels[i];
        message[2 + i] = _hantek_channel_setup(chan->vpd, chan->coupling, chan->bw_limit);
    }

    if (H_FAILED(ret = _hantek_bulk_cmd_out(dev->hdl, message, sizeof(message), &transferred))) {
        DEBUG("Failed to send frontend configuration, aborting.");
        goto done;
    }

    if (sizeof(message) != transferred) {
        DEBUG("Failed to transfer frontend configuration, aborting.");
        ret = H_ERR_NOT_READY;
        goto done;
    }

    transferred = 0;

    /* This is what the SDK does */
    usleep(4000);

    /* This seems to force latching */
    message[7] = 0x1;

    for (size_t i = 0; i < HT_MAX_CHANNELS; i++) {
        message[2 + i] &= (1 << HT_CHAN_BW_LIMIT_SHIFT) |
                          (1 << HT_CHAN_COUPLING_SHIFT) |
                          (1 << 1);
    }

    if (H_FAILED(ret = _hantek_bulk_cmd_out(dev->hdl, message, sizeof(message), &transferred)))  {
        DEBUG("Failed to send second frontend configuration command, aborting.");
        goto done;
    }

    if (sizeof(message) != transferred) {
        DEBUG("Failed to send second stage of frontend configuration command, aborting.");
        ret = H_ERR_NOT_READY;
        goto done;
    }

    /* This is also what the SDK does */
    usleep(50000);

done:
    return ret;
}


static
uint32_t _hantek_tpd_to_spacing[HT_ST_MAX] = {
    [HT_ST_2NS] = 1,
    [HT_ST_5NS] = 1,
    [HT_ST_10NS] = 1,
    [HT_ST_25NS] = 1,
    [HT_ST_50NS] = 1,
    [HT_ST_100NS] = 1,
    [HT_ST_250NS] = 1,
    [HT_ST_500NS] = 1,
    [HT_ST_1000NS] = 1,
    [HT_ST_2500NS] = 1,
    [HT_ST_5000NS] = 2,
    [HT_ST_10US] = 5,
    [HT_ST_25US] = 10,
    [HT_ST_50US] = 25,
    [HT_ST_100US] = 50,
    [HT_ST_250US] = 100,
    [HT_ST_500US] = 250,
    [HT_ST_1000US] = 500,
    [HT_ST_2500US] = 1000,
    [HT_ST_5000US] = 2500,
    [HT_ST_10MS] = 5000,
    [HT_ST_25MS] = 10000,
    [HT_ST_50MS] = 25000,
    [HT_ST_100MS] = 50000,
    [HT_ST_250MS] = 100000,
    [HT_ST_500MS] = 250000,
    [HT_ST_1S] = 500000,
};

HRESULT hantek_set_sampling_rate(struct hantek_device *dev, enum hantek_time_per_division sample_spacing)
{
    HRESULT ret = H_OK;

    uint8_t message[6] = { HT_MSG_SET_TIME_DIVISION, 0x0 };
    uint32_t spacing = 0;
    size_t transferred = 0;

    HASSERT_ARG(NULL != dev);

    static_assert((HT_ST_MAX * 4) == sizeof(_hantek_tpd_to_spacing),
            "You've added some time divisions but not updated the spacing array");

    spacing = _hantek_tpd_to_spacing[sample_spacing] - 1;

    message[2] = spacing & 0xff;
    message[3] = (spacing >> 8) & 0xff;
    message[4] = (spacing >> 16) & 0xff;
    message[5] = (spacing >> 24) & 0xff;

    if (H_FAILED(ret = _hantek_bulk_cmd_out(dev->hdl, message, sizeof(message), &transferred))) {
        DEBUG("Failed to set sampling rate.");
        ret = H_ERR_BAD_SAMPLE_RATE;
        goto done;
    }

done:
    return ret;
}

/**
 * Write an HMCAD1511 control register
 */
static
HRESULT _hantek_hmcad1511_write_reg(struct hantek_device *dev, uint8_t reg, uint16_t value)
{
    HRESULT ret = H_OK;

    uint8_t msg[8] = { HT_MSG_SEND_SPI, 0x00, 0x00, value & 0xff, (value >> 8) & 0xff, reg, HT_SPI_CS_HMCAD1511, 0x00 };
    size_t transferred = 0;

    HASSERT_ARG(NULL != dev);

    if (H_FAILED(ret = _hantek_bulk_cmd_out(dev->hdl, msg, sizeof(msg), &transferred))) {
        DEBUG("Failed to send HMCAD1511 SPI command (reg = %02x, value = %04x), aborting.", (unsigned)reg, (unsigned)value);
        goto done;
    }

    if (8 != transferred) {
        DEBUG("Failure: expected to transfer 8 bytes, sent %zu", transferred);
        ret = H_ERR_CONTROL_FAIL;
        goto done;
    }

    usleep(3000);

done:
    return ret;
}

/**
 * Set up the number of channels and clock divider. Requires we shut down the analog frontend,
 * per the HMCAD1511 manual.
 */
static
HRESULT _hantek_hmcad1511_set_channel_count(struct hantek_device *dev, size_t nr_chans)
{
    HRESULT ret = H_OK;

    uint8_t clk_div = 0,
            chan_mask  = 0;

    HASSERT_ARG(NULL != dev);
    HASSERT_ARG(0 != nr_chans && 5 > nr_chans);

    /* Power down the front-end */
    if (H_FAILED(ret = _hantek_hmcad1511_write_reg(dev, HMCAD1511_REG_SLEEP_PD, HMCAD1511_REG_SLEEP_PD_PD))) {
        goto done;
    }

    switch (nr_chans) {
    case 1:
        /* Divide by 1 */
        clk_div = 0;
        chan_mask = 0x1;
        break;
    case 2:
        /* Divide by 2 */
        clk_div = 1;
        chan_mask = 0x2;
        break;
    case 3:
    case 4:
        /* Divide by 4 */
        clk_div = 2;
        chan_mask = 0x4;
        break;
    default:
        ret = H_ERR_INVAL_CHANNELS;
        goto done;
    }

    if (H_FAILED(ret = _hantek_hmcad1511_write_reg(dev, HMCAD1511_REG_CHAN_NUM_CLK_DIV, (clk_div << 8) | chan_mask))) {
        goto done;
    }

    /* Restore power to the front-end */
    if (H_FAILED(ret = _hantek_hmcad1511_write_reg(dev, HMCAD1511_REG_SLEEP_PD, 0x0))) {
        goto done;
    }


done:
    return ret;
}

/**
 * Set the channel mappings for inputs to output channels, based on the channel configuration
 */
static
HRESULT _hantek_hmcad1511_set_channel_mappings(struct hantek_device *dev, size_t nr_chans)
{
    HRESULT ret = H_OK;

    uint16_t chan_map[HT_MAX_CHANNELS] = { 0x1, 0x2, 0x4, 0x8 };
    size_t chan_id = 0;

    HASSERT_ARG(NULL != dev);
    HASSERT_ARG(0 != nr_chans && 5 > nr_chans);

    for (size_t i = 0; i < HT_MAX_CHANNELS && chan_id < HT_MAX_CHANNELS; i++) {
        if (true == dev->channels[i].enabled) {
            switch (nr_chans) {
            case 1:
                /* One channel maps to all ADC channels (since the HMCAD1511 interleaves the ADCs) */
                chan_map[0] = chan_map[1] = chan_map[2] = chan_map[3] = 0x2 << i;
                chan_id = 4;
                break;
            case 2:
                /* A pair of channels (high or low) maps to an ADC channel */
                chan_map[chan_id] = chan_map[chan_id + 1] = 0x2 << i;
                chan_id += 2;
                break;
            case 3:
            case 4:
                chan_map[i] = 0x2 << i;
                break;
            }
        }
    }

    /* Write out the two input select registers */
    if (H_FAILED(ret = _hantek_hmcad1511_write_reg(dev, HMCAD1511_REG_INP_SEL_CH_LO, (chan_map[2] << 8) | chan_map[3]))) {
        DEBUG("Failed to write out channel map for HI registers");
        goto done;
    }

    if (H_FAILED(ret = _hantek_hmcad1511_write_reg(dev, HMCAD1511_REG_INP_SEL_CH_HI, (chan_map[0] << 8)| chan_map[1]))) {
        DEBUG("Failed to write out channel map for LO registers");
        goto done;
    }

done:
    return ret;
}

static
HRESULT _hantek_hmcad1511_set_coarse_gains(struct hantek_device *dev, size_t nr_chans)
{
    HRESULT ret = H_OK;

    uint8_t reg = 0;
    uint16_t gains = 0,
             ch_id = 0;

    HASSERT_ARG(NULL != dev);
    HASSERT_ARG(0 != nr_chans && 5 > nr_chans);

    switch (nr_chans) {
    case 1:
        reg = HMCAD1511_REG_CGAIN2_1;
        for (size_t i = 0; i < HT_MAX_CHANNELS; i++) {
            if (dev->channels[i].enabled == true) {
                assert(dev->channels[i].vpd < 12);
                gains = _hantek_vpd_gain_mapping[dev->channels[i].vpd] << 8;
                break;
            }
        }
        break;
    case 2:
        reg = HMCAD1511_REG_CGAIN2_1;
        for (size_t i = 0; i < HT_MAX_CHANNELS && ch_id < 2; i++) {
            if (dev->channels[i].enabled == true) {
                assert(dev->channels[i].vpd < 12);
                gains |= (_hantek_vpd_gain_mapping[dev->channels[i].vpd] & 0xf) << (4 * ch_id);
                ch_id++;
            }
        }
        assert(ch_id == 2);
        break;
    case 3:
    case 4:
        reg = HMCAD1511_REG_CGAIN4;
        for (size_t i = 0; i < HT_MAX_CHANNELS; i++) {
            if (dev->channels[i].enabled == true) {
                assert(dev->channels[i].vpd < 12);
                gains |= (_hantek_vpd_gain_mapping[dev->channels[i].vpd] & 0xf) << (4 * i);
            }
        }
        break;
    }

    if (H_FAILED(ret = _hantek_hmcad1511_write_reg(dev, reg, gains))) {
        DEBUG("Failed to set channel coarse gains, aborting.");
        goto done;
    }

done:
    return ret;
}

static
HRESULT _hantek_configure_adc_range_scaling(struct hantek_device *dev)
{
    HRESULT ret = H_OK;

    uint8_t message[8] = { HT_MSG_SEND_SPI, 0x00, 0x00, 0x00, 0x00, HMCAD1511_REG_FS_CNTRL, HT_SPI_CS_HMCAD1511, 0x00 };
    size_t transferred = 0,
           nr_chans = 0;

    HASSERT_ARG(NULL != dev);

    for (size_t i = 0; i < HT_MAX_CHANNELS; i++) {
        if (dev->channels[i].enabled == true) {
            nr_chans++;
        }
    }

    DEBUG("Setting range scaling for %zu channels", nr_chans);

    if (nr_chans == 0) {
        DEBUG("No channels enabled, aborting.");
        ret = H_ERR_INVAL_CHANNELS;
        goto done;
    }

    switch (nr_chans) {
    case 1:
        message[3] = dev->pcb_revision == 105 ? 0 : 25;
        break;
    case 2:
        message[3] = dev->pcb_revision == 105 ? 10 : 48;
        break;
    case 3:
    case 4:
        message[3] = dev->pcb_revision == 105 ? 55 : 63;
        break;
    default:
        DEBUG("Invalid channel count: %zu", nr_chans);
        ret = H_ERR_INVAL_CHANNELS;
        goto done;
    }

    if (H_FAILED(ret = _hantek_bulk_cmd_out(dev->hdl, message, sizeof(message), &transferred))) {
        DEBUG("Failed to set scaling control");
        ret = H_ERR_INVAL_CHANNELS;
        goto done;
    }

    usleep(3000);

done:
    return ret;
}

HRESULT hantek_configure_adc_routing(struct hantek_device *dev)
{
    HRESULT ret = H_OK;

    size_t nr_chans = 0;

    HASSERT_ARG(NULL != dev);

    for (size_t i = 0; i < HT_MAX_CHANNELS; i++) {
        if (dev->channels[i].enabled == true) {
            nr_chans++;
        }
    }

    if (H_FAILED(ret = _hantek_configure_adc_range_scaling(dev))) {
        DEBUG("Failed to set ADC front-end max channels, aborting.");
        goto done;
    }

    if (H_FAILED(ret = _hantek_hmcad1511_set_channel_mappings(dev, nr_chans))) {
        DEBUG("Failed to set channel mappings in ADC");
        goto done;
    }

    if (H_FAILED(ret = _hantek_hmcad1511_set_channel_count(dev, nr_chans))) {
        DEBUG("Failed to set active channel count in ADC, aborting.");
        goto done;
    }

    if (H_FAILED(ret = _hantek_hmcad1511_set_coarse_gains(dev, nr_chans))) {
        DEBUG("Failed to set coarse gains, aborting.");
        goto done;
    }

done:
    return ret;
}

static const
double _hantek_vpd_to_scale[12] = {
    [0] = 50.0,
    [1] = 20.0,
    [2] = 10.0,
    [3] = 5.0,
    [4] = 2.0,
    [5] = 1.0,
    [6] = 5.0,
    [7] = 2.0,
    [8] = 1.0,
    [9] = 5.0,
    [10] = 2.0,
    [11] = 1.0,
};

static
HRESULT _hantek_set_frontend_level(struct hantek_device *dev, unsigned channel_num, unsigned chan_level)
{
    HRESULT ret = H_OK;

    uint8_t message[4] = { 0x0 };
    uint16_t offset = 0;
    uint16_t hi = 0,
             lo = 0,
             v = 0,
             q = 0,
             upper = 0,
             lower = 0,
             mode_map = 0;
    double x = 0.0;
    const uint16_t *line = NULL;
    size_t transferred = 0;

    switch (channel_num) {
    case 0:
        message[0] = HT_MSG_POSITION_CH0;
        break;
    case 1:
        message[0] = HT_MSG_POSITION_CH1;
        break;
    case 2:
        message[0] = HT_MSG_POSITION_CH2;
        break;
    case 3:
        message[0] = HT_MSG_POSITION_CH3;
        break;
    default:
        DEBUG("Invalid channel ID %u, aborting.", channel_num);
        ret = H_ERR_INVAL_CHANNELS;
        goto done;
    }

    switch (dev->channels[channel_num].vpd) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        offset = 0x3c;
        break;
    case 6:
    case 7:
    case 8:
        offset = 0x60;
        break;
    case 9:
    case 10:
    case 11:
        offset = 0x84;
        break;
    default:
        DEBUG("Invalid Volts/Division: %u", dev->channels[channel_num].vpd);
        ret = H_ERR_INVAL_VOLTS_PER_DIV;
        goto done;
    }

    line = &dev->cal_data[channel_num * 144];

    hi = line[offset + mode_map];
    lo = line[offset + mode_map + 1];

    v = (unsigned)((((double)(hi + lo))/2.0) + 0.5);
    x = (double)(lo - v)/_hantek_vpd_to_scale[dev->channels[channel_num].vpd];
    q = (unsigned)(x + 0.5);
    upper = v + q;
    lower = v - q;

    offset = ((double)(upper - lower)/255.0) * chan_level + lower;

    message[2] = offset & 0xff;
    message[3] = (offset >> 8) & 0xff;

    if (H_FAILED(ret = _hantek_bulk_cmd_out(dev->hdl, message, sizeof(message), &transferred))) {
        DEBUG("Failed to send frontend configuration, aborting.");
        goto done;
    }

    if (transferred != sizeof(message)) {
        DEBUG("Failed to transfer message to set channel position, aborting.");
        ret = H_ERR_CONTROL_FAIL;
        goto done;
    }

    usleep(10000);

done:
    return ret;
}

HRESULT hantek_configure_channel_frontend(struct hantek_device *dev, unsigned channel_num, enum hantek_volts_per_div volts_per_div, enum hantek_coupling coupling, bool bw_limit, bool enable, unsigned chan_level)
{
    HRESULT ret = H_OK;

    struct hantek_channel *this_chan = NULL;

    HASSERT_ARG(NULL != dev);
    HASSERT_ARG(channel_num < HT_MAX_CHANNELS);

    this_chan = &dev->channels[channel_num];

    this_chan->vpd = volts_per_div;
    this_chan->coupling = coupling;
    this_chan->bw_limit = bw_limit;
    this_chan->enabled = enable;
    this_chan->level = chan_level;

    /* Commit the frontend configuration */
    if (H_FAILED(ret = _hantek_commit_frontend_config(dev))) {
        DEBUG("Failed to configure frontend, aborting.");
        goto done;
    }

    /* Set the level of this channel */
    if (H_FAILED(ret = _hantek_set_frontend_level(dev, channel_num, this_chan->level))) {
        DEBUG("Failed to set level of channel, aborting.");
        goto done;
    }

done:
    return ret;
}

static
HRESULT _hantek_set_trigger_mode(libusb_device_handle *hdl, enum hantek_trigger_mode mode, enum hantek_trigger_slope slope, enum hantek_coupling coupling)
{
    HRESULT ret = H_OK;

    uint8_t message[6] = { HT_MSG_CONFIGURE_TRIGGER, 0x0 };
    size_t transferred = 0;

    HASSERT_ARG(NULL != hdl);

    message[2] = (uint8_t)mode;
    message[3] = (uint8_t)slope;
    message[4] = (uint8_t)coupling;
    message[5] = 0x0;

    if (H_FAILED(ret = _hantek_bulk_cmd_out(hdl, message, sizeof(message), &transferred))) {
        DEBUG("Failed to send set trigger mode command, aborting.");
        goto done;
    }

done:
    return ret;
}

static
HRESULT _hantek_set_trigger_level(struct hantek_device *dev, uint8_t trig_vertical_level, uint8_t slop)
{
    HRESULT ret = H_OK;

    uint8_t message[26] = { HT_MSG_SET_TRIGGER_LEVEL, 0x0 };
    int32_t pos = 0,
            round = 0,
            high = 0,
            low = 0;
    size_t transferred = 0;

    HASSERT_ARG(NULL != dev);

    /* Convert to Q22.10 */
    pos = ((200 * trig_vertical_level * 1024)/256);

    /* Round it off */
    if ((pos & 0x3ff) > 0x1ff) {
        round = 1;
    }

    pos /= 1024;
    pos += round + 28;

    /* Calculate threshold */
    high = pos + slop;
    low = pos - slop;

    if (high < 0) high = 0;
    if (high > HT_TRIGGER_MAX_VALUE) high = HT_TRIGGER_MAX_VALUE;
    if (low < 0) low = 0;
    if (low > high || low > HT_TRIGGER_MAX_VALUE) low = 0x40; /* This is what Hantek does, probably is busted */

    /* Fill in the message */
    message[2] = high;
    message[3] = high;
    message[4] = low;
    message[5] = low;

    message[6] = high;
    message[7] = high;
    message[8] = low;
    message[9] = low;

    message[10] = high;
    message[11] = high;
    message[12] = low;
    message[13] = low;

    message[14] = high;
    message[15] = high;
    message[16] = low;
    message[17] = low;

    message[18] = pos;
    message[19] = pos;
    message[20] = pos;
    message[21] = pos;
    message[22] = pos;
    message[23] = pos;
    message[24] = pos;
    message[25] = pos;

    if (H_FAILED(ret = _hantek_bulk_cmd_out(dev->hdl, message, sizeof(message), &transferred))) {
        DEBUG("Failed to send set trigger level message, aborting.");
        goto done;
    }

done:
    return ret;
}

static
HRESULT _hantek_set_trigger_horizontal_offset(struct hantek_device *dev, uint32_t trig_horiz_offset, uint32_t slop)
{
    HRESULT ret = H_OK;

    uint8_t message[14] = { HT_MSG_SET_TRIG_HORIZ_POS, 0x0 };
    size_t transferred = 0;
    uint64_t leading = 0x831c4,
             trailing = 0x7d7d0;

    HASSERT_ARG(NULL != dev);

    DEBUG("Warning: this function has FIXMEs");

    /* FIXME: hardcoded for experimentation */

    message[2] = (leading >> 0) & 0xff;
    message[3] = (leading >> 8) & 0xff;
    message[4] = (leading >> 16) & 0xff;
    message[5] = (leading >> 24) & 0xff;
    message[6] = (leading >> 32) & 0xff;
    message[7] = (leading >> 40) & 0xff;

    message[8] = (trailing >> 0) & 0xff;
    message[9] = (trailing >> 8) & 0xff;
    message[10] = (trailing >> 16) & 0xff;
    message[11] = (trailing >> 24) & 0xff;
    message[12] = (trailing >> 32) & 0xff;
    message[13] = (trailing >> 40) & 0xff;

    if (H_FAILED(ret = _hantek_bulk_cmd_out(dev->hdl, message, sizeof(message), &transferred))) {
        DEBUG("Failed to set horizontal offset, aborting.");
        goto done;
    }

done:
    return ret;
}

static
HRESULT _hantek_set_trigger_source(struct hantek_device *dev, uint8_t channel, uint8_t peak)
{
    HRESULT ret = H_OK;

    uint8_t message[6] = { HT_MSG_SET_TRIGGER_SOURCE, 0x0 };
    bool is_ch_not_enabled = false;
    uint8_t mask = 0;
    size_t transferred = 0;

    HASSERT_ARG(NULL != dev);
    HASSERT_ARG(channel < 4);

    DEBUG("WARNING: setting trigger source has FIXMEs");

    is_ch_not_enabled = !dev->channels[channel].enabled;

    /* FIXME: this is only valid for 250MSPS and lower */
    switch (__hantek_device_nr_active_chans(dev)) {
    case 1:
        mask = 0x3;
        break;
    case 2:
        mask = 0x2;
        break;
    case 3:
    case 4:
        mask = 0x1;
        break;
    default:
        ret = H_ERR_BAD_ARGS;
        DEBUG("Weird number of channels active, aborting.");
        goto done;
    }

    message[2] = ((peak & 1) << 6) | (__hantek_device_chan_mask(dev) << 2) | (mask & 0x3);
    message[4] = 0; /* FIXME: only valid for sampling rates 250MSPS and below */
    message[5] = (is_ch_not_enabled << 2) | (channel & 0x3);

    if (H_FAILED(ret = _hantek_bulk_cmd_out(dev->hdl, message, sizeof(message), &transferred))) {
        DEBUG("Failed to send set trigger level message, aborting.");
        goto done;
    }

done:
    return ret;
}

HRESULT hantek_configure_trigger(struct hantek_device *dev, unsigned channel_num, enum hantek_trigger_mode mode, enum hantek_trigger_slope slope, enum hantek_coupling coupling, uint8_t trig_vertical_level, uint8_t trig_vertical_slop, uint32_t trig_horiz_offset)
{
    HRESULT ret = H_OK;

    HASSERT_ARG(NULL != dev);
    HASSERT_ARG(4 > channel_num);
    HASSERT_ARG(trig_horiz_offset <= 100);

    /* Set trigger horizontal offset, with hard-coded slop for now */
    if (H_FAILED(ret = _hantek_set_trigger_horizontal_offset(dev, trig_horiz_offset, 4))) {
        goto done;
    }

    if (H_FAILED(ret = _hantek_set_trigger_source(dev, channel_num, 0))) {
        goto done;
    }

    /* Set trigger voltage level */
    if (H_FAILED(ret = _hantek_set_trigger_level(dev, trig_vertical_level, trig_vertical_slop))) {
        goto done;
    }

    if (H_FAILED(ret = _hantek_set_trigger_mode(dev->hdl, mode, slope, coupling))) {
        goto done;
    }

done:
    return ret;
}

static
HRESULT _hantek_capture_read_status(struct hantek_device *dev, uint64_t *pstatus)
{
    HRESULT ret = H_OK;

    uint8_t message[2] = { HT_MSG_BUFFER_STATUS, 0x00 };
    size_t transferred = 0;
    uint8_t result[5] = { 0x0 };

    HASSERT_ARG(NULL != dev);

    if (H_FAILED(ret = _hantek_bulk_cmd_out(dev->hdl, message, sizeof(message), &transferred))) {
        DEBUG("Failed to send buffer status request command, aborting.");
        goto done;
    }

    if (transferred != sizeof(message)) {
        DEBUG("Failed to transfer message for some reason, aborting.");
        ret = H_ERR_NOT_READY;
        goto done;
    }

    if (H_FAILED(ret = _hantek_bulk_read_in(dev->hdl, result, sizeof(result)))) {
        DEBUG("Failed to read in status word, aborting.");
        goto done;
    }

    *pstatus = ((uint64_t)result[4] << 32) |
            ((uint64_t)result[3] << 24) |
            ((uint64_t)result[2] << 16) |
            ((uint64_t)result[1] << 8) |
            ((uint64_t)result[0]);

    DEBUG("Capture Status word: 0x%010lx", *pstatus);

done:
    return ret;
}

static
HRESULT __hantek_send_prepare_readback_req(struct hantek_device *dev)
{
    HRESULT ret = H_OK;

    uint8_t message[4] = { HT_MSG_BUFFER_PREPARE_TRANSFER, 0x00 };

    HASSERT_ARG(NULL != dev);



    return ret;
}

static
HRESULT __hantek_request_read_buffer(struct hantek_device *dev)
{
    HRESULT ret = H_OK;

    uint8_t message[4] = { HT_MSG_READBACK_BUFFER, 0x00 };
    size_t transferred = 0;
    size_t cap_buf_len = 0;

    HASSERT_ARG(NULL != dev);

    cap_buf_len = dev->capture_buffer_len >> 1;

    message[2] = cap_buf_len & 0xff;
    message[3] = (cap_buf_len >> 8) & 0xff;

    if (H_FAILED(ret = _hantek_bulk_cmd_out(dev->hdl, message, sizeof(message), &transferred))) {
        DEBUG("Failed to send set trigger level message, aborting.");
        goto done;
    }

done:
    return ret;
}

static
HRESULT _hantek_read_capture_buffer(struct hantek_device *dev, uint8_t *ch1, uint8_t *ch2, uint8_t *ch3, uint8_t *ch4)
{
    HRESULT ret = H_OK;

    HASSERT_ARG(NULL != dev);

    if (H_FAILED(ret = __hantek_send_readback_req(dev))) {
        DEBUG("Failed to send readback request, aborting.");
        goto done;
    }

    if (H_FAILED(ret = __hantek_request_read_buffer(dev))) {
        DEBUG("Failed to send request to initiate buffer read, aborting.");
        goto done;
    }

    /* TODO: read back the buffer, until there is an empty result */

done:
    return ret;
}

HRESULT hantek_retrieve_buffer(struct hantek_device *dev, uint8_t *ch1, uint8_t *ch2, uint8_t *ch3, uint8_t *ch4)
{
    HRESULT ret = H_OK;

    uint64_t cap_status = 0;

    HASSERT_ARG(NULL != dev);

    if (H_FAILED(ret = _hantek_capture_read_status(dev, &cap_status))) {
        DEBUG("Failed to get capture readback status, aborting.");
        goto done;
    }

done:
    return ret;
}

HRESULT hantek_start_capture(struct hantek_device *dev, enum hantek_capture_mode mode)
{
    HRESULT ret = H_OK;

    uint8_t message[4] = { HT_MSG_SEND_START_CAPTURE, 0x00, mode, 0x00 };
    size_t transferred = 0;

    HASSERT_ARG(NULL != dev);

    if (H_FAILED(ret = _hantek_bulk_cmd_out(dev->hdl, message, sizeof(message), &transferred))) {
        DEBUG("Failed to send start capture command, aborting.");
        goto done;
    }

    if (transferred != sizeof(message)) {
        DEBUG("Failed to transfer message for some reason, aborting.");
        goto done;
    }

done:
    return ret;
}

