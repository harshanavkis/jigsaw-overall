#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <arpa/inet.h> // For htons
#include <net/if.h> // For if_nametoindex
#include <linux/if_ether.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <poll.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <netinet/ether.h> // For address conversion

#include "../../include/common.h"

static int ps = -1;

static char *remoteMacString = NULL;
static char *localInterfaceString = NULL;

static uint8_t *rx_ring = NULL;
static uint8_t *tx_ring = NULL;
static size_t mapped_size = 0;

static size_t tx_frame_idx = 0;
static size_t rx_idx_head = 0;
static size_t rx_idx_tail = 0;

static struct pollfd tx_pfd;
static struct pollfd rx_pfd;

static struct ethhdr tx_ethernet_hdr;

static void cleanup(void)
{
    if (ps != -1)
	close(ps);
    
    if (rx_ring)
	munmap(rx_ring, mapped_size);
}

void signal_handler(int signum) {
    printf("signal_handler: received signal %d\n", signum);

    cleanup();

    exit(EXIT_FAILURE);
}

void *eth_recv_first(bool wait)
{
    void *frame;
    struct tpacket2_hdr *tphdr;
    void *payload;

    frame = rx_ring + (rx_idx_head * FRAME_SIZE);

    tphdr = frame;

    if (rx_idx_head == rx_idx_tail) {
	while ((tphdr->tp_status & TP_STATUS_USER) == 0) {
	    if (wait)
		poll(&rx_pfd, 1, -1);
	    else
		return NULL;
	}

	rx_idx_tail = (rx_idx_tail + 1) % FRAME_NR;
    }

    payload = frame + tphdr->tp_mac + sizeof(struct ethhdr);

    return payload;
}

void *eth_recv_next(bool wait, uint8_t *address)
{
    void *frame;
    ssize_t idx;
    struct tpacket2_hdr *tphdr;
    void *payload;

    idx = (address - rx_ring) / FRAME_SIZE;

    if (idx < 0 || idx >= FRAME_NR) {
	fprintf(stderr, "eth_recv_next: Invalid address\n");
	cleanup();
	exit(EXIT_FAILURE);
    }

    idx = (idx + 1) % FRAME_NR;

    frame = rx_ring + (idx * FRAME_SIZE);

    tphdr = frame;

    while ((tphdr->tp_status & TP_STATUS_USER) == 0) {

	if ((size_t)idx != rx_idx_tail) {

	    idx = (idx + 1) % FRAME_NR;

	    frame = rx_ring + (idx * FRAME_SIZE);

	    tphdr = frame;

	} else {

	    if (wait)
		poll(&rx_pfd, 1, -1);
	    else
		return NULL;

	}

    }

    payload = frame + tphdr->tp_mac + sizeof(struct ethhdr);

    if ((size_t)idx == rx_idx_tail)
	rx_idx_tail = (rx_idx_tail + 1) % FRAME_NR;

    return payload;
}

/*
 * Changes state of this frame to be again available for the kernel
 * @address is a pointer returned by eth_recv_first or eth_recv_next
 */
void eth_recv_done(uint8_t *address)
{
    void *frame;
    ssize_t idx;
    struct tpacket2_hdr *tphdr;

    idx = (address - rx_ring) / FRAME_SIZE;

    if (idx < 0 || idx >= FRAME_NR) {
	fprintf(stderr, "eth_recv_done: Invalid address\n");
	cleanup();
	exit(EXIT_FAILURE);
    }

    frame = rx_ring + (idx * FRAME_SIZE);

    tphdr = frame;

    tphdr->tp_status = TP_STATUS_KERNEL;

    // Advance the global current idx indicator, if possible
    if ((size_t)idx == rx_idx_head) {

	rx_idx_head = (rx_idx_head + 1) % FRAME_NR;

	while (rx_idx_head != rx_idx_tail) {

	    frame = rx_ring + (rx_idx_head * FRAME_SIZE);

	    tphdr = frame;

	    if ((tphdr->tp_status & TP_STATUS_USER) != 0)
		break;

	    rx_idx_head = (rx_idx_head + 1) % FRAME_NR;
	}
    }
}

