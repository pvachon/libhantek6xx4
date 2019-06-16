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

    if (H_FAILED(hantek_close_device(&dev))) {
        printf("Failed to close device, aborting.\n");
        goto done;
    }

done:
	return EXIT_SUCCESS;
}
