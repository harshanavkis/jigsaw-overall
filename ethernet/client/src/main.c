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
#include <sys/mman.h>

#include "../../include/common.h"

int ps = -1;
char *serverMacString = NULL;
char *localInterfaceString = NULL;
uint8_t *rx_ring;
uint8_t *tx_ring;

size_t tx_frame_idx = 0;
size_t rx_frame_idx = 0;

struct pollfd tx_pfd;
struct pollfd rx_pfd;

struct ethhdr tx_ethernet_hdr;

int eth_send(void *buf, size_t size)
{
    void *frame;
    struct tpacket2_hdr *tphdr;
    void *data;
    ssize_t ret;

    if (size > MTU) {
	// TODO: Split up into multiple packets
	fprintf(stderr, "eth_send: too big size argument\n");
	return 1;
    }

    frame = tx_ring + (tx_frame_idx * FRAME_SIZE);
    tx_frame_idx = (tx_frame_idx + 1) % FRAME_NR;

    tphdr = frame;
    data = frame + TPACKET_ALIGN(sizeof(*tphdr));

    while (tphdr->tp_status != TP_STATUS_AVAILABLE) { 
	poll(&tx_pfd, 1, -1);
    }

    memcpy(data, &tx_ethernet_hdr, sizeof(tx_ethernet_hdr));

    memcpy(data + sizeof(tx_ethernet_hdr), buf, size);

    tphdr->tp_len = size + sizeof(tx_ethernet_hdr);
    tphdr->tp_status = TP_STATUS_SEND_REQUEST;

    ret = send(ps, NULL, 0, 0);

    if (ret == -1) {
	perror("send failed");
	return 1;
    }

    printf("Sent frame of size\n");

    return 0;
}

ssize_t eth_recv(void *buf, size_t max_size)
{
    void *frame;
    struct tpacket2_hdr *tphdr;
    void *payload;
    size_t size;

    frame = rx_ring + (rx_frame_idx * FRAME_SIZE);
    rx_frame_idx = (rx_frame_idx + 1) % FRAME_NR;

    tphdr = frame;

    while ((tphdr->tp_status & TP_STATUS_USER) == 0) {
	poll(&rx_pfd, 1, -1);
    }

    payload = frame + tphdr->tp_mac + sizeof(struct ethhdr);

    size = tphdr->tp_len - sizeof(struct ethhdr);
    
    if (max_size > size) {
	fprintf(stderr, "eth_recv: received data size does exceed max_size\n");
	return -1;
    }

    memcpy(buf, payload, size);

    printf("Received frame of size: %lu\n", size);

    tphdr->tp_status = TP_STATUS_KERNEL;

    return size;
}


// Register signals with signal handler to close socket
void signal_handler(int signum) {
    printf("signal_handler: received signal %d\n", signum);

    if (ps != -1) 
	close(ps);

    exit(EXIT_FAILURE);
}

int init(int argc, char **argv)
{
    int op;

    /*** Register signal handler ***/
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

    /*** Read command line arguments ***/
    struct option long_opts[] = {
	{ "serverMAC", 1, NULL, 'a' },
	{ "localInterface", 1, NULL, 'b' },
	{ NULL, 0, NULL, 0 }
    };

    while ((op = getopt_long(argc, argv, "a:b:", long_opts, NULL)) != -1) {
	switch (op) {
	case 'a':
	    serverMacString = optarg;
	    break;
	case 'b':
	    localInterfaceString = optarg;
	    break;
	default:
	    printf("usage: %s\n", argv[0]);
	    printf("\t[--serverMAC [Server's interface MAC]       or -a]\n");
	    printf("\t[--localInterface [local interface]         or -b]\n");
	    return 1;
	}
    }

    if (!serverMacString || !localInterfaceString) {
	printf("All arguments have to be specified\n");
	printf("usage: %s\n", argv[0]);
	printf("\t[--serverMAC [Server's interface MAC]       or -a]\n");
	printf("\t[--localInterface [local interface]         or -b]\n");
	return 1;
    }

    // TODO: make this depend on the option
    // This is wilfred's interface MAC
    tx_ethernet_hdr.h_dest[0] = 0xb4; tx_ethernet_hdr.h_dest[1] = 0x96; tx_ethernet_hdr.h_dest[2] = 0x91; tx_ethernet_hdr.h_dest[3] = 0xb3; tx_ethernet_hdr.h_dest[4] = 0x8b; tx_ethernet_hdr.h_dest[5] = 0x04;

    // This is amy's interface MAC
    tx_ethernet_hdr.h_source[0] = 0xb4; tx_ethernet_hdr.h_source[1] = 0x96; tx_ethernet_hdr.h_source[2] = 0x91; tx_ethernet_hdr.h_source[3] = 0xb3; tx_ethernet_hdr.h_source[4] = 0x8a; tx_ethernet_hdr.h_source[5] = 0x90;

    tx_ethernet_hdr.h_proto = htons(ETH_PROT_NR);

    return 0;
}

