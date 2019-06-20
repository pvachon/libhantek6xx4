#include <hantek.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char *argv[])
{
    printf("Hantek Device Test\n");

    struct hantek_device *dev = NULL;

    if (H_FAILED(hantek_open_device(&dev))) {
        printf("Failed to open device. Aborting.\n");
        goto done;
    }

    if (H_FAILED(hantek_set_sampling_rate(dev, HT_ST_500NS))) {
        printf("Failed to set sampling rate, aborting.\n");
    }

    for (size_t i = 0; i < 4; i++) {
        if (H_FAILED(hantek_configure_channel_frontend(dev, i, HT_VPD_1V, HT_COUPLING_AC, false))) {
            printf("Failed to set up channel %zu\n", i);
            goto done;
        }
    }

done:
    if (NULL != dev && H_FAILED(hantek_close_device(&dev))) {
        printf("Failed to close device, aborting.\n");
        goto done;
    }

	return EXIT_SUCCESS;
}
