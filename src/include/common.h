#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define SHMEM_SIZE (1 << 20)  // 1 MB, adjust as needed
#define DMA_REGION_OFFSET (1 << 12) // 4K aligned; offset of DMA region in shmem
#define DMA_SIZE (SHMEM_SIZE - DMA_REGION_OFFSET)
#define MMIO_MESSAGE_SIZE 92 // Size of the MMIO message buffer send/recv over socket (uneven because first byte is type)
#define NUM_RECV_BUFS 8 // Has to be an even number

// Types of messages used in socket communication
#define TYPE_REQUEST 0
#define TYPE_REPLY 1
#define TYPE_REQUEST_DMA_TO_DEVICE 2
#define TYPE_REPLY_DMA_TO_DEVICE 3
#define TYPE_DMA_FROM_DEVICE 4

//#define DEBUG_MESSAGES

#endif // COMMON_H
