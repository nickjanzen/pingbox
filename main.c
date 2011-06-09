/****************************************************
 * version 2.1 - Date modified: Oct 10, 2002       *
 * pingbox - monitoring software for unix os's      *
 *      1.0 and on is persistant monitoring         *
 *         *** Multiple host version ***            *
 *        *** Single ping process version ***       *
 *          *** RPC version (disabled) ***          *
 *                                                  *
 * developed by Nicholas Janzen (nj@third-net.com)  *
 * Tested on OpenBSD 2.6/2.7/2.8/2.9/3.0/3.1/3.2    *
 ****************************************************/

/* User Configurable Options */

#define DELAY_BETWEEN 900 /* time between each ping */
#define SMTP_SERVER "127.0.0.1"
#define RPC_PORT 8234 /* TCP port that the rpc client runs on */
#define EMAIL_FROM "nj@telin.com"

#define HTML_HEADER "<html><head><title></title></head><body bgcolor=black text=white link=white vlink=white alink=while><table border=3><tr><td></td>\n"

/* Do not modify after this line */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#define MAX_HOSTS 220
#define MAX_IP_LEN 21
#define MAX_NAME_LEN 31
#define MAX_NOTIFY_LEN 128
#define MAX_RPC_LEN 88
#define MAX_TIME_LEN 64
#define MAX_SHARE_MEM_ELEMENT 1000

#define DISABLE_ID '0'
#define UP_ID '1'
#define DOWN_ID '2'
#define RECOVER_ID '3'

#define ERROR_NONE 0
#define ERROR_NOTFOUND 1
#define ERROR_INPUTENDED 2
#define ERROR_TOMANY 3
#define ERROR_SOCKET 4
#define ERROR_BIND 5
#define ERROR_LISTEN 6
#define ERROR_PIPE_OPEN 7
#define ERROR_PIPE_SIZE 8
#define ERROR_PIPE_MAP 9
#define ERROR_ARG 10

#define YES_EMAIL 1

#define BACKLOG 10     /* how many pending connections queue will hold */
#define MAXDATASIZE 1000 /* max number of bytes we can get at once */

char *rpcget(char *hostname, int port);

/* Global Varibles */

char rpc_buff[MAX_RPC_LEN]; /* global, ya i know!!!, for use with rpcget function */
int numhosts;
char names[MAX_HOSTS][MAX_NAME_LEN]; /* name of each host */
char ips[MAX_HOSTS][MAX_IP_LEN]; /* ip address or hostname of sys */
int ports[MAX_HOSTS]; /* port to monitor if 0 then icmp */
char depend[MAX_HOSTS][MAX_NAME_LEN]; /* what host does it depend on */
int options[MAX_HOSTS]; /* ones = notify by email */
char notify[MAX_HOSTS][MAX_NOTIFY_LEN]; /* email address to notify */
int numberdown[MAX_HOSTS]; /* number of times a host has been down */  
int numberdown2[MAX_HOSTS]; /* email the user after being down */

int numhosts;

/* end of Global Varibles */

/* include rest of the code*/

#include "readconf.h"
#include "tcpprobe.h"
#include "email.h"
#include "sys_fork.h"

/*int main()
{
	tcpprobe("209.115.237.66", 80);
}*/

/* Main function - */
int main(int argc, char **argv)
{
	/* main function of application */

	int lport; /* port to listen on */
	int fid; /* file descriptor for shared mem */

	printf("Pingbox Version 2.1 - by Nicholas Janzen (nj@third-net.com)\n\n");

	if (argc != 3) /* check for 1 arg, port number */
        {
                printf("Please invoke this program %s <port> <conf_file>\n", argv[0]);
                exit(ERROR_ARG);
        }

	lport = atoi(argv[1]); /* port to listen on */

	/* read in conf.txt - loading all of the data */
	fid = readconf(argv[2]);

	/* infininte lopp */
	sys_fork(lport, fid);

	/* code never gets here */

	return 0;
}


/***************************************************************
* rpcget() takes a string hostname and port                    *
* and returns NULL if it can't connect, or pointer to result   *
* No need to modify function.                                  *
****************************************************************/
char *rpcget(char *hostname, int port)
{
	strcpy(rpc_buff, "n/a");
	return rpc_buff;
}
char *rpcget1(char *hostname, int port)
{
        int sockfd, code = 0;
        struct hostent *he;     
        struct sockaddr_in remote;
	char buftmp[2];
	int i;

	rpc_buff[0] = NULL; /* a little safety */

	if ((he=gethostbyname(hostname)) == NULL)
	{
		perror("gethostbyname");
		return NULL;
	}            
	if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		return NULL;
	}
	remote.sin_family = PF_INET;
	remote.sin_port = htons(port);
	remote.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(remote.sin_zero), 8);                     

	if (connect(sockfd, (struct sockaddr *)&remote, sizeof(struct sockaddr)) == -1)
        {
		close(sockfd);
		return NULL;
        }

	for (i = 0; i < MAX_RPC_LEN; i++)
	{
		if(recv(sockfd, &buftmp, 1, 0) == -1)
		{
			perror("recv");
			return NULL;
		}
		if (buftmp[0] == '\n')
		{
			/* end of rpc call */
			break;
		}
		else
		{
			/* add char to the buffer */
			rpc_buff[i] = buftmp[0];
		}
	}
	rpc_buff[i + 1] = '|';
         
	close(sockfd);
	return rpc_buff;
}
