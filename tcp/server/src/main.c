#include <sys/socket.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <signal.h>
#include <arpa/inet.h>

int lfd = -1; // Listening fd
int cfd = -1; // Client fd

// Register signals with signal handler to close socket
void signal_handler(int signum) {
    printf("signal_handler: received signal %d\n", signum);

    if (lfd != -1) 
	close(lfd);
    
    if (cfd != -1)
	close(cfd);

    exit(EXIT_FAILURE);
}

// Calls recv until has received size bytes
int recv_data(int ifd, void *buf, size_t size, int flags) {
    size_t size_recv = 0;
    ssize_t ret;


    while (size_recv < size) {
	ret = recv(ifd, buf + size_recv, size - size_recv, flags);

	// Check for error
	if (ret <= 0) {
	    if (ret == 0) {
		// Peer closed connection
		printf("Connection was closed by peer\n");
		return 1;
	    } else if (ret == -1) {
		// Error
		perror("recv failed");
		return 1;
	    }
	}

	size_recv += ret;
    }

    return 0;
}

int main(int argc, char **argv) {

    int op, ret = 0;
    uint32_t ret_size;
    char *serverAddrString = NULL, *serverPortString = NULL;
    int serverPort = -1;
    struct sockaddr_in ssaddr, csaddr; // server/client sock addr
    char csaddrString[INET_ADDRSTRLEN + 1]; // Used to print the client address

    /*** Register signal handler ***/
    struct sigaction sig_action[1];
    memset(&sig_action[0], 0, sizeof(struct sigaction));
    sig_action[0].sa_handler = signal_handler;

    if (sigaction(SIGINT, &sig_action[0], NULL) != 0) {
	perror("sigaction failed for SIGINT");
	goto out;
    }	
    if (sigaction(SIGTERM, &sig_action[0], NULL) != 0) {
	perror("sigaction failed for SIGINT");
	goto out;
    }	

    /*** Read ip address and port for listening socket from command line arguments ***/
    struct option long_opts[] = {
	{ "serverAddr", 1, NULL, 'a' },
	{ "serverPort", 1, NULL, 'b' },
	{ NULL, 0, NULL, 0 }
    };

    while ((op = getopt_long(argc, argv, "a:b:c:d:", long_opts, NULL)) != -1) {
	switch (op) {
	case 'a':
	    serverAddrString = optarg;
	    break;
	case 'b':
	    serverPortString = optarg;
	    break;
	default:
	    printf("usage: %s\n", argv[0]);
	    printf("\t[--serverAddr [Server's IP address]       or -a]\n");
	    printf("\t[--serverPort [Server's Port]             or -b]\n");
	    goto out;
	}
    }

    if (!serverAddrString || !serverPortString) {
	printf("All two arguments have to be specified\n");
	printf("usage: %s\n", argv[0]);
	printf("\t[--serverAddr [Server's IP address]       or -a]\n");
	printf("\t[--serverPort [Server's Port]             or -b]\n");
	goto out;
    }

    serverPort = atoi(serverPortString);
    if (serverPort < 1024 || serverPort >= 65536) {
	printf("Invalid port number for serverPort\n");
	goto out;
    }

    /*** Create listening socket ***/
    int lfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (lfd == -1) {
	perror("Creating of listening socket failed");
	goto out;
    }

    // Convert command line arguments to address info for server address (address of this interface)
    ssaddr.sin_family = AF_INET;
    ssaddr.sin_port = htons(serverPort);
    if (0 == inet_aton(serverAddrString, &ssaddr.sin_addr)) {
	printf("Invalid serverAddr: %s\n", serverAddrString);
	goto err_sock;
    }

    ret = bind(lfd, (struct sockaddr *)&ssaddr, sizeof(ssaddr));
    if (ret != 0) {
	perror("Bind to server address failed");
	goto err_sock;	
    }

    ret = listen(lfd, 1);
    if (ret != 0){
	perror("Marking the socket as listening failed");
	goto err_sock;
    }


    /*** Accept the incoming connection ***/
    ret_size = sizeof(csaddr); // size argument of struct sockaddr is needed as pointer
    cfd = accept(lfd, (struct sockaddr *)&csaddr, &ret_size); // Blocks until incoming connection, as lfd is not marked as non-blocking

    if (cfd == -1) {
	perror("Accept failed");
	ret = 1;
	goto err_sock;
    }

    if (ret_size != sizeof(csaddr)) {
	printf("Returned size of client address is different than expected\n");
	ret = 1;
	goto err_acc;
    }

    // Print the address information of connected client
    if (NULL == inet_ntop(AF_INET, &csaddr.sin_addr.s_addr, csaddrString, sizeof(csaddrString) - 1)) {
	perror("inet_ntop failed");
	ret = 1;
	goto err_acc;
    }

    csaddrString[sizeof(csaddrString) - 1] = '\0';
    printf("Accepted connection to client: address = %s, port = %d\n", csaddrString, ntohs(csaddr.sin_port));

    /*** send/recv tests ***/
    {
	char recvBuf[64] = { 0 };
	char *sendBuf = "Hello";
	ssize_t ret;

	// Send message "Hello"
	ret = send(cfd, sendBuf, 5, 0);	
	if (ret != 5)
	    goto err_sock;

	// Recv message "World"
	ret = recv_data(cfd, recvBuf, 5, 0);
	if (ret != 0)
	    goto err_sock;

	printf("received message %s\n", recvBuf);

    }

    while(1) { }

err_acc:
    if (cfd != -1)
	close(cfd);
err_sock:
    if (lfd != -1)
	close(lfd);
out:
    return ret;
}
