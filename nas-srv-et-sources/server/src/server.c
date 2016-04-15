#include "../hdr/server-func.h"

int endEventLoop, endMainLoop, epollFD, readyFDs;

config servConfig;

transport transp_proto;

struct epoll_event event;
struct epoll_event evList[MAX_EPOLL_EVENTS];

struct sockaddr_in serverAddr;
socklen_t serverAddrSize;

service_info srvInfoTable[MAX_NUM_OF_SERVICES];
int num_of_services;

void eventLoop(connection *connList, int *pFD_SockArray, int netns_num);

int main(int argc, char *argv[]) {
	struct sigaction sa;
	sigset_t newset;
	char **netns_names;
	int netns_num;
	int *fd_socket;

	openlog("NAS-server_emulator", LOG_PID | LOG_CONS, LOG_DAEMON);
	
	if((argc == 2) && (strcmp(argv[1], "-d") == 0)) {
		printf("Running NAS-server_emulator in daemon mode...\n");
		int fd;
		struct rlimit flim;
		if (getppid() != 1){
			if(fork() != 0) exit(0);
			setsid();
		}

		getrlimit(RLIMIT_NOFILE, &flim);
		for(fd = 0; fd < flim.rlim_max; fd++)
			close(fd);

		syslog(LOG_INFO, "Daemon has started successfully!");
	}
	sigemptyset(&newset);
	sigprocmask(SIG_BLOCK, &newset, 0);
	sa.sa_handler = sig_handler;
	sigaction(SIGINT, &sa, 0);
	sigaction(SIGHUP, &sa, 0);

	do {
		endEventLoop = 0;
		int result, i;
		connection connList[NUM_OF_CONNECTIONS];
		config_t cfg;

		memset(&connList, 0, sizeof(connList));

		config_init(&cfg);

		result = readConfigFile(&cfg);
		if(result < 0)
			handleErr(result);

		result = checkArgs();
		if(result < 0)
			handleErr(result);

		// netns_num = num_of_services;
		// netns_names = (char **)malloc(netns_num * sizeof(char *));
		// for(i = 0; i < netns_num; i++) {
		// 	netns_names[i] = (char *)malloc(5);
		// 	sprintf(netns_names[i], "ns%d", (netns_num - i));
		// }

		netns_num = num_of_services;
                netns_names = (char **)malloc(netns_num * sizeof(char *));
                printf("Netns_num:%d\n", netns_num);
                for(i = 0; i < netns_num; i++) {
                        netns_names[i] = (char *)malloc(5);
                        char a;
                        a = srvInfoTable[netns_num - i - 1].srv_name[1];
                        int b;
                        b = atoi(&a);
                    printf("Service int:%d\n", b);
                        sprintf(netns_names[i], "ns%d", b);
                        printf("NS name:%s\n", netns_names[i]);

                }


		fd_socket = open_socket_in_netns(netns_names, netns_num);

		if(transp_proto == TCP)
			printf("Waiting for connections... Listening on port %d with queue length %d...\n", servConfig.port, servConfig.qlen);
		else
			printf("Using UDP protocol. Waiting for connections on port %d with queue length %d...\n", servConfig.port, servConfig.qlen);

		eventLoop(connList, fd_socket, netns_num);

		for(i = 0; i < NUM_OF_CONNECTIONS; i++)
			if(connList[i].clientNickName[0] != '\0') {
				if(write(connList[i].clientSockFD, SRV_IS_OFFLINE_NOTIF, strlen(SRV_IS_OFFLINE_NOTIF)) < 0)
					return SEND_ERR;
				close(connList[i].clientSockFD);
			}
		config_destroy(&cfg);
	}
	while(!endMainLoop);

	closelog();

	return 0;
}

void eventLoop(connection *connList, int *pFD_SockArray, int netns_num) {
	int i, j, result;

	epollFD = epoll_create(EPOLL_QUEUE_LEN);
	for(i = 0; i < netns_num; i++) {
		event.data.fd = pFD_SockArray[i];
		event.events = EPOLLIN;
		epoll_ctl(epollFD, EPOLL_CTL_ADD, pFD_SockArray[i], &event);
	}

	while(!endEventLoop) {
		readyFDs = epoll_wait(epollFD, evList, MAX_EPOLL_EVENTS, EPOLL_RUN_TIMEOUT);

		if(transp_proto == TCP)
			timeoutCheck(connList);

		for(i = 0; i < readyFDs; i++) { // add EPOLLHUP, EPOLLERR
			switch(transp_proto) {
			case TCP:
				for(j = 0; j < netns_num; j++) {
					if (evList[i].data.fd == pFD_SockArray[j]) {
						while(1) {
							result = acceptNewConnection(pFD_SockArray[j], connList, &evList[i]);
							if(result < 0)
								handleErr(result);
							break;
						}
					break;
					}
				}
				if(j != netns_num)
					continue;
				while(1) {
					result = dataExchangeWithClient(0, connList, &evList[i]);
					if(result < 0)
						handleErr(result);
					break;
				}
				break;
			case UDP:
				for(j = 0; j < netns_num; j++) {
					if (evList[i].data.fd == pFD_SockArray[j]) {
						while(1) {
							result = dataExchangeWithClient(pFD_SockArray[j], connList, &evList[i]);
							if(result < 0)
								handleErr(result);
							break;
						}
					}
				}
				break;
			}
		}
	}
}
