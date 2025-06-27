#ifndef CONNECTION_H
#define CONNECTION_H

#include <stdint.h>
#include <stdbool.h>

#include "../../include/common.h"

typedef uint64_t dma_addr_t;

/**
 * @brief Operation code for read requests
 */
#define OP_READ 1

/**
 * @brief Operation code for write requests
 */
#define OP_WRITE 2

/**
 * @brief Structure representing the message header
 */
struct guest_message_header
{
    uint8_t operation; /**< Operation type */
    uint64_t address;  /**< Memory address for the operation */
    uint32_t length;   /**< Length of data to read or write */
} __attribute__((packed));

typedef size_t (region_access_cb_t)(void *opaque, char *buf, size_t count, size_t offset, bool is_write);

/**
 * @brief Structure representing the PCI BARs in a device
 */
typedef struct disagg_pci_bar_info
{
    uint64_t *addr; // Address of region, -1 means not mapped
    uint64_t *size; // Size of region
    region_access_cb_t *cb;
    uint64_t vmPhys; // The physical address of this BAR on the VM; read once
    bool vmPhys_valid;
} disagg_pci_bar_info;

/**
 * @brief Number of PCI regions including config space and BARs
 */
#define PCI_NUM_REGIONS 7

/**
 * @brief Structure representing the PCI information that is passed to the shmem thread
 */
typedef struct disagg_pci_dev_info
{   
    disagg_pci_bar_info regions[PCI_NUM_REGIONS];
} disagg_pci_dev_info;

int get_pci_region(disagg_pci_dev_info *disagg_pci_info, uint64_t addr, uint32_t size);

void *run_shmem_app(disagg_pci_dev_info* arg, void *opaque);

void *proxyDMA_to_proxyShmem(void *proxyDMA);

// Offset of this address from start of shared memory region
uint64_t proxyDMA_offset(dma_addr_t proxyDMA);

#define DMA_REGION_OFFSET (1 << 12) // 4K aligned
#define DMA_SIZE (SHMEM_SIZE - DMA_REGION_OFFSET)

/* Offsets in the shared memory with special values */
#define OFFSET_PROXY_DMA (256)
#define OFFSET_BAR_PHYS_ADDR (264)


#endif // CONNECTION_H
