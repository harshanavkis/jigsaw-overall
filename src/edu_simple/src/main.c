#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>

#include "edu.h"
#include "connection.h"
#include "sec_disagg.h"
#include "rdma_server.h"

int main(int argc, char **argv) {

    /** Initialize everything related to the device emulation **/
    EduState *edu = init_edu_device();

    if (edu == NULL)
	exit(EXIT_FAILURE);

    // Setup region info
    disagg_pci_dev_info *disagg_pci_info = (disagg_pci_dev_info *) malloc(sizeof(disagg_pci_dev_info));

    for (int i = 0; i < PCI_NUM_REGIONS; i++)
    {
        disagg_pci_info->regions[i].addr = &(edu->regions[i].addr);
        disagg_pci_info->regions[i].size = &(edu->regions[i].size);
        disagg_pci_info->regions[i].vmPhys = 0;
        disagg_pci_info->regions[i].vmPhys_valid = false;
	disagg_pci_info->regions[i].cb = edu->regions[i].cb;

        printf("main.c: Setting for region %d, proxy Address: 0x%" PRIx64 ", size: %lu\n", i, *(disagg_pci_info->regions[i].addr), *(disagg_pci_info->regions[i].size));
    }

    /** Setup RDMA **/
    int op, ret;
    char *serverIP = NULL;
    char *port = NULL;

    while ((op = getopt(argc, argv, "s:p:")) != -1) {
	    switch (op) {
	    case 's':
		    serverIP = optarg;
		    break;
	    case 'p':
		    port = optarg;
		    break;
	    default:
		    printf("usage: %s\n", argv[0]);
		    printf("\t[-s server_address]\n");
		    printf("\t[-p port_number]\n");
		    exit(EXIT_FAILURE);
	    }
    }

    if (!serverIP || !port) {
	printf("Both server IP address and port have to be specified\n");
	exit(EXIT_FAILURE);
    }

    ret = init_rdma(serverIP, port);
    if (ret != 0)
	exit(EXIT_FAILURE);

    run_shmem_app(disagg_pci_info, edu);
}

