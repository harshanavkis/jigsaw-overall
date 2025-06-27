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
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

#include "../../include/common.h"

static struct rdma_cm_id *id;
static struct ibv_mr *mr_recv[NUM_RECV_BUFS], *send_mr;
static int send_flags;
static uint8_t send_msg[BUFS_SIZE];
static uint8_t recv_msg[NUM_RECV_BUFS][BUFS_SIZE];

struct ibv_mr *mr_shmem;

static void deregister_mregions(void)
{
    rdma_dereg_mr(mr_dma);

    for (int i = 0; i < NUM_RECV_BUFS; ++i) {
	rdma_dereg_mr(mr_recv[i]);
    }

    if ((send_flags & IBV_SEND_INLINE) == 0)
	rdma_dereg_mr(send_mr);
}

// Allocates regions and
// Registers regions in struct regions_dma with the RDMA side
static int register_mregions(void)
{
    struct ibv_mr *res_mr;
    void *res_buf;

    // First, the enc_buf, input for rdma_post_send
    if ((send_flags & IBV_SEND_INLINE) == 0) {
	res_mr = rdma_reg_msgs(id, send_msg, BUFS_SIZE);
	if (!res_mr) {
	    perror("rdma_reg_msgs");
	    goto out;
	}
	send_mr = res_mr;
    } else {
	send_mr = NULL;
    }

    // Now, the recv buffers, input for rdma_post_recv
    int i = 0;
    for (; i < NUM_RECV_BUFS; ++i) {
	res_mr = rdma_reg_msgs(id, recv_msg[i], BUFS_SIZE);
	if (!res_mr) {
	    perror("rdma_reg_msgs");
	    free(res_buf);
	    goto free_dereg;
	}
	mr_recv[i] = res_mr;
    }

    // The shmem buffer; contains decrypted DMA region
    res_mr = ibv_reg_mr(id->pd, shmem, SHMEM_SIZE, IBV_ACCESS_LOCAL_WRITE | 
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
	rdma_dereg_mr(regions_rdma.recv_bufs[i].mr);
    }

    if ((send_flags & IBV_SEND_INLINE) == 0)
	rdma_dereg_mr(regions_rdma.mr_enc_send_buf);
out:
    return 1;
}

int init_rdma(char *server, char *port)
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
	attr.cap.max_send_wr = attr.cap.max_recv_wr = 8;
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
	ret = register_mregions();
	if (ret != 0)
	    goto out_dereg_send;

	// Post first half of recv buffers
	for (uint64_t i = 0; i < NUM_RECV_BUFS / 2; ++i) {
	    ret = rdma_post_recv(id, (void *) i; recv_msg[i], BUFS_SIZE, mr_recv[i]);
	    if (ret) {
		perror("rdma_post_recv");
		goto out_dereg_send;
	    }
	}

	// Connect to server
	ret = rdma_connect(id, NULL);
	if (ret) {
		perror("rdma_connect");
		goto out_dereg_send;
	}

	{

	    // Send the necessary metadata to the server
	    struct meta_remote_info *meta_dma_reg = (struct meta_remote_info *) send_msg;
	    meta_dma_reg->remote_addr = (uint64_t) dma_reg;
	    meta_dma_reg->rkey = mr_dma->rkey;

	    printf("addr: %p, rkey: 0x%x\n", dma_reg, meta_dma_reg->rkey);

	    ret = rdma_post_send(id, NULL, send_msg, sizeof(*meta_dma_reg), send_mr, send_flags);
	    if (ret) {
		    perror("rdma_post_send");
		    goto out_disconnect;
	    }

	    while ((ret = rdma_get_send_comp(id, &wc)) == 0);
	    if (ret < 0) {
		    perror("rdma_get_send_comp");
		    goto out_disconnect;
	    }

	}

	/*
	ret = rdma_post_send(id, NULL, send_msg, 16, send_mr, send_flags);
	if (ret) {
		perror("rdma_post_send");
		goto out_disconnect;
	}

	while ((ret = rdma_get_send_comp(id, &wc)) == 0);
	if (ret < 0) {
		perror("rdma_get_send_comp");
		goto out_disconnect;
	}
	*/

	while ((ret = rdma_get_recv_comp(id, &wc)) == 0);
	if (ret < 0)
		perror("rdma_get_recv_comp");
	else
		ret = 0;


	// Print dma_region and test if it worked
	for (uint64_t i = 0; i < dma_size; ++i)
	    printf("%02x ", dma_reg[i]);
	printf("\n");

out_disconnect:
	rdma_disconnect(id);
out_dereg_send:
	free(dma_reg);
	if ((send_flags & IBV_SEND_INLINE) == 0)
		rdma_dereg_mr(send_mr);
out_dereg_recv:
	rdma_dereg_mr(mr);
out_destroy_ep:
	rdma_destroy_ep(id);
out_free_addrinfo:
	rdma_freeaddrinfo(res);
out:
	return ret;
}

