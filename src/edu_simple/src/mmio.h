#ifndef MMIO_H
#define MMIO_H

#include <stdint.h>
#include <stdbool.h>

#include "../../include/common.h"

typedef uint64_t dma_addr_t;

/**
 * @brief Structure representing the message header
 */
struct guest_message_header
{
	uint8_t operation; /** Operation type */
	uint64_t address;  /** Memory address for the operation */
	uint64_t length;   /** Length of data to read or write */
	uint64_t value;    /** Value to write in case of OP_WRITE */
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

void *run_mmio_app(disagg_pci_dev_info* arg, void *opaque);

#endif // MMIO_H
