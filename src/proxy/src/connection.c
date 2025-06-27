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

//#define CONFIG_DISAGG_DEBUG_MMIO

#define SHMEM_FILE "/dev/shm/ivshmem"  // Adjust this path as needed
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

    void *enc_buf = disagg_mmio_encrypt(buf, count);
    if (!enc_buf) {
	return 0;
    }

    wait_for_read_doorbell_clear();

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

void *run_shmem_app(disagg_pci_dev_info *pci_info, void *opaque) {
    if (init_shared_memory() < 0) {
        printf("SHMEM: init_shared_memory failed\n");
        // return;
    }
    
    if (disagg_init_crypto()) {
	printf("SHMEM: disagg_init_crypto failed\n");
    }

    printf("connection.c: In shmem app\n");

    printf("ages...\n");

    void *data = NULL;
    bool is_write = false;
    region_access_cb_t *cb;
    loff_t offset;


    munmap(shmem, SHMEM_SIZE);
}


/***********************/

/***************/
