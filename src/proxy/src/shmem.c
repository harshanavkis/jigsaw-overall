#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <time.h>

#include "shmem.h"

#include "../../include/common.h"

//#define CONFIG_DISAGG_DEBUG_MMIO

static void *shmem = NULL;
static volatile uint8_t *read_doorbell = NULL;
static volatile uint8_t *write_doorbell = NULL;

static int create_or_open_shmem_file() {
    int fd = open(SHMEM_FILE, O_RDWR | O_CREAT, 0644);
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

int init_shared_memory(char **ret_shmem) {
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

    printf("Virtual address of shmem: %p\n", shmem);

    read_doorbell = (volatile uint8_t *)shmem + READ_DOORBELL_OFFSET;
    write_doorbell = (volatile uint8_t *)shmem + WRITE_DOORBELL_OFFSET;
    close(fd);
    
    // Initialize doorbells to 0
    *read_doorbell = 0;
    *write_doorbell = 0;


    // Write virtual address of shmem DMA region to shmem
    // Driver needs this to instruct device
    *((uint64_t *)(shmem + OFFSET_PROXY_DMA)) = (uint64_t) shmem + DMA_REGION_OFFSET;

    msync(shmem, TOTAL_DOORBELL_SIZE, MS_SYNC);

    *ret_shmem = shmem;
    
    return 0;
}

/*
 * @return 1 if is set, 0 if not
 */
int get_write_doorbell(void) {
    return __atomic_load_n(write_doorbell, __ATOMIC_ACQUIRE);
}

void reset_write_doorbell(void) {
    __atomic_store_n(write_doorbell, 0, __ATOMIC_RELEASE);
}

static void wait_for_read_doorbell_clear() {
    while (__atomic_load_n(read_doorbell, __ATOMIC_ACQUIRE) != 0) {
        // Busy-wait
    }
}

ssize_t ivshmem_write(void *buf, size_t count, off_t offset) {
#ifdef DEBUG_MESSAGES 
    printf("ivshmem_write\ncount: %lu\nbuffer: ", count);
    for (size_t i = 0; i < count; ++i)
	printf("%x", *((unsigned char *)buf + i));
    printf("\n");
#endif

    wait_for_read_doorbell_clear();

    memcpy(shmem + MMIO_REGION_OFFSET + offset, buf, count);

    __atomic_store_n(read_doorbell, 1, __ATOMIC_RELEASE);

    return count;
}

/***************/

