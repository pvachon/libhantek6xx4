#include <hantek.h>
#include <hantek_flash.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

static
bool dump_bitstream_flash = false;

static
const char *bitstream_flash_filename = NULL;

static
void _dump_bitstream_flash(struct hantek_device *dev, const char *filename)
{
    FILE *fp = NULL;
    uint8_t *buffer = NULL;

    if (NULL == (buffer = calloc(HT_BITSTREAM_FLASH_SIZE, 1))) {
        fprintf(stderr, "Failed to allocate %x bytes for flash buffer\n", (unsigned)HT_BITSTREAM_FLASH_SIZE);
        exit(EXIT_FAILURE);
    }

    if (H_FAILED(hantek_read_bitstream_flash(dev, buffer, HT_BITSTREAM_FLASH_SIZE))) {
        fprintf(stderr, "Failed to read flash data in, aborting.\n");
        exit(EXIT_FAILURE);
    }

    if (NULL == (fp = fopen(filename, "w+"))) {
        fprintf(stderr, "Failed to open file '%s' for writing. Reason: %s (%d)\n",
                filename, strerror(errno), errno);
        exit(EXIT_FAILURE);
    }

    if (1 != fwrite(buffer, HT_BITSTREAM_FLASH_SIZE, 1, fp)) {
        fprintf(stderr, "Failed to write to flash data file, aborting. Reason: %s (%d)\n",
                strerror(errno), errno);
        exit(EXIT_FAILURE);
    }

    fclose(fp);
}

static
void _parse_args(int argc, char *const *argv)
{
    int c = -1;

    while (0 < (c = getopt(argc, argv, "hB:"))) {
        switch (c) {
        case 'h':
            fprintf(stderr, "No help here, yet\n");
            exit(EXIT_FAILURE);
            break;
        case 'B':
            printf("Dumping flash to file '%s'\n", optarg);
            bitstream_flash_filename = optarg;
            dump_bitstream_flash = true;
            break;
        default:
            fprintf(stderr, "Unknown argument: -%c\n", c);
        }
    }
}

int main(int argc, char * const *argv)
{
    printf("Hantek Device Test Tool\n");

    struct hantek_device *dev = NULL;

    _parse_args(argc, argv);

    if (H_FAILED(hantek_open_device(&dev, 4096))) {
        printf("Failed to open device. Aborting.\n");
        goto done;
    }

    if (H_FAILED(hantek_set_sampling_rate(dev, HT_ST_500NS))) {
        printf("Failed to set sampling rate, aborting.\n");
        goto done;
    }

    if (H_FAILED(hantek_configure_adc_range_scaling(dev))) {
        printf("Failed to set ADC front-end max channels, aborting.\n");
        goto done;
    }

    for (size_t i = 0; i < 4; i++) {
        if (H_FAILED(hantek_configure_channel_frontend(dev, i, HT_VPD_1V, HT_COUPLING_AC, false))) {
            printf("Failed to set up channel %zu\n", i);
            goto done;
        }
    }

    if (H_FAILED(hantek_get_status(dev, NULL))) {
        printf("Failed to get status, aborting.\n");
        goto done;
    }

    if (true == dump_bitstream_flash) {
        _dump_bitstream_flash(dev, bitstream_flash_filename);
    }

done:
    if (NULL != dev && H_FAILED(hantek_close_device(&dev))) {
        printf("Failed to close device, aborting.\n");
        goto done;
    }

	return EXIT_SUCCESS;
}
