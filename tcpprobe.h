static void connect_alarm(int signo)
{
	return;
}

/* 0 and 1 ok, but returns 2 means there is an error */
int sendrecvcmd(int sockfd, char *fname)
{
	int state;
	char s[1024];
	char s2[1024 * 128];
	FILE *fstream;

	if ((fstream = fopen(fname, "r")) == NULL)
	{
		/* no string data avail */
		return 1;
	}

	state = 0;

	while (feof(fstream) == 0)
	{
		fgets(s, 1023, fstream);

		if (state == 0)
		{
			/* send */

			if(send(sockfd, s, strlen(s), 0) == -1)
				perror("send");

			if (s[0] == '\n')
			{
				state = 1;
			}
		}
		else if (state == 1)
		{
			/* start recv */

			if(send(sockfd, "\n", 1, 0) == -1)
				perror("send");

			break;
		}
	}

	usleep(10);

	strcpy(s2, "");

	while(strlen(s2) < (1024 * 128) && (state < 6))
	{
		state = 7;
		if (recv(sockfd, s2, (1024 * 128), 0) == -1)
		{
			/* done receiving */
			break;
		}
		break;
	}

	//printf("\n\n\nSTART%sEND\n\n\n\n", s);
	//printf("START%sEND\n", s2);

	s[strlen(s) - 1] = NULL;

	if (strstr(s2, s) == NULL)
	{
		/* Can't find string */
		return 2;
	}

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
	int optlen;

	//sigfunc *sigfunc;

	signal(SIGALRM, connect_alarm);
	if (alarm(3) != 0)
		printf("problem 3563\n");

	if ((he=gethostbyname(hostname)) == NULL)
	{
		perror("gethostbyname");
		return 1;
	}

	if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket");
		return 1;
	}

	remote.sin_family = PF_INET;
	remote.sin_port = htons(port);
	remote.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(remote.sin_zero), 8);
	if (connect(sockfd, (struct sockaddr *)&remote, sizeof(struct sockaddr)) == -1)
        {
		close(sockfd);
		code = 1;
        }

	/* check to see if we have a string to watch for - and command to send */
	if (sendrecvcmd(sockfd, hostname) == 2)
	{
		//printf("JDSFDFDLHFJD\n");
		return 1;
	}

	alarm(0);

	//signal(SIGALRM, sigfunc);

	close(sockfd);

	return code;
}