void *eth_get_send_buf(size_t size)
{
    void *frame;
    struct tpacket2_hdr *tphdr;
    void *data;

    if (size > MTU) {
	fprintf(stderr, "eth_get_send_buf: too big size argument\n");
	return NULL;
    }

    frame = tx_ring + (tx_frame_idx * FRAME_SIZE);

    tx_frame_idx = (tx_frame_idx + 1) % FRAME_NR;

    tphdr = frame;

    data = frame + TPACKET_ALIGN(sizeof(*tphdr));

    while (tphdr->tp_status != TP_STATUS_AVAILABLE) { 
	poll(&tx_pfd, 1, -1);
    }

    memcpy(data, &tx_ethernet_hdr, sizeof(tx_ethernet_hdr));

    tphdr->tp_len = size + sizeof(tx_ethernet_hdr);

    return data + sizeof(tx_ethernet_hdr);
}

int eth_send_buf(uint8_t *address)
{
    void *frame;
    ssize_t idx;
    struct tpacket2_hdr *tphdr;
    int ret;

    idx = (address - tx_ring) / FRAME_SIZE;

    frame = tx_ring + (idx * FRAME_SIZE);

    tphdr = frame;

    tphdr->tp_status = TP_STATUS_SEND_REQUEST;

    ret = send(ps, NULL, 0, 0);

    if (ret == -1) {
	perror("send failed");
	return 1;
    }

    return 0;
}

static int init_signal_handler(void)
{
    struct sigaction sig_action[1];
    memset(&sig_action[0], 0, sizeof(struct sigaction));
    sig_action[0].sa_handler = signal_handler;

    if (sigaction(SIGINT, &sig_action[0], NULL) != 0) {
	perror("sigaction failed for SIGINT");
	return 1;
    }	
    if (sigaction(SIGTERM, &sig_action[0], NULL) != 0) {
	perror("sigaction failed for SIGINT");
	return 1;
    }	

    return 0;
}

static void print_usage(void)
{
    printf("usage: [program_name]\n");
    printf("\t[--remoteMAC [remote's interface MAC]             or -a]\n");
    printf("\t[--localInterface [local interface name]          or -b]\n");
}

static int init_params(int argc, char **argv)
{
    int op;

    /*** Read command line arguments ***/
    struct option long_opts[] = {
	{ "remoteMAC", 1, NULL, 'a' },
	{ "localInterface", 1, NULL, 'b' },
	{ NULL, 0, NULL, 0 }
    };

    while ((op = getopt_long(argc, argv, "a:b:", long_opts, NULL)) != -1) {
	switch (op) {
	case 'a':
	    remoteMacString = optarg;
	    break;
	case 'b':
	    localInterfaceString = optarg;
	    break;
	default:
	    print_usage();
	    return 1;
	}
    }

    if (!remoteMacString || !localInterfaceString) {
	printf("All arguments have to be specified\n");
	print_usage();
	return 1;
    }

    return 0;
}

