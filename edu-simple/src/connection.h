#ifndef CONNECTION_H
#define CONNECTION_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Operation code for read requests
 */
#define OP_READ 1

/**
 * @brief Operation code for write requests
 */
#define OP_WRITE 2

/**
 * @brief Operation code for initial DMA requests
 */
#define DISAGG_DEV_OP_DMA_MAP 3

/*
 * Instructs device to encrypt specific memory region into shmem
 * 1st message (host -> proxy): struct guest_message_header: addr = proxyDMA
 * 2nd message (proxy -> host): completion information (0 for success, 1 for failure), size of message 1 byte
 */
#define DISAGG_DEV_OP_DMA_ENC 4

/*
 * Instructs device to decrypt specific memory region into its own virtual address space
 * 1st message (host -> proxy): struct guest_message_header: addr = proxyDMA
 * 2nd message (proxy -> host): completion information (0 for success, 1 for failure), size of message 1 byte
 */
#define DISAGG_DEV_OP_DMA_DEC 5

/*
 * @brief Instructs proxy to send its proxyDMA address
 * Only done once during initialization
 */
#define DISAGG_DEV_OP_ADDR_INIT 6

/*
 * @brief VM sends the pyhsical address of its mapped region (EDU BAR)
 * Only done once during initialization
 * Done in two sends to get BAR number and then region
 * 1st message: address == physical address 
 * 2nd message: bar nr (no guest_message_header) size = 1B
 */
#define DISAGG_DEV_OP_BAR_PHYS 7

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
    uint64_t vmPhys; // The physical address of this BAR on the VM; set with OP == 7 during init
    region_access_cb_t *cb;
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

#endif // CONNECTION_H
