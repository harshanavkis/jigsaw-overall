#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <time.h>

#include "mmio.h"
#include "sec_disagg.h"
#include "tcp_server.h"

//#define CONFIG_DISAGG_DEBUG_MMIO

static ssize_t mmio_recv(void *buf, size_t count) {
    void *res;

    res = tcp_recv_mmio_request();
    if (!res) {
	printf("error receiving mmio\n");
	return -1;
    }

    return disagg_mmio_decrypt(res, buf, count);
}

static ssize_t mmio_send(void *buf, size_t count) {
    void *enc_send_buf = disagg_mmio_encrypt(buf, regions_tcp->send_buf + 1, count);
    if (!enc_send_buf) {
	return -1;
    }

    int ret = tcp_send_mmio_reply(regions_tcp->send_buf, count + disagg_crypto_mmio_global.authsize);
    if (ret != 0)
	return -1;

    return count;
}

static int wait_and_read_data(void *buf, size_t count) {
    ssize_t read_bytes = mmio_recv(buf, count);
    if (read_bytes < 0) {
        return -1;
    }

    return read_bytes;
}

void *run_mmio_app(disagg_pci_dev_info *pci_info, void *opaque) {
    if (disagg_init_crypto()) {
	printf("disagg_init_crypto failed\n");
    }

    printf("MMIO communication application started. Waiting for messages...\n");

    void *data = NULL;
    bool is_write = false;
    region_access_cb_t *cb;
    loff_t offset;
    int pci_region;
    uint32_t ret;

    while (1) {
        struct guest_message_header header;
        if (wait_and_read_data(&header, sizeof(struct guest_message_header)) < 0) {
            perror("Failed to read message");
            continue;
        }

#ifdef CONFIG_DISAGG_DEBUG_MMIO
        printf("Received message: operation=%u, address=0x%lx, length=%u\n",
               header.operation, header.address, header.length);
#endif

        switch (header.operation)
        {
        case OP_READ:
#ifdef CONFIG_DISAGG_DEBUG_MMIO
            printf("connection.c: OP_READ: Received read operation: Address 0x%lx, Length %u\n", header.address, header.length);
#endif
            data = realloc(data, header.length);
            if (data == NULL)
            {
                fprintf(stderr, "Memory reallocation failed\n");
                continue;
            }

            pci_region = 0;

#ifdef CONFIG_DISAGG_DEBUG_MMIO
            printf("connection.c: OP_READ: Got PCI region: %d ", pci_region);
#endif
            cb = pci_info->regions[pci_region].cb;

	    offset = header.address;
#ifdef CONFIG_DISAGG_DEBUG_MMIO
            printf("with offset %ld\n", offset);
#endif
            is_write = false;
            ret = cb(opaque, data, header.length, offset, is_write);

            if (ret != header.length)
            {
                printf("connection.c: OP_READ: Reading %u bytes failed\n", header.length);
                memset(data, 'A', header.length);
            }

#ifdef CONFIG_DISAGG_DEBUG_MMIO
            printf("connection.c: OP_READ: Read data: ");
            for (uint32_t i = 0; i < header.length; i++)
            {
                printf("%02X", ((uint8_t *)data)[i]);
            }
            printf("\n");
#endif

            if (mmio_send(data, header.length) < 0) {
                perror("Failed to write response");
                continue;
            }
            continue;

        case OP_WRITE:
#ifdef CONFIG_DISAGG_DEBUG_MMIO
            printf("connection.c: OP_WRITE: Received write operation: Address 0x%lx, Length %u\n", header.address, header.length);
#endif
            data = realloc(data, header.length);
            if (data == NULL)
            {
                fprintf(stderr, "Memory reallocation failed\n");
                continue;
            }

            if (wait_and_read_data(data, header.length) < 0) {
                perror("Failed to read data");
                continue;
            }

            pci_region = 0;

            cb = pci_info->regions[pci_region].cb;

#ifdef CONFIG_DISAGG_DEBUG_MMIO
            printf("connection.c: OP_WRITE: Write data: ");
            for (uint32_t i = 0; i < header.length; i++)
            {
                printf("%02X", ((uint8_t *)data)[i]);
            }
            printf("\n");
#endif

	    offset = header.address;
            is_write = true;
            ret = cb(opaque, data, header.length, offset, is_write);

            if (ret != header.length) {
                printf("connection.c: OP_WRITE: Writing %u bytes failed\n", header.length);
            }

            continue;

        default:
            fprintf(stderr, "Unknown operation: %d\n", header.operation);
            continue;
        }
    }
}

