#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

/**
 * @brief Operation code for read requests
 */
#define OP_READ 0

/**
 * @brief Operation code for write requests
 */
#define OP_WRITE 1

#define SHMEM_SIZE (1 << 20)  // 1 MB, adjust as needed
#define DMA_REGION_OFFSET (1 << 12) // 4K aligned; offset of DMA region in shmem
#define MMIO_REGION_OFFSET (8)
#define DMA_SIZE (SHMEM_SIZE - DMA_REGION_OFFSET)
#define BUFS_SIZE 92 // Size of the rdma buffers used in recv/send
#define NUM_RECV_BUFS 8 // Has to be an even number

//#define DEBUG_MESSAGES

#endif // COMMON_H
