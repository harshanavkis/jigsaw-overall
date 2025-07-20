#ifndef MMIO_H
#define MMIO_H

#include <stdint.h>
#include <stdbool.h>

#include "../../include/common.h"

typedef uint64_t dma_addr_t;

typedef size_t (region_access_cb_t)(void *opaque, char *buf, size_t count, size_t offset, bool is_write);

/**
 * @brief Structure representing the PCI BARs in a device
 */
typedef struct disagg_pci_bar_info {
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
typedef struct disagg_pci_dev_info {   
    disagg_pci_bar_info regions[PCI_NUM_REGIONS];
} disagg_pci_dev_info;

void *run_mmio_app(disagg_pci_dev_info* arg, void *opaque);

#endif // MMIO_H
