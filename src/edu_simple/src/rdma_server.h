#ifndef RDMA_SERVER_H
#define RDMA_SERVER_H

#include <stdlib.h>
#include <infiniband/verbs.h>
#include "../../include/common.h"

//#define CONFIG_DISAGG_DEBUG_DMA_SEC
#define CONFIG_DISAGG_DEBUG_MMIO_SEC

int init_rdma(const char *serverIP, const char *port);

int rdma_send(void *buf, size_t size);

void *rdma_recv(void);

// Writes @count bytes of the local shmem_buf at @raddr to the remote region
int rdma_read(uint64_t raddr, size_t count);

// Reads @count bytes from the remote region at @raddr to the local dma_buf 
int rdma_write(uint64_t raddr, size_t count);

int reg_dma_mr(void *buf);

struct disagg_regions_rdma {
	void *enc_send_buf; // Used as output for mmio_encrypt and source for rdma_post_send
	struct ibv_mr *mr_enc_send_buf;

	struct recv_bufs { // Used for rdma_post_recv and as input for mmio_decrypt
		void *buf;
		struct ibv_mr *mr;
	} recv_bufs[NUM_RECV_BUFS]; // Only the first half of the buffers is actually posted as recv.
				    // Every buffer in the first half has a "buddy" in the second half.
				    // Only ever one buddy of a pair is posted as recv.
				    // When a buffer is in a completed request his buddy is posted for recv.
				    // The buffer itself is returned as a result.
				    // This prevents overriding of memory regions if the result is not yet processed.

	void *dma_buf; // contains the encrypted sent DMA region; needed as the read requests need a destination
		       // Because the data cannot be directly decrypted from a rdma request
	struct ibv_mr *mr_dma_buf;

	// Used when writing to remote DMA region
	uint32_t rkey;
};

extern struct disagg_regions_rdma regions_rdma;

#endif // RDMA_SERVER_H
