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

//#define CONFIG_DISAGG_DEBUG_MMIO

#define SHMEM_FILE "/dev/shm/ivshmem"  // Adjust this path as needed
#define SHMEM_SIZE (1 << 20)  // 1 MB, adjust as needed
#define READ_DOORBELL_OFFSET 0
#define WRITE_DOORBELL_OFFSET 1
#define DOORBELL_SIZE 1  // 1 byte for each doorbell
#define TOTAL_DOORBELL_SIZE (DOORBELL_SIZE * 2)
#define DMA_REGION_OFFSET (1 << 12) // 4K aligned
#define DMA_SIZE (SHMEM_SIZE - DMA_REGION_OFFSET)

/* Offsets in the shared memory with special values */
#define OFFSET_PROXY_DMA (256)
#define OFFSET_BAR_PHYS_ADDR (264)

static void *shmem = NULL;
static volatile uint8_t *read_doorbell = NULL;
static volatile uint8_t *write_doorbell = NULL;

static int create_or_open_shmem_file() {
    int fd = open(SHMEM_FILE, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        perror("Failed to open or create shared memory file");
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("Failed to get file status");
        close(fd);
        return -1;
    }

    if (st.st_size != SHMEM_SIZE) {
        if (ftruncate(fd, SHMEM_SIZE) < 0) {
            perror("Failed to set file size");
            close(fd);
            return -1;
        }
        printf("Created shared memory file with size %d bytes\n", SHMEM_SIZE);
    } else {
        printf("Opened existing shared memory file with correct size\n");
    }

    return fd;
}

static int init_shared_memory() {
    int fd = create_or_open_shmem_file();
    if (fd < 0) {
        return -1;
    }

    shmem = mmap(NULL, SHMEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shmem == MAP_FAILED) {
        perror("Failed to mmap shared memory");
        close(fd);
        return -1;
    }

    read_doorbell = (volatile uint8_t *)shmem + READ_DOORBELL_OFFSET;
    write_doorbell = (volatile uint8_t *)shmem + WRITE_DOORBELL_OFFSET;
    close(fd);
    
    // Initialize doorbells to 0
    *read_doorbell = 0;
    *write_doorbell = 0;

    // alloc region for decryption of dma requests
    void *dma_dec = malloc(DMA_SIZE); 
    if (dma_dec == NULL) {
	printf("malloc for big DMA region failed\n");
	return -1;
    }
    disagg_crypto_dma_global.proxyDMA_start = dma_dec;

    // write proxyDMA address into shmem
    *((uint64_t *)(shmem + OFFSET_PROXY_DMA)) = (uint64_t) dma_dec; 


    msync(shmem, TOTAL_DOORBELL_SIZE, MS_SYNC);
    
    return 0;
}

static void wait_for_write_doorbell_set() {
    // uint32_t counter = 0;
    while (__atomic_load_n(write_doorbell, __ATOMIC_ACQUIRE) == 0) {
        // if (++counter % 1000000 == 0) {
        //     printf("Still waiting for write doorbell to be set. Current value: %u\n", *write_doorbell);
        // }
    }
}

static void wait_for_read_doorbell_clear() {
    while (__atomic_load_n(read_doorbell, __ATOMIC_ACQUIRE) != 0) {
        // Busy-wait
    }
}

static ssize_t ivshmem_read(void *buf, size_t count, off_t offset) {
    if (!shmem || offset >= SHMEM_SIZE - TOTAL_DOORBELL_SIZE)
        return -1;

    if (offset + count > SHMEM_SIZE - TOTAL_DOORBELL_SIZE)
        count = SHMEM_SIZE - TOTAL_DOORBELL_SIZE - offset;

    memcpy(disagg_crypto_mmio_global.buf, shmem + TOTAL_DOORBELL_SIZE + offset, disagg_crypto_mmio_global.adlen + count + disagg_crypto_mmio_global.authsize);

    return disagg_mmio_decrypt(buf, count);
}

static ssize_t ivshmem_write(void *buf, size_t count, off_t offset) {
    if (!shmem || offset >= SHMEM_SIZE - TOTAL_DOORBELL_SIZE)
        return -1;

    if (offset + count > SHMEM_SIZE - TOTAL_DOORBELL_SIZE)
        count = SHMEM_SIZE - TOTAL_DOORBELL_SIZE - offset;

    wait_for_read_doorbell_clear();

    void *enc_buf = disagg_mmio_encrypt(buf, count);
    if (!enc_buf) {
	return 0;
    }

    memcpy(shmem + TOTAL_DOORBELL_SIZE + offset, enc_buf, disagg_crypto_mmio_global.adlen + count + disagg_crypto_mmio_global.authsize);

    __atomic_store_n(read_doorbell, 1, __ATOMIC_RELEASE);

    return count;
}

static int wait_and_read_data(void *buf, size_t count) {
    wait_for_write_doorbell_set();

    ssize_t read_bytes = ivshmem_read(buf, count, 0);
    if (read_bytes < 0) {
        return -1;
    }

    __atomic_store_n(write_doorbell, 0, __ATOMIC_RELEASE);

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
    if (init_shared_memory() < 0) {
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
            // Simulate reading data from the specified address
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

    munmap(shmem, SHMEM_SIZE);
    // return;
}


/***********************/

/***************/
