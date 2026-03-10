#ifndef RDMA_CLIENT_H
#define RDMA_CLIENT_H

// Number of receive buffers
#define NUM_RECV_BUFS 8

// Size of each receive buffer
#define BUFS_SIZE 92

int init_rdma(char *server,
              char *port,
              void *local_buf,
              size_t local_buf_size,
              void *send_buf,
              size_t size_send_buf
              );

/*
 * Retreives a completed recv work request if available and retruns corresponding buffer in @ret
 * @return 0 for non available
 * @return -1 for failure
 * @return size of recv otherwise
 */
ssize_t rdma_recv(void **ret);

/*
 * Sends message data of @size from @buf
 * @buf has to be a region in the mapped shared memory
 * @return 0 for success
 */
int rdma_send(void *buf, size_t size);

#endif // RDMA_CLIENT_H
