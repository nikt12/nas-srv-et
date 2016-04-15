#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <malloc.h>
#include <unistd.h>
#include "../hdr/netns-func.h"

#ifndef NETNS_RUN_DIR
#define NETNS_RUN_DIR "/var/run/netns"
#endif

#define CLONE_NEWNET 0x40000000

#define PART1_GLOBAL_NETNS_PATH "/proc/"
#define PART2_GLOBAL_NETNS_PATH "/ns/net"

int netns_change(char *ns_name, int fd_global_netns) {
/* If you need to change current netns to global netns:
 * set the first argument "NULL", the second argument
 * is file descriptor of global netns (it must be saved
 * before first call of this function)
 * If you need to change current netns to another netns:
 * set the first argument as netns which you need,
 * the second argument may be any
*/
	char netns_path[MAXPATHLEN];
	int fd_netns;

 /*  Join to global network namespace
  *  if (network namespace "ns_name" == NULL &&
  *  file descriptor of global network namespace
  *  "fd_global_netns" >-1)
  *  else exit function with error -1
  *  if (file descriptor of global network namespace
  *  "fd_global_netns" < 0)
 */
	if (ns_name == NULL) {
		if (setns(fd_global_netns, CLONE_NEWNET) == -1) {
			return -1;
		}
		return 0;
	}

/* Get path for network namespace "ns_name" */
	snprintf(netns_path, sizeof(netns_path), "%s/%s", NETNS_RUN_DIR, ns_name);

/* Create the base netns directory if it doesn't exist */
	mkdir(NETNS_RUN_DIR, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);

/* Create(or open) netns file */
	fd_netns = open(netns_path, O_RDONLY|O_EXCL, 0);
	if (fd_netns < 0) {
		return -2;
	}

/* Join to network namespace "ns_name" */
	if (setns(fd_netns, CLONE_NEWNET) < 0) {
		close(fd_netns);
		return -1;
	}

	return 0;
}

int* open_socket_in_netns(char **ns_names, int num_netns) {
	int fd_global_netns; /* File descriptor of global network namespace */
	int *fd_socket; /* File descriptors sockets that openning in netns ns_names[i] */
	int exit_stat; /* Exit status of function netns_change() */
	int i; /* Index of current network namespace */
	static char path1[MAXPATHLEN] = {0}; /* Path to global netns of this process */
        static char pid_current[30] = {0}; /* Pid the current process */

	printf("Started OSIN!\n");

/* Allocate memory for array fd_socket */
	fd_socket = (int*)malloc(sizeof(int)*num_netns);

/* Get path to the global network namespace of this process */
	strcat(path1,PART1_GLOBAL_NETNS_PATH);
	sprintf(pid_current, "%ld", (long)getpid());
	strcat(path1,pid_current);
	strcat(path1,PART2_GLOBAL_NETNS_PATH);

	printf("Path of global netns:%s\n", path1);

/* Save file descriptor of global network namespace */
	fd_global_netns = open(path1, O_RDONLY, 0);
	if (fd_global_netns < 0) {
		fprintf(stderr, "*** Cannot get file descriptor of global namespace file : %s\n",
				strerror(errno));
		return -1;
	}
	
	printf("FD of global netns:%d\n", fd_global_netns);

/* Set ns_name[i] as a network namespace, open socket */
	for(i = 0; i < num_netns; i++)
	{
/* Change netns to ns_names[i] */
		exit_stat = netns_change(ns_names[i], fd_global_netns);

		printf("ns_name[%d]:%s\n", i, ns_names[i]);

		if (exit_stat == -1) {
			fprintf(stderr, "*** Failed to create(or set) a new network namespace \"%s\": %s\n",
				ns_names[i], strerror(errno));
			return -1;
		}
		else if (exit_stat == -2) {
			fprintf(stderr, "*** Cannot create network namespace(\"%s\") file : %s\n",
				ns_names[i], strerror(errno));
			return -2;
		}

/* Open socket in netns ns_names[i] */
		fd_socket[i] = createServerSocket();
		if (fd_socket[i] < 0) {
			if(fd_socket[i] < 0)
				handleErr(fd_socket[i]);
			return -2;
		}
		sprintf(srvInfoTable[i].netns_name, "%s", ns_names[i]);
		srvInfoTable[i].netns_fd = fd_socket[i];

		printf("srvInfoTable[%d].netns_name:%s\n", i, ns_names[i]);
		printf("srvInfoTable[%d].netns_fd:%d\n", i, fd_socket[i]);
		printf("srvInfoTable[%d].srv_name:%s\n", i, srvInfoTable[i].srv_name);


	}

/* Return to global netns */
	netns_change(NULL, fd_global_netns);
	if (exit_stat == -1) {
		fprintf(stderr, "*** Failed to set a global network namespace : %s\n",
				strerror(errno));
		return -1;
	}

/* Close file descriptor of global network namespace */
	close(fd_global_netns);

/* Return array of file descriptors sockets which openning in netns nsnames */
	return fd_socket;
}