int main(int argc, char **argv)
{
    int ret = 0;
    int version = TPACKET_VERSION;
    struct sockaddr_ll saddr;
    int ifidx;
    struct tpacket_req tpreq_rx, tpreq_tx;

    ret = init(argc, argv);
    if (ret != 0) {
	fprintf(stderr, "init() failed\n");
	goto out;
    }

    /*** Setup socket ***/
    ps = socket(AF_PACKET, SOCK_RAW, htons(ETH_PROT_NR));
    if (ps < 0) {
	perror("socket() failed");
	ret = 1;
	goto out;
    }

    ret = setsockopt(ps, SOL_PACKET, PACKET_VERSION, &version, sizeof(version));
    if (ret < 0) {
	perror("setsockopt() failed");
	goto out_sock;
    }

    /*** Bind to right interface ***/
    ifidx = if_nametoindex(localInterfaceString);
    if (ifidx == 0) {
	perror("if_nametoindex failed");
	ret = 1;
	goto out_sock;
    }

    memset(&saddr, 0, sizeof(saddr));
    saddr.sll_family = AF_PACKET;
    saddr.sll_protocol = htons(ETH_PROT_NR);
    saddr.sll_ifindex = ifidx;

    bind(ps, (struct sockaddr *)&saddr, sizeof(struct sockaddr_ll));

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
	goto out_sock;
    }

    setsockopt(ps, SOL_PACKET, PACKET_TX_RING, &tpreq_tx, sizeof(tpreq_tx));
    if (ret != 0) {
	perror("setsockopt for tx ring failed");
	goto out_sock;
    }


    rx_ring = mmap(0, 
		   (tpreq_rx.tp_block_size * tpreq_rx.tp_block_nr) + (tpreq_tx.tp_block_size * tpreq_tx.tp_block_nr),
		   PROT_READ | PROT_WRITE, MAP_SHARED, ps, 0);
    if (rx_ring == MAP_FAILED) {
	perror("mmap failed");
	ret = 1;
	goto out_sock;
    }

    tx_ring = rx_ring + (tpreq_rx.tp_block_size * tpreq_rx.tp_block_nr);

    /*** Setup poll for rx and tx ***/
    rx_pfd.fd = ps;
    rx_pfd.revents = 0;
    rx_pfd.events = POLLIN | POLLRDNORM | POLLERR;

    tx_pfd.fd = ps;
    tx_pfd.revents = 0;
    tx_pfd.events = POLLOUT;

    {
	/*** Tests ***/
	char test_recv[9000];
	char test_send[9000];
	memset(test_send, 0x54, 9000);
	char test_cmp[9000];
	memset(test_cmp, 0x32, 9000);

	// Send
	if (eth_send(test_send, 10) != 0) {
	    ret = 1;
	}

	// Recv
	if (eth_recv(test_recv, 56) == -1) {
	    ret = 1;
	}

	if (memcmp(test_cmp, test_recv, 56) != 0) {
	    printf("cmp failed\n");
	    ret = 1;
	}

	// Recv
	if (eth_recv(test_recv, 56) == -1) {
	    ret = 1;
	}

	if (memcmp(test_cmp, test_recv, 56) != 0) {
	    printf("cmp failed\n");
	    ret = 1;
	}

	// Send
	if (eth_send(test_send, 8000) != 0) {
	    ret = 1;
	}
    }

//out_mmap:
    munmap(rx_ring, (tpreq_rx.tp_block_size * tpreq_rx.tp_block_nr) + (tpreq_tx.tp_block_size * tpreq_tx.tp_block_nr));
out_sock:
    close(ps);
out:
    return ret;
}
