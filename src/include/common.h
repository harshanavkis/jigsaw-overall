#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <linux/if_ether.h>

#define SHMEM_SIZE (1 << 20)  // 1 MB, adjust as needed
#define DMA_REGION_OFFSET (1 << 12) // 4K aligned; offset of DMA region in shmem
#define DMA_SIZE (SHMEM_SIZE - DMA_REGION_OFFSET)
#define NUM_RECV_BUFS (8) // Has to be an even number

// Types of messages used in socket communication
#define OP_MMIO_READ 0
#define OP_MMIO_WRITE 1
#define OP_DMA_TO_DEVICE 2
#define OP_DMA_FROM_DEVICE 3

//#define DEBUG_MESSAGES

struct mmio_message {
    /*
     * Operation type (OP_READ or OP_WRITE)
     */
    uint8_t operation;

    /*
     * Memory address for the operation
     */
    uint64_t address;

    /*
     * Length of data to read or write
     */
    uint64_t length;

    /*
     * Value in case of OP_WRITE
     */
    uint64_t value;
} __attribute__((packed));


/************** Ethernet stuff ***************/

#define ETH_PROT_NR 0x9876

#define MTU 9000

// Use v2 as v3 would only poll on block-level
// But maybe change to v3 as it allows variable sized frames
#define TPACKET_VERSION TPACKET_V2
#define HDRLEN TPACKET2_HDRLEN

#define BLOCK_SIZE (1 << 22)
#define FRAME_SIZE (1 << 14)
#define BLOCK_NR (8)
#define FRAME_NR (BLOCK_NR * BLOCK_SIZE / FRAME_SIZE)

#endif // COMMON_H
