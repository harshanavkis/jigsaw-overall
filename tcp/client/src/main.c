#include <sys/socket.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

int fd = -1;

// Register signals with signal handler to close socket
void signal_handler(int signum) {
    printf("signal_handler: received signal %d\n", signum);

    if (fd != -1) 
	close(fd);

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
    char *serverAddrString = NULL, *serverPortString = NULL;
    char *localAddrString = NULL, *localPortString = NULL;
    int serverPort = -1;
    int localPort = -1;
    struct sockaddr_in lsaddr, ssaddr; // listening/server sock addr

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

    /*** Read command line arguments ***/
    struct option long_opts[] = {
	{ "serverAddr", 1, NULL, 'a' },
	{ "serverPort", 1, NULL, 'b' },
	{ "localAddr", 1, NULL, 'c' },
	{ "localPort", 1, NULL, 'd' },
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
	case 'c':
	    localAddrString = optarg;
	    break;
	case 'd':
	    localPortString = optarg;
	    break;
	default:
	    printf("usage: %s\n", argv[0]);
	    printf("\t[--serverAddr [Server's IP address]       or -a]\n");
	    printf("\t[--serverPort [Server's Port]             or -b]\n");
	    printf("\t[--localAddr [Local IP address]           or -c]\n");
	    printf("\t[--localPort [Local Port]                 or -d]\n");
	    goto out;
	}
    }

    if (!serverAddrString || !serverPortString || !localAddrString || !localPortString) {
	printf("All four arguments have to be specified\n");
	printf("usage: %s\n", argv[0]);
	printf("\t[--serverAddr [Server's IP address]       or -a]\n");
	printf("\t[--serverPort [Server's Port]             or -b]\n");
	printf("\t[--localAddr [Local IP address]           or -c]\n");
	printf("\t[--localPort [Local Port]                 or -d]\n");
	goto out;
    }

    serverPort = atoi(serverPortString);
    if (serverPort < 1024 || serverPort >= 65536) {
	printf("Invalid port number for serverPort\n");
	goto out;
    }

    localPort = atoi(localPortString);
    if (localPort < 1024 || localPort >= 65536) {
	printf("Invalid port number for localPort\n");
	goto out;
    }

    /*** Bind local address to socket ***/
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == -1) {
	perror("Creating of local socket failed");
	goto out;
    }

    // Convert command line arguments to address info for local address
    lsaddr.sin_family = AF_INET;
    lsaddr.sin_port = htons(localPort);
    if (0 == inet_aton(localAddrString, &lsaddr.sin_addr)) {
	printf("Invalid localAddr: %s\n", localAddrString);
	goto err_sock;
    }

    ret = bind(fd, (struct sockaddr *)&lsaddr, sizeof(lsaddr));
    if (ret != 0) {
	perror("Bind to local address failed");
	goto err_sock;
    }

    /*** Connect to server ***/
    // Convert command line arguments to address info for remote server address
    ssaddr.sin_family = AF_INET;
    ssaddr.sin_port = htons(serverPort);
    if (0 == inet_aton(serverAddrString, &ssaddr.sin_addr)) {
	printf("Invalid serverAddr: %s\n", serverAddrString);
	goto err_sock;
    }

    ret = connect(fd, (struct sockaddr *)&ssaddr, sizeof(ssaddr));
    if (ret != 0) {
	perror("Connect to server failed");
	goto err_sock;
    }


    /*** send/recv tests ***/
    {
	char recvBuf[64] = { 0 };
	char *sendBuf = "World";
	ssize_t ret;

	// Recv message "Hello"
	ret = recv_data(fd, recvBuf, 5, 0);
	if (ret != 0)
	    goto err_sock;


	// Send message "World"
	ret = send(fd, sendBuf, 5, 0);	
	if (ret != 5)
	    goto err_sock;

	printf("received message %s\n", recvBuf);
    }

    while(1) { }

err_sock:
    if (fd != -1)
	close(fd);
out:
    return ret;
} 