static int init_socket(void)
{
    int ret;
    struct sockaddr_ll saddr, bsaddr;
    int version = TPACKET_VERSION;
    int ifidx;
    struct ether_addr *remoteMac;
    socklen_t bsaddr_len = sizeof(bsaddr);

    ps = socket(AF_PACKET, SOCK_RAW, htons(ETH_PROT_NR));
    if (ps < 0) {
	perror("socket() failed");
	return 1;
    }

    ret = setsockopt(ps, SOL_PACKET, PACKET_VERSION, &version, sizeof(version));
    if (ret < 0) {
	perror("setsockopt() failed");
	return 1;
    }

    /*** Bind to interface ***/
    ifidx = if_nametoindex(localInterfaceString);
    if (ifidx == 0) {
	perror("if_nametoindex failed");
	return 1;
    }

    memset(&saddr, 0, sizeof(saddr));
    saddr.sll_family = AF_PACKET;
    saddr.sll_protocol = htons(ETH_PROT_NR);
    saddr.sll_ifindex = ifidx;

    ret = bind(ps, (struct sockaddr *)&saddr, sizeof(struct sockaddr_ll));
    if (ret != 0) {
	perror("bind to local interface failed");
	return 1;
    }


    /*** Init the addresses ***/

    // Convert remoteMac to bytes array
    remoteMac = ether_aton(remoteMacString);
    if (!remoteMac) {
	fprintf(stderr, "Invalid remoteMac: %s\n", remoteMacString);
	print_usage();
	return 1;
    }

    memcpy(tx_ethernet_hdr.h_dest, remoteMac, sizeof(*remoteMac));

    // Get interface MAC bytes
    ret = getsockname(ps, (struct sockaddr *)&bsaddr, &bsaddr_len);
    if (ret != 0) {
	perror("getsockname for MAC interface address failed");
	return 1;
    }

    memcpy(tx_ethernet_hdr.h_source, &bsaddr.sll_addr, sizeof(tx_ethernet_hdr.h_source));

    tx_ethernet_hdr.h_proto = htons(ETH_PROT_NR);

    return 0;
}

static int init_rings(void)
{
    struct tpacket_req tpreq_rx, tpreq_tx;
    int ret;

    /*** Setup up the rx and tx ring ***/
    tpreq_rx.tp_block_size = BLOCK_SIZE;
    tpreq_rx.tp_frame_size = FRAME_SIZE;
    tpreq_rx.tp_block_nr = BLOCK_NR;
    tpreq_rx.tp_frame_nr = FRAME_NR;

    tpreq_tx.tp_block_size = BLOCK_SIZE;
    tpreq_tx.tp_frame_size = FRAME_SIZE;
    tpreq_tx.tp_block_nr = BLOCK_NR;
    tpreq_tx.tp_frame_nr = FRAME_NR;

    ret = setsockopt(ps, SOL_PACKET, PACKET_RX_RING, &tpreq_rx, sizeof(tpreq_rx));
    if (ret != 0) {
	perror("setsockopt for rx ring failed");
	return 1;
    }

    setsockopt(ps, SOL_PACKET, PACKET_TX_RING, &tpreq_tx, sizeof(tpreq_tx));
    if (ret != 0) {
	perror("setsockopt for tx ring failed");
	return 1;
    }

    mapped_size = (tpreq_rx.tp_block_size * tpreq_rx.tp_block_nr) + (tpreq_tx.tp_block_size * tpreq_tx.tp_block_nr);

    rx_ring = mmap(0, 
		   mapped_size, 
		   PROT_READ | PROT_WRITE, MAP_SHARED, ps, 0);

    if (rx_ring == MAP_FAILED) {
	perror("mmap failed");
	return 1;
    }

    tx_ring = rx_ring + (tpreq_rx.tp_block_size * tpreq_rx.tp_block_nr);

    /*** Setup poll for rx and tx ***/
    rx_pfd.fd = ps;
    rx_pfd.revents = 0;
    rx_pfd.events = POLLIN | POLLRDNORM | POLLERR;

    tx_pfd.fd = ps;
    tx_pfd.revents = 0;
    tx_pfd.events = POLLOUT;

    return 0;
}


int init_ethernet(int argc, char **argv)
{
    int ret = 0;

    ret = init_signal_handler();
    if (ret != 0) {
	fprintf(stderr, "init_signal_handler() failed\n");
	goto out;
    }

    ret = init_params(argc, argv);
    if (ret != 0) {
	fprintf(stderr, "init_params() failed\n");
	goto out;
    }

    ret = init_socket();
    if (ret != 0) {
	fprintf(stderr, "init_socket() failed\n");
	goto out;
    }

    ret = init_rings();
    if (ret != 0) {
	fprintf(stderr, "init_rings() failed\n");
	goto out;
    }

    return 0;

out:
    cleanup();
    return 1;
}
