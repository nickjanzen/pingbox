/****************************************************
 * version 2.0 - Date modified: Nov 13, 2001        *
 * pingbox - monitoring software for unix os's      *
 *      1.0 and on is persistant monitoring         *
 *         *** Multiple host version ***            *
 *        *** Single ping process version ***       *
 *               *** RPC version ***                *
 * developed by Nicholas Janzen (nj@third-net.com)  *
 * Tested on OpenBSD 2.6/2.7/2.8/2.9                *
 ****************************************************/

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
#include <time.h>
#include <unistd.h>

#define DELAY_BETWEEN 9000000 /* time between each ping 9sec */

#define MAX_HOSTS 60
#define MAX_IP_LEN 21
#define MAX_NAME_LEN 31
#define MAX_NOTIFY_LEN 128
#define MAX_RPC_LEN 88

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

#define RPC_PORT 8234 /* TCP port that the rpc client runs on */

#define HTML_HEADER "<html><head><title>Status</title></head><body><table border=1><tr><td>Name</td><td>IP address</td><td>port</td><td>status</td><td>RPC information</td></tr>\n"
#define HTML_FOOTER "</table></body>\n"

int tcpprobe(char *hostname, int port);
char *rpcget(char *hostname, int port);

char rpc_buff[MAX_RPC_LEN]; /* global, ya i know!!!, for use with rpcget function */

/* used for testing */
/*int main() 
{
	rpcget("209.115.237.66", 8234);
	return 0;
}*/

