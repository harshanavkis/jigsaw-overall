#ifndef RDMA_CLIENT_H
#define RDMA_CLIENT_H

int init_rdma(char *server, char *port, void *shmem);

/*
 * Retreives a completed recv work request if available and writes corresponding buffer to @ret
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
