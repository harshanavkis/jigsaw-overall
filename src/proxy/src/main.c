#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>

#include <sys/mman.h>

#include "shmem.h"
#include "rdma_client.h"
#include "../../include/common.h"

int main(int argc, char **argv)
{
	int op, ret;
	ssize_t ret_size;
	char *server = NULL;
	char *port = NULL;
	char *shmem = NULL;
	void *buf; 

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
		goto err;
	    }
	}

	if (!server || !port) {
	    fprintf(stderr, "Error: server and port both necessary");
	    goto err;
	}

	if (init_shared_memory(&shmem) < 0) {
	    printf("init_shared_memory failed\n");
	    goto err;
	}

	printf("rdma_client: start\n");
	ret = init_rdma(server, port, shmem);
	if (ret != 0) {
	    goto err_unmap;
	} 

	/*** Check in-turn if either mmio message has arrived in shmem or server sent message ***/
	
	while (1) {
	    // Check if the vm has written something to the mmio region
	    ret = get_write_doorbell();
	    if (ret == 1) {
		// Message ready to send
		ret = rdma_send(shmem + 2, BUFS_SIZE);
		if (ret != 0) {
		    perror("rdma_send for mmio failed\n");
		    goto err_unmap;
		}

		reset_write_doorbell();
	    }

	    
	    // Check if there is a message to receive
	    ret_size = rdma_recv(&buf);
	    if (ret_size == 0) {
		continue;
	    } else if (ret_size == -1) {
		printf("error receiving message form server\n");
		goto err_unmap;
	    } else {
		// Received message. Write to shmem
		ret_size = ivshmem_write(buf, ret_size, 0);
		if (ret_size == -1) {
		    printf("shmem write failed\n");
		    goto err_unmap;
		}
	    }
	}


	// Will never reach that
	printf("rdma_client: end %d\n", ret);
	exit(EXIT_SUCCESS);

err_unmap:
	munmap(shmem, SHMEM_SIZE);
err:
	exit(EXIT_FAILURE);
}

