#ifndef DISAGG_DMA_H
#define DISAGG_DMA_H

#include <stddef.h>
#include <stdint.h>

typedef uint64_t dma_addr_t;

/* Qemu API normally provides those functions */
void pci_dma_read(dma_addr_t addr, void *buf, size_t len);

void pci_dma_write(dma_addr_t addr, void *buf, size_t len);

#endif // DISAGG_DMA_H