int main(int argc, char **argv)
{
	FILE *fstream; /* file for config file */
	char names[MAX_HOSTS][MAX_NAME_LEN]; /* name of each host */
	char ips[MAX_HOSTS][MAX_IP_LEN]; /* ip address or hostname of sys */
	int ports[MAX_HOSTS]; /* port to monitor if 0 then icmp */
	char depend[MAX_HOSTS][MAX_NAME_LEN]; /* what host does it depend on */
	int options[MAX_HOSTS]; /* ones = notify by email */
	char notify[MAX_HOSTS][MAX_NOTIFY_LEN]; /* email address to notify */
	int numberdown[MAX_HOSTS]; /* number of times a host has been down */
	int numberdown2[MAX_HOSTS]; /* email the user after being down */
	int i, j, k, numhosts, currhost; /* counting varibles */

	struct sockaddr_in my_addr;    /* my address information */
	struct sockaddr_in their_addr; /* connector's address information */
	int sin_size; /* size of connection in webserver */
	int numbytes, sockfd, new_fd; /* socket addressing */
	int lport; /* port to listen on */

	char buf[MAXDATASIZE]; /* temp buffers for building output */
	char bufout[MAXDATASIZE]; /* temp buffers for building output */

	time_t now;
	struct tm tp;

	int fid, child, mstatus; /* varibles for shared memory */
	pid_t wait_status; /* varibles for shared memory */
	caddr_t mmap_ptr; /* last known status of each entry */
	/* this varible is in this shape n + 0 = status n + 10 
		= first char of rpc, max rpc len = 88 */

	printf("Pingbox Version 1.7 - by Nicholas Janzen (nj@third-net.com)\n\n");
	if (argc != 2) /* check for 1 arg, port number */
	{
		printf("Please invoke this program %s <port>\n", argv[0]);
		exit(ERROR_ARG);
	}

	lport = atoi(argv[1]); /* port to listen on */

	/* open the configuration file */
	if ((fstream = fopen("conf.txt", "r")) == NULL)
	{
		puts("Cannot Configuration file\n");
		exit(ERROR_NOTFOUND);
	}

	/* Create a new file for read/write access with permissions restricted
		to owner rwx access only */
	if ((fid = open("pingbox.pipe", O_RDWR | O_CREAT | O_TRUNC, (mode_t) 0755 )) < 0)
	{
		printf("Error opening the pipe file\n");
		exit(ERROR_PIPE_OPEN);
	}

	/* make the file the buffer size */
	if ((mstatus = ftruncate(fid, MAX_HOSTS * 100)) == 1)
	{
		printf("Could Not allocate space on pipe file\n");
		exit(ERROR_PIPE_SIZE);
	}

	for (i = 0; i < MAX_HOSTS; i++)
	{
		numberdown[i] = 0;
		numberdown2[i] = 0;
	}

	i = 0; /* set counter for file input */
	/* parse the configuration file */
	while (feof(fstream) == 0)
	{
		/* make sure we haven't gone over the limit */
		if (i >= MAX_HOSTS)
		{
			puts("To many hosts found\n");
			exit(ERROR_TOMANY);
		}
		/* 1 = name, 2 = ip, 3 = options, 4 = depend, 5 = notify */
		fscanf(fstream, "%s%s%d %d %s%s\n", &names[i], &ips[i],
			&ports[i], &options[i], &depend[i], &notify[i]);
		//{
			/* file ended too quick */
		//	puts("input of conf file ended too quick\n");
			//exit(ERROR_INPUTENDED);
		//}
		//notify[i] = NULL; /* set the end of string for the email addresses */
		i++; /* i = 1 more than we want */
	}
	numhosts = i - 1; /* because i = 1 more than we want */
	fclose(fstream);

	/* debug - print hosts table */
	printf("NAME IP             PORTS OPTIONS  DEPEND  NOTIFY\n");
	for (j = 0; j <= numhosts; j++)
	{
		printf("%s %s %d %d %s %s\n",
			names[j], ips[j], ports[j], options[j], depend[j], notify[j]);
	}
	/* end debug */

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		exit(ERROR_SOCKET);
	}

	my_addr.sin_family = AF_INET;         /* host byte order */
	my_addr.sin_port = htons(lport);     /* short, network byte order */
	my_addr.sin_addr.s_addr = INADDR_ANY; /* automatically fill with my IP */
	bzero(&(my_addr.sin_zero), 8);        /* zero the rest of the struct */

	if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1)
	{
		perror("bind");
		exit(ERROR_BIND);
	}

	if (listen(sockfd, BACKLOG) == -1)
	{
		perror("listen");
		exit(ERROR_LISTEN);
	}

	/* Start the pinging process */
        if (!fork())
        {
		/* allocate a shared memory region using mmap which can
			be inherited by child processes */
		mmap_ptr = mmap((caddr_t) 0,   /* Memory Location */
			MAX_HOSTS * 100,/* How many bytes to mmap */
			PROT_READ | PROT_WRITE, /* Read and write permissions */
			MAP_SHARED,    /* Accessible by another process */
			fid,           /* which file is associated with mmap */
			(off_t) 0);    /* Offset in page frame */
		if (mmap_ptr == MAP_FAILED)
		{
			printf("Child Memory Map Failed\n");
			exit(ERROR_PIPE_MAP);
		}

		currhost = -1;
		/* This is the child process */
                while (1)
                {
			if (currhost >= numhosts)
			{
				currhost = 0;
				usleep(DELAY_BETWEEN); /* make this 5 mins 1000 ms / 1 s */
			}
			else
			{
				currhost++;
			}
			/* Main ping loop */
			/* pick a host and ping it */
			if (ports[currhost] == 0) /* icmp probe */
			{
				continue;
			}
			else /* tcp probe */
			{
				/*tp = (void)localtime(now); */
				printf("- tcpprobe: %s port: %d", ips[currhost], ports[currhost]);
				if (tcpprobe(ips[currhost], ports[currhost]) == 1)
				{
				 	/* Can't connect to tcp host */
					/* check the dep */
					if (tcpprobe("www.apple.com", 80) == 1)
					{
						continue;
					}
					if (mmap_ptr[currhost * 100] != DOWN_ID)
					{
						mmap_ptr[currhost * 100] = DOWN_ID;
						numberdown[currhost] = 1; /* number of times down */
					}
					else /* it was down last time it was checked */
					{
						if (numberdown[currhost] != 3)
						{
							/* don't do anything yet */
							numberdown[currhost]++;
						}
						else
						{
							/* send an email informing user system is down */
							if ((options[currhost] == YES_EMAIL) && (numberdown2[currhost] == 0))
							{
								printf("\nnumberdown[%d] = %d\n", currhost, numberdown[currhost]);
								email(DOWN_ID, names[currhost], notify[currhost]);
								printf("Down - Email Sent\n");
								numberdown2[currhost] = 2; /* don't send anymore mail */
							}
						}
					}
					printf(" DOWN\n");
				}
				else
				{
					/* Can connect to tcp host */
					if (rpcget(ips[currhost], RPC_PORT) == NULL)
					{
						/* no rpc data avail */
						mmap_ptr[(currhost * 100) + 10] = 0;
					}
					else
					{
						/* we got some rpc data */
						for (i = 0; i < MAX_RPC_LEN; i++)
						{
							/* loop through rpc data, and stick it */
							mmap_ptr[(currhost * 100) + 10 + i] = rpc_buff[i];
							if (rpc_buff[i] == '|')
							{
								break;
							}
						}
					}
					if (mmap_ptr[currhost * 100] != DOWN_ID)
					{
						mmap_ptr[currhost * 100] = UP_ID;
					}
					if (mmap_ptr[currhost * 100] == DOWN_ID)
					{
						mmap_ptr[currhost * 100] = UP_ID;
						if ((options[currhost] == YES_EMAIL) && (numberdown2[currhost] == 2))
						{
							email(UP_ID, names[currhost], notify[currhost]);
							printf("Up - Email Sent\n");
						}
					}
					numberdown[currhost] = 0;
					numberdown2[currhost] = 0;
					printf(" UP\n");
				}
			}
                }
		close(fid); /* close the mmap file */
	}

	/* This is the parent Process */
	sleep(1);
	/* allocate a shared memory region using mmap which can be inherited
		by child processes */
	mmap_ptr = mmap((caddr_t) 0,   /* Memory Location */
		MAX_HOSTS * 100,/* How many bytes to mmap */
		PROT_READ | PROT_WRITE, /* Read and write permissions */
		MAP_SHARED,    /* Accessible by another process */
		fid,           /* which file is associated with mmap */
		(off_t) 0);    /* Offset in page frame */
	if (mmap_ptr == MAP_FAILED)
	{
		printf("Parent Memory Map Failed\n");
                	exit(ERROR_PIPE_MAP);
	}

	/* Main program loop */
	while(1)
	{
		sin_size = sizeof(struct sockaddr_in);
		if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1)
		{
			perror("accept");
			continue;
		}

		printf("server: got connection from %s\n",inet_ntoa(their_addr.sin_addr));
		/* Catch client connections */
		if (!fork())
		{
			/* this is the child process */
			usleep(50); /*sleep for 50 ms to fake a GET */
			if(write(new_fd, HTML_HEADER,strlen(HTML_HEADER)) == -1)
			{
				perror("send");
			}

			for (j = 0; j <= numhosts; j++)
			{
				bufout[0] = NULL;
				/*sprintf(bufout, "<tr><td>%s</td><td>%s</td>
					<td>%s</td><td>good</td></tr>\n",
					names[j], ips[j], ports[j]); */
				strcat(bufout, "<tr><td>");
				strcat(bufout, names[j]);
				strcat(bufout, "</td><td>");
				strcat(bufout, ips[j]);
				strcat(bufout, "</td><td>");
				sprintf(bufout, "%s%d", bufout, ports[j]);
				strcat(bufout, "</td>");
				switch (mmap_ptr[j * 100])
				{
					case UP_ID : strcat(bufout, "<td bgcolor=#00FF00>ALIVE");
						break;
					case DOWN_ID : strcat(bufout, "<td bgcolor=#FF0000>DOWN");
						break;
					default : strcat(bufout, "<td>UNKNOWN");
				}
				strcat(bufout, "</td><td>");
				/* print out the rpc col */
				if (mmap_ptr[(j * 100) + 10] == 0)
				{
					/* no rpc data for this host */
					strcat(bufout, "n/a");
				}
				else
				{
					for (i = 0; i < MAX_RPC_LEN; i++)
					{
						if (mmap_ptr[(j * 100) + 10 + i] == '|')
						{
							/* end of output */
							break;
						}
						sprintf(bufout, "%s%c", bufout, mmap_ptr[(j * 100) + 10 + i]);
					}
				}
				strcat(bufout, "</td></tr>");
				if(write(new_fd, bufout, strlen(bufout)) == -1)
				{
					perror("send");
				}
			}
			
			if(write(new_fd, HTML_FOOTER,strlen(HTML_FOOTER)) == -1)
			{
				perror("send");
			}
			close(new_fd);
			exit(ERROR_NONE);
		}
		close(new_fd);  /* parent doesn't need this */

		while(waitpid(-1,NULL,WNOHANG) > 0); /* clean up all child processes */
	}
	close(sockfd);
	munmap(mmap_ptr, MAX_HOSTS);
	close(fid); /* close the mmap file */
	unlink("pingbox.pipe"); /* unlink (i.e. remove) the mmap file */
        return ERROR_NONE;
}

