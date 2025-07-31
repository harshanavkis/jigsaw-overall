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
#include "rdma_server.h"

//#define CONFIG_DISAGG_DEBUG_MMIO

static ssize_t mmio_recv(struct guest_message_header *hdr) {
	void *res;

	res = rdma_recv();

	if (!res) {
		printf("error rdma_recv\n");
		return -1;
	}

	if (*((uint8_t *)res) == OP_READ) {
		memcpy(hdr, res + 1, sizeof(struct guest_message_header) - sizeof(uint64_t));
		return sizeof(struct guest_message_header) - sizeof(uint64_t);
	} else {
		memcpy(hdr, res + 1, sizeof(struct guest_message_header));
		return sizeof(struct guest_message_header);
	}

}

static ssize_t mmio_send(void *buf, size_t count) {
	memcpy(regions_rdma.enc_send_buf + 1, buf, count);

	// Set first byte to OP_READ
	*((uint8_t *)regions_rdma.enc_send_buf) = OP_READ;

	int ret = rdma_send(regions_rdma.enc_send_buf, 1 + count);

	if (ret != 0)
		return -1;

	return count;
}

void *run_mmio_app(disagg_pci_dev_info *pci_info, void *opaque) {
	printf("MMIO communication application started. Waiting for messages...\n");

	bool is_write = false;
	region_access_cb_t *cb;
	loff_t offset;
	int pci_region;
	uint32_t ret;
	char data[8];

	while (1) {
		struct guest_message_header header;
		if (mmio_recv(&header) == 0) {
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

				if (ret != header.length) {
					printf("connection.c: OP_READ: Reading %lu bytes failed\n", header.length);
					memset(data, 'A', header.length);
				}

#ifdef CONFIG_DISAGG_DEBUG_MMIO
				printf("connection.c: OP_READ: Read data: ");
				for (uint32_t i = 0; i < header.length; i++) {
					printf("%02X", ((uint8_t *)data)[i]);
				}
				printf("\n");
#endif

				if (mmio_send(data, sizeof(data)) < 0) {
					perror("Failed to write response");
					continue;
				}
				continue;

			case OP_WRITE:
#ifdef CONFIG_DISAGG_DEBUG_MMIO
				printf("connection.c: OP_WRITE: Received write operation: Address 0x%lx, Length %u\n", header.address, header.length);
#endif
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
				ret = cb(opaque, (void *)(&header.value), header.length, offset, is_write);

				if (ret != header.length) {
					printf("connection.c: OP_WRITE: Writing %lu bytes failed\n", header.length);
				}

				continue;

			default:
				fprintf(stderr, "Unknown operation: %d\n", header.operation);
				continue;
		}
	}
}

