#include <libusb.h>
#include <string.h>
#include <stdio.h>

#define MAX_PRODUCT_LEN  1024
#define DATA_BUFFER_LEN  27
#define TRANSFER_TIMEOUT 500

libusb_device *get_device_by_prod_name_prefix(libusb_device **devs, ssize_t cnt,
                                              const char *prefix) {
    ssize_t i;
    struct libusb_device_descriptor desc;
    libusb_device_handle *h;
    libusb_device *result = NULL;
    char prodName[MAX_PRODUCT_LEN];
    int r;
    size_t prefixLen = strlen(prefix);
    for (i = 0; i < cnt && !result; i++) {
        r = libusb_get_device_descriptor(devs[i], &desc);
        if (r < 0) {
            continue;
        }
        r = libusb_open(devs[i], &h);
        if (r < 0) {
            continue;
        }
        r = libusb_get_string_descriptor_ascii(h, desc.iProduct,
                                               prodName, MAX_PRODUCT_LEN);
        if (r >= 0 && strncmp(prefix, prodName, prefixLen) == 0) {
            result = devs[i];
        }
        libusb_close(h);
    }
    return result;
}

int main(int argc, char **argv) {
    int r;

    r = libusb_init(NULL);
    if (r < 0) {
        return r;
    }

    libusb_device **devs;
    ssize_t cnt = libusb_get_device_list(NULL, &devs);
    if (cnt < 0) {
        fprintf(stderr, "List is empty\n");
        return (int)cnt;
    }

    libusb_device *dev =
        get_device_by_prod_name_prefix(devs, cnt, "Harmonix RB3 Keyboard");

    if (dev == NULL) {
        fprintf(stderr, "Failed to find device\n");
        goto finish;
    }

    fprintf(stderr, "Brilliant news! Found device!\n");

    struct libusb_config_descriptor *cfgDesc;
    r = libusb_get_active_config_descriptor(dev, &cfgDesc);
    if (r < 0) {
        fprintf(stderr, "Failed to get active config\n");
        goto finish;
    }

    if (cfgDesc->bNumInterfaces < 1 || cfgDesc->interface[0].num_altsetting < 1 || cfgDesc->interface[0].altsetting[0].bNumEndpoints < 1) {
        fprintf(stderr, "No endpoints found\n");
        goto finish2;
    }

    uint8_t numEndpoints = cfgDesc->interface[0].altsetting[0].bNumEndpoints;
    const struct libusb_endpoint_descriptor *endpoints = &cfgDesc->interface[0].altsetting[0].endpoint[0];
    const struct libusb_endpoint_descriptor *endpoint = NULL;

    int i;
    for (i = 0; i < numEndpoints; i++) {
        if ((endpoints[i].bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN &&
            (endpoints[i].bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_INTERRUPT) {
            endpoint = &endpoints[i];
        }
    }

    if (endpoint == NULL) {
        fprintf(stderr, "No suitable endpoint\n");
        goto finish2;
    }

    fprintf(stderr, "Got endpoint!\n");

    libusb_device_handle *h = NULL;
    r = libusb_open(dev, &h);
    if (r < 0) {
        fprintf(stderr, "Failed to open device\n");
        goto finish2;
    }

    r = libusb_claim_interface(h, 0);
    if (r < 0) {
        fprintf(stderr, "Failed to claim interface\n");
        goto finish3;
    }

    uint8_t buffer1[DATA_BUFFER_LEN];
    uint8_t buffer2[DATA_BUFFER_LEN];
    memset(buffer1, 0, sizeof(buffer1));
    memset(buffer2, 0, sizeof(buffer2));
    uint8_t *curBuffer = buffer1;
    uint8_t *otherBuffer = buffer2;
    int transferred_len = 0, pending_len = 0;
    
    //struct libusb_transfer transfer;
    //libusb_fill_interrupt_transfer(&transfer, h, endpoint->bEndpointAddress, buffer, DATA_BUFFER_LEN, got_data, NULL, TRANSFER_TIMEOUT);

    while (1) {

        r = libusb_interrupt_transfer(h, endpoint->bEndpointAddress, curBuffer + pending_len, DATA_BUFFER_LEN - pending_len, &transferred_len, TRANSFER_TIMEOUT);

        if (r < 0 || transferred_len == 0) {
            fprintf(stderr, "Transfer failed\n");
            break;
        }

        if (pending_len + transferred_len < DATA_BUFFER_LEN) {
            pending_len += transferred_len;
            transferred_len = 0;
            continue;
        }

        transferred_len += pending_len;
        pending_len = 0;

        if (memcmp(curBuffer, otherBuffer, DATA_BUFFER_LEN) != 0) {

            for (i = 0; i < transferred_len; i++) {
                fprintf(stderr, " %02x", curBuffer[i]);
            }
            fprintf(stderr, "\n");

            // Swap buffers
            uint8_t *tmp = curBuffer;
            curBuffer = otherBuffer;
            otherBuffer = tmp;

        }

        transferred_len = 0;

    }

finish4:
    libusb_release_interface(h, 0);
finish3:
    libusb_close(h);
finish2:
    libusb_free_config_descriptor(cfgDesc);
finish:
    libusb_free_device_list(devs, 1);
    libusb_exit(NULL);

    return 0;
}