/***************************************************************
* email() takes a int type, a string name, and email addresses *
* No need to modify unless /usr/bin/mail is elsewhere          *
****************************************************************/
int email(int type, char *name, char *addr)
{
	char msgf[2000]; /* varible for building the email */

	if (type == DOWN_ID)
	{
		sprintf(msgf, "/bin/echo 'Down\n' | /usr/bin/mail -s 'Pingbox: %s
			-> Down' %s \n", name, addr);
	}
	else if (type == UP_ID)
	{
		sprintf(msgf, "/bin/echo 'Alive\n' | /usr/bin/mail -s 'Pingbox: %s
			-> Alive' %s \n", name, addr);
	}

	system(msgf);

	return 0;
}

/***************************************************************
* tcpProbe() takes a string hostname and port                  *
* and returns 1 if it can't connect, 0 if it can (reverse)     *
* No need to modify function.                                  *
****************************************************************/
int tcpprobe(char *hostname, int port)
{
	int sockfd, code = 0;
	struct hostent *he;
	struct sockaddr_in remote;

    if ((he=gethostbyname(hostname)) == NULL) {
        perror("gethostbyname");
	return 1;
    }
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return 1;
    }
    remote.sin_family = PF_INET;
    remote.sin_port = htons(port);
    remote.sin_addr = *((struct in_addr *)he->h_addr);
    bzero(&(remote.sin_zero), 8);
    if (connect(sockfd, (struct sockaddr *)&remote, sizeof(struct sockaddr)) == -1)
	{
        code = 1;
	}

    close(sockfd);
    return code;
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
