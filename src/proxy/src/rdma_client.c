// The initial structure of this file was taken from: https://github.com/linux-rdma/rdma-core/blob/master/librdmacm/examples/rdma_client.c
// But it was modified to work for this project
/*
 * Copyright (c) 2010 Intel Corporation.  All rights reserved.
 *
 * This software is available to you under the OpenIB.org BSD license
 * below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AWV
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

#include "../../include/common.h"

static struct rdma_cm_id *id;
static struct ibv_mr *mr_recv[NUM_RECV_BUFS];
static int send_flags;
static uint8_t recv_msg[NUM_RECV_BUFS][BUFS_SIZE];

struct ibv_mr *mr_shmem;

static void deregister_mregions(void)
{
	rdma_dereg_mr(mr_shmem);

	for (int i = 0; i < NUM_RECV_BUFS; ++i) {
		rdma_dereg_mr(mr_recv[i]);
	}
}

// Allocates regions and
// Registers regions in struct regions_dma with the RDMA side
static int register_mregions(void *shmem)
{
	struct ibv_mr *res_mr;

	// Now, the recv buffers, input for rdma_post_recv
	int i = 0;
	for (; i < NUM_RECV_BUFS; ++i) {
		res_mr = rdma_reg_msgs(id, recv_msg[i], BUFS_SIZE);
		if (!res_mr) {
			perror("rdma_reg_msgs");
			goto free_dereg;
		}
		mr_recv[i] = res_mr;
	}

	// The shmem buffer; contains decrypted DMA region
	res_mr = ibv_reg_mr(id->pd, shmem + DMA_REGION_OFFSET, DMA_SIZE, IBV_ACCESS_LOCAL_WRITE | 
			IBV_ACCESS_REMOTE_READ |
			IBV_ACCESS_REMOTE_WRITE);
	if (!res_mr) {
		perror("rdma_reg_msgs for remote DMA");
		goto free_dereg;
	}
	mr_shmem = res_mr;

	return 0;

free_dereg:
	--i;
	for (; i >= 0; --i) {
		rdma_dereg_mr(mr_recv[i]);
	}

	return 1;
}

int rdma_send(void *buf, size_t size)
{
#ifdef DEBUG_MESSAGES
	printf("rdma_send:\nlength: %lu\nbuf: ", size);
	for (size_t i = 0; i < size; ++i)
		printf("%x", *((unsigned char *)buf + i));
	printf("\n\n");
#endif
	int ret; 
	struct ibv_wc wc;

	ret = rdma_post_send(id, NULL, buf, size, mr_shmem, send_flags);
	if (ret) {
		perror("rdma_post_send");
		goto out;
	}

	while ((ret = rdma_get_send_comp(id, &wc)) == 0);
	if (ret < 0)
		perror("rdma_get_send_comp");
	else
		ret = 0;

out:
	return ret;
}


// This is the rdma_get_recv_comp from the rdma-core library (https://github.com/linux-rdma/rdma-core/blob/5abc21f894bf39eaaa3ecee14a0a3358eabfd121/librdmacm/rdma_verbs.h#L283) with the exception that it only does one iteration and not loop until recv is available. This allows non-blocking implementation.
	static inline int
rdma_get_recv_comp_one_iter(struct rdma_cm_id *id, struct ibv_wc *wc)
{
	struct ibv_cq *cq;
	void *context;
	int ret;

	ret = ibv_poll_cq(id->recv_cq, 1, wc);
	if (ret)
		goto out;

	ret = ibv_req_notify_cq(id->recv_cq, 0);
	if (ret)
		return rdma_seterrno(ret);

	ret = ibv_poll_cq(id->recv_cq, 1, wc);
	if (ret)
		goto out;

	ret = ibv_get_cq_event(id->recv_cq_channel, &cq, &context);
	if (ret == -1)
		return 0;

	assert(cq == id->recv_cq && context == id);
	ibv_ack_cq_events(id->recv_cq, 1);

out:
	return (ret < 0) ? rdma_seterrno(ret) : ret;
}

ssize_t rdma_recv(void **ret_buf)
{
	struct ibv_wc wc;
	uint64_t i;
	int ret;
	uint64_t buddy;

	ret = rdma_get_recv_comp_one_iter(id, &wc);
	if (ret < 0) {
		perror("rdma_get_recv_comp_one_iter");
		return -1;
	} else if (ret == 0) {
		return 0;
	}

	i = wc.wr_id;

	if (i >= NUM_RECV_BUFS) {
		printf("invalid recv buffer id\n");
		return -1;
	}

	// Get buddy index
	if (i < NUM_RECV_BUFS / 2) {
		buddy = i + (NUM_RECV_BUFS / 2);
	} else {
		buddy = i - (NUM_RECV_BUFS / 2);
	}

	ret = rdma_post_recv(id, (void *) buddy, recv_msg[buddy], BUFS_SIZE, mr_recv[buddy]);
	if (ret) {
		perror("rdma_post_recv");
		return -1;
	}

	*ret_buf = recv_msg[i];

#ifdef DEBUG_MESSAGES
	printf("rdma_recv:\nlength: %d\nbuf: ", wc.byte_len);
	for (ssize_t n = 0; n < wc.byte_len; ++n)
		printf("%x", recv_msg[i][n]);
	printf("\n\n");
#endif
	return wc.byte_len;
}

int init_rdma(char *server, char *port, void *shmem)
{
	struct rdma_addrinfo hints, *res;
	struct ibv_qp_init_attr attr;
	struct ibv_wc wc;
	int ret;

	memset(&hints, 0, sizeof hints);
	hints.ai_port_space = RDMA_PS_TCP;
	ret = rdma_getaddrinfo(server, port, &hints, &res);
	if (ret) {
		printf("rdma_getaddrinfo: %s\n", gai_strerror(ret));
		goto out;
	}

	memset(&attr, 0, sizeof attr);
	attr.cap.max_send_wr = attr.cap.max_recv_wr = NUM_RECV_BUFS / 2;
	attr.cap.max_send_sge = attr.cap.max_recv_sge = 1;
	attr.cap.max_inline_data = BUFS_SIZE;
	attr.qp_context = id;
	attr.sq_sig_all = 1;
	attr.qp_type = IBV_QPT_RC;
	ret = rdma_create_ep(&id, res, NULL, &attr);
	// Check to see if we got inline data allowed or not
	if (attr.cap.max_inline_data >= BUFS_SIZE)
		send_flags = IBV_SEND_INLINE;
	else
		printf("rdma_client: device doesn't support IBV_SEND_INLINE, "
				"using sge sends\n");

	if (ret) {
		perror("rdma_create_ep");
		goto out_free_addrinfo;
	}

	// Register all regions
	ret = register_mregions(shmem);
	if (ret != 0)
		goto out_dereg;

	// Post first half of recv buffers
	for (uint64_t i = 0; i < NUM_RECV_BUFS / 2; ++i) {
		ret = rdma_post_recv(id, (void *) i, recv_msg[i], BUFS_SIZE, mr_recv[i]);
		if (ret) {
			perror("rdma_post_recv");
			goto out_dereg;
		}
	}

	// Connect to server
	ret = rdma_connect(id, NULL);
	if (ret) {
		perror("rdma_connect");
		goto out_dereg;
	}


	/*** Send metadata with information of dma region to server ***/
	// The server needs this information to be able to perfrom read/writes

	// Register temporary memory region
	struct ibv_mr *rkey_mr = NULL;

	if ((send_flags & IBV_SEND_INLINE) == 0) {
		rkey_mr = rdma_reg_msgs(id, &mr_shmem->rkey, sizeof(mr_shmem->rkey));
		if (!rkey_mr) {
			perror("rdma_reg_msgs");
			goto out_disconnect;
		}
	}

	// Send the necessary metadata to the server
	printf("rkey: 0x%x\n", mr_shmem->rkey);

	ret = rdma_post_send(id, NULL, &mr_shmem->rkey, sizeof(mr_shmem->rkey), rkey_mr, send_flags);
	if (ret) {
		perror("rdma_post_send");
		goto out_meta_dma;
	}

	while ((ret = rdma_get_send_comp(id, &wc)) == 0);
	if (ret < 0) {
		perror("rdma_get_send_comp");
		goto out_meta_dma;
	}

	if (rkey_mr)
		rdma_dereg_mr(rkey_mr);
	/***/

	/* change recv queue to non-blocking */
	// Copied this section from https://www.rdmamojo.com/2013/03/09/ibv_get_cq_event/
	int flags;
	flags = fcntl(id->recv_cq_channel->fd, F_GETFL);
	ret = fcntl(id->recv_cq_channel->fd, F_SETFL, flags | O_NONBLOCK);
	if (ret < 0) {
		fprintf(stderr, "Failed to change file descriptor of Completion Event Channel\n");
		goto out_meta_dma;
	}

	return 0;

out_meta_dma:
	if (rkey_mr)
		rdma_dereg_mr(rkey_mr);
out_disconnect:
	rdma_disconnect(id);
out_dereg:
	deregister_mregions();
	rdma_destroy_ep(id);
out_free_addrinfo:
	rdma_freeaddrinfo(res);
out:
	return ret;
}

