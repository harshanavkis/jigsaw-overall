#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <time.h>

#include "connection.h"
#include "sec_disagg.h"
#include "rdma_server.h"

//#define CONFIG_DISAGG_DEBUG_MMIO

static void *shmem = NULL;

uint64_t proxyDMA_offset(dma_addr_t proxyDMA) {
    return ((uint64_t)(proxyDMA - (dma_addr_t) disagg_crypto_dma_global.proxyDMA_start)) + DMA_REGION_OFFSET; 
}

static int init_dma_memory() 
{
    shmem = regions_rdma.shmem_buf;

    // alloc region for decryption of dma requests
    void *dma_dec = malloc(DMA_SIZE); 
    if (dma_dec == NULL) {
	printf("malloc for big DMA region failed\n");
	return -1;
    }
    disagg_crypto_dma_global.proxyDMA_start = dma_dec;


    // write proxyDMA address into shmem
    *((uint64_t *)(shmem + OFFSET_PROXY_DMA)) = (uint64_t) dma_dec; 
    rdma_write(OFFSET_PROXY_DMA, sizeof(uint64_t));

    return 0;
}

static ssize_t ivshmem_read(void *buf, size_t count, off_t offset) {
    (void) offset;
    void *res;

    res = rdma_recv();
    if (!res) {
	printf("error rdma_recv\n");
	return -1;
    }

    return disagg_mmio_decrypt(res, buf, count);
}

static ssize_t ivshmem_write(void *buf, size_t count, off_t offset) {
    (void) offset;
    void *enc_send_buf = disagg_mmio_encrypt(buf, regions_rdma.enc_send_buf, count);
    if (!enc_send_buf) {
	return -1;
    }

    int ret = rdma_send(enc_send_buf, count);
    if (ret != 0)
	return -1;

    return count;
}

static int wait_and_read_data(void *buf, size_t count) {
    ssize_t read_bytes = ivshmem_read(buf, count, 0);
    if (read_bytes < 0) {
        return -1;
    }

    return read_bytes;
}

void *proxyDMA_to_proxyShmem(void *proxyDMA) {
    if (disagg_crypto_dma_global.proxyDMA_start > shmem)
	return proxyDMA - (disagg_crypto_dma_global.proxyDMA_start - DMA_REGION_OFFSET - shmem);
    else
	return proxyDMA + (shmem - disagg_crypto_dma_global.proxyDMA_start + DMA_REGION_OFFSET);
}

void read_vmPhys_mapping(disagg_pci_dev_info *pci_info, int bar_nr) {
    // Support only for 
    if (bar_nr != 0) {
	pci_info->regions[bar_nr].vmPhys = 0;
	pci_info->regions[bar_nr].vmPhys_valid = true;
	return;
    }

    rdma_read(OFFSET_BAR_PHYS_ADDR, sizeof(uint64_t *));
    pci_info->regions[bar_nr].vmPhys = *((uint64_t *)(shmem + OFFSET_BAR_PHYS_ADDR));
    pci_info->regions[bar_nr].vmPhys_valid = true;

    printf("vmPhys for bar %d: 0x%lx\n", bar_nr, pci_info->regions[bar_nr].vmPhys);
}

int get_pci_region(disagg_pci_dev_info *pci_info, uint64_t addr, uint32_t size)
{
    for (int i = 0; i < PCI_NUM_REGIONS; i++)
    {
	if (!pci_info->regions[i].vmPhys_valid)
	    read_vmPhys_mapping(pci_info, i);

        if (pci_info->regions[i].vmPhys == 0x0)
            continue;
        
	if ((addr >= (pci_info->regions[i].vmPhys)) 
	     && (addr + size <= (pci_info->regions[i].vmPhys) + *(pci_info->regions[i].size))) {
	 return i;
	}
    }

    return -1;
}

void *run_shmem_app(disagg_pci_dev_info *pci_info, void *opaque) {
    if (init_dma_memory() < 0) {
        printf("SHMEM: init_shared_memory failed\n");
        // return;
    }
    
    if (disagg_init_crypto()) {
	printf("SHMEM: disagg_init_crypto failed\n");
    }

    printf("connection.c: In shmem app\n");

    printf("SHMEM application started. Waiting for messages...\n");

    void *data = NULL;
    bool is_write = false;
    region_access_cb_t *cb;
    loff_t offset;

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

            int pci_region = get_pci_region(pci_info, header.address, header.length);

#ifdef CONFIG_DISAGG_DEBUG_MMIO
            printf("connection.c: OP_READ: Got PCI region: %d ", pci_region);
#endif
            cb = pci_info->regions[pci_region].cb;

	    offset = header.address - (pci_info->regions[pci_region].vmPhys);
#ifdef CONFIG_DISAGG_DEBUG_MMIO
            printf("with offset %ld\n", offset);
#endif
            is_write = false;
            uint32_t ret = cb(opaque, data, header.length, offset, is_write);

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

            if (ivshmem_write(data, header.length, 0) < 0) {
                perror("Failed to write response");
                continue;
            }
            continue;
            // break;

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

            pci_region = get_pci_region(pci_info, header.address, header.length);

            cb = pci_info->regions[pci_region].cb;

#ifdef CONFIG_DISAGG_DEBUG_MMIO
            printf("connection.c: OP_WRITE: Write data: ");
            for (uint32_t i = 0; i < header.length; i++)
            {
                printf("%02X", ((uint8_t *)data)[i]);
            }
            printf("\n");
#endif

	    offset = header.address - (pci_info->regions[pci_region].vmPhys);
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

        printf("Response sent. Waiting for next message...\n");
    }

    // return;
}


/***********************/

/***************/
