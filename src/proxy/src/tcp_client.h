#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

int init_tcp(int argc, char **argv);

/*
 * Sends @buf to server.
 * Size of send is determined by the first byte of @buf
 */
int tcp_send_mmio_request(const char *buf);

/*
 * Receives data of @size and writes into @buf.
 * @flags is provided as argument to recv().
 * @return 0 for success, -1 otherwise (with errno set to error code)
 */
int tcp_recv_data(void *buf, size_t size, int flags);

/*
 * Sends @size bytes of @buf to remote.
 * @return 0 for success, -1 otherwise (with errno set to error code)
 */
int tcp_send_buf(const char *buf, size_t size);

#endif // TCP_CLIENT_H
