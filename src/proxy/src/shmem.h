#ifndef SHMEM_H
#define SHMEM_H

#include <stdint.h>
#include <stdbool.h>

#define SHMEM_FILE "/dev/shm/ivshmem"  // Adjust this path as needed
#define READ_DOORBELL_OFFSET 0
#define WRITE_DOORBELL_OFFSET 1
#define DOORBELL_SIZE 1  // 1 byte for each doorbell
#define TOTAL_DOORBELL_SIZE (DOORBELL_SIZE * 2)
#define MMIO_REGION_OFFSET (8)

/* Offsets in the shared memory with special values */
#define OFFSET_PROXY_DMA (256)


/*
 * Maps shared memory
 * If successfull, writes the mapped shmem pointer into @shmem
 * @return 0 for success
 */
int init_shared_memory(char **shmem);

ssize_t ivshmem_write(void *buf, size_t count, off_t offset);

int get_write_doorbell(void);

void reset_write_doorbell(void);

#endif // SHMEM_H
