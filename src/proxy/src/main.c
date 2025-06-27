#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "unistd.h"

#include "connection.h"
#include "rdma_client.h"

int main(int argc, char **argv)
{
	int op, ret;
	char *server = NULL;
	char *port = NULL;

	while ((op = getopt(argc, argv, "s:p:")) != -1) {
		switch (op) {
		case 's':
			server = optarg;
			break;
		case 'p':
			port = optarg;
			break;
		default:
			printf("usage: %s\n", argv[0]);
			printf("\t[-s server_address]\n");
			printf("\t[-p port_number]\n");
			exit(1);
		}
	}

	if (!server || !port) {
	    fprintf(stderr, "Error: server and port both necessary");
	    return EXIT_FAILURE;
	}

	printf("rdma_client: start\n");
	ret = run(server, port);
	printf("rdma_client: end %d\n", ret);
	return ret;
}

