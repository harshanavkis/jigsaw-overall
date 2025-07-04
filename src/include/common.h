#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define SHMEM_SIZE (1 << 20)  // 1 MB, adjust as needed
#define DMA_REGION_OFFSET (1 << 12) // 4K aligned; offset of DMA region in shmem
#define DMA_SIZE (SHMEM_SIZE - DMA_REGION_OFFSET)
#define BUFS_SIZE 92 // Size of the rdma buffers used in recv/send
#define NUM_RECV_BUFS 8 // Has to be an even number

//#define DEBUG_MESSAGES

#endif // COMMON_H
