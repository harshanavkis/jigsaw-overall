#ifndef DISAGG_ETHERNET_H
#define DISAGG_ETHERNET_H

#include <stdbool.h>

int init_ethernet(int argc, char **argv);

/*
 * Send mechanism
 * Usage: 	1. eth_get_send_buf to acquire a buffer for the payload data
 * 		2. eth_send_buf to send a previously acquired buffer
 */

/*
 * @return pointer to a buffer
 */
void *eth_get_send_buf(size_t size);

/*
 * Send a payload which was previously acquired @address through eth_get_send_buf
 * @return 0 for success
 */
int eth_send_buf(uint8_t *address);


/*
 * Recv mechanism
 * Usage: 	1. eth_recv_first returns an address, if available.
 * 		2. If the payload is accepted and the user is done with it, call eth_recv_done(address).
 * 		3. Otherwise if not accepted call eth_recv_next(address), returns address if available.
 * 		4. If payload not accepted go to step 3. Otherwise, call eth_recv_done(address) if done with it.
 */

/*
 * set @wait to true, if application should block until a packet is available
 * set @wait to false, if application should return immediatly if no packet is available
 * @return NULL if non available, otherwise pointer to payload
 */
void *eth_recv_first(bool wait);

/*
 * @address has to be a value returned by eth_recv_first
 * set @wait to true, if application should block until a packet is available
 * set @wait to false, if application should return immediatly if no packet is available
 * @return NULL if non available, otherwise pointer to payload
 */
void *eth_recv_next(bool wait, uint8_t *address);

/*
 * Changes state of this frame to be again available for the kernel
 * @address is a pointer returned by eth_recv_first or eth_recv_next
 */
void eth_recv_done(uint8_t *address);

#endif // DISAGG_ETHERNET_H
