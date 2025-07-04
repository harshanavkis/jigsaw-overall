#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define SHMEM_SIZE (1 << 20)  // 1 MB, adjust as needed
#define BUFS_SIZE 92 // Size of the rdma buffers used in recv/send
#define NUM_RECV_BUFS 8 // Has to be an even number

//#define DEBUG_MESSAGES

#endif // COMMON_H
