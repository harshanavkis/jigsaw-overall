#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>

#include <sys/mman.h>

#include "rdma_client.h"
#include "message.h"

void print_help(char **argv)
{
    printf("usage: %s\n", argv[0]);
    printf("\t--remoteAddress <IP address of remote host>                 or -a\n");
    printf("\t--remotePort <Port of remote host>                          or -p\n");
    printf("\t--size <Size of the buffer to be transferred>               or -s\n");
    printf("\t--direction [D2H | H2D]                                     or -d\n");
    printf("\t--iterations <Number of iterations>                         or -i\n");
}

int main(int argc, char **argv)
{
	int op, ret;
	ssize_t ret_size;
	char *local_buf = NULL;
    char *send_buf = NULL;
    struct msg *msg_send_header;
    struct msg *msg_recv_header;
    size_t size_send_buf = sizeof(struct msg);
	char *remoteAddr = NULL;
	char *remotePort = NULL;
    size_t size = 1024;
    size_t direction = 0; // 0 for D2H, 1 for H2D
    size_t iterations = 10;

	/*** Read command line arguments ***/
	struct option long_opts[] = {
		{ "remoteAddress", 1, NULL, 'a' },
		{ "remotePort", 1, NULL, 'p' },
        { "size", 1, NULL, 's' },
        { "direction", 1, NULL, 'd' },
        { "iterations", 1, NULL, 'i' },
		{ NULL, 0, NULL, 0 }
	};

	while ((op = getopt_long(argc, argv, "a:p:s:d:i:", long_opts, NULL)) != -1) {
		switch (op) {
			case 'a':
				remoteAddr = optarg;
				break;
			case 'p':
				remotePort = optarg;
				break;
            case 's':
                size = strtoll(optarg, NULL, 0);
                if (errno == EINVAL || errno == ERANGE) {
                    fprintf(stderr, "Error: Invalid value \"%s\" for option size\n", optarg);
                    print_help(argv);
                    goto err;
                }
                break;
            case 'd':
                if (strncmp(optarg, "D2H", 3) == 0) {
                    direction = 0;
                } else if (strncmp(optarg, "H2D", 3) == 0) {
                    direction = 1;
                } else {
                    fprintf(stderr, "Error: Invalid value \"%s\" for option direction\n", optarg);
                    print_help(argv);
                    goto err;
                }
                break;
            case 'i':
                iterations = strtoll(optarg, NULL, 0);
                if (errno == EINVAL || errno == ERANGE) {
                    fprintf(stderr, "Error: Invalid value \"%s\" for option iterations\n", optarg);
                    print_help(argv);
                    goto err;
                }
                break;
			default:
                print_help(argv);
				goto err;
		}
	}

	if (!remoteAddr || !remotePort) {
		fprintf(stderr, "Error: IP address and port both necessary\n");
        print_help(argv);
		goto err;
	}

    // Alloc local buffer used for rdma read/write
    local_buf = malloc(size);
    if (!local_buf) {
		printf("malloc for local_buf failed\n");
		goto err;
    }

    // Alloc buffer used rdma send
    send_buf = malloc(size_send_buf);
    if (!send_buf) {
		printf("malloc for send_buf failed\n");
		goto err;
    }

    msg_send_header = (struct msg *) send_buf;

	printf("rdma_client: start\n");
	ret = init_rdma(remoteAddr, remotePort, local_buf, size, send_buf, size_send_buf);
	if (ret != 0) {
		goto err_unmap;
	} 

    // Fill the msg fields with the corresponding instruction (will not change over the course of the iterations)
    msg_send_header->type = 0;
    msg_send_header->size = size;
    msg_send_header->direction = direction;

    /* Main loop to do the transfers */
    for (size_t i = 0; i < iterations; ++i) {
        // Instruct the server
        if (rdma_send(msg_send_header, size_send_buf) != 0) {
            fprintf(stderr, "rdma_send failed\n");
            goto err_unmap;
        }

        // Wait for completion response
        ret_size = 0;
        while (ret_size == 0) 
            ret_size = rdma_recv((void *)&msg_recv_header);
    
        if (ret_size == -1) {
            fprintf(stderr, "rdma_recv failed\n");
            goto err_unmap;
        }

        if (msg_recv_header->type != 1) {
            fprintf(stderr, "Server messaged failure of operation\n");
            goto err_unmap;
        }
    }


	printf("rdma_client: end %d\n", ret);
	exit(EXIT_SUCCESS);

err_unmap:
	munmap(local_buf, size);
err:
	exit(EXIT_FAILURE);
}

