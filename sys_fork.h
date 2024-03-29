struct tvi {
        u_int   tv_sec;
        u_int   tv_usec;
};

int sys_fork(int lport, int fid)
{
	int i, j, k, currhost; /* counting varibles */

	struct sockaddr_in my_addr;    /* my address information */
	struct sockaddr_in their_addr; /* connector's address information */
	int sin_size; /* size of connection in webserver */
	int numbytes, sockfd, new_fd; /* socket addressing */

	float uptime;

	char buf[MAXDATASIZE]; /* temp buffers for building output */
	char bufout[MAXDATASIZE]; /* temp buffers for building output */

	time_t nowp;			/* varible for current time */
	char timebuf[MAX_TIME_LEN];	/* buf for current time */

	int child, mstatus; /* varibles for shared memory */
	pid_t wait_status; /* varibles for shared memory */
	caddr_t mmap_ptr; /* last known status of each entry */
	/* this varible is in this shape n + 0 = status n + 10 
		= first char of rpc, max rpc len = 88 */

	/* timer variables */
	struct tvi tvi;
        struct timeval tv, tp;
        quad_t triptime = 0;

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
			MAX_HOSTS * MAX_SHARE_MEM_ELEMENT,/* How many bytes to mmap */
			PROT_READ | PROT_WRITE, /* Read and write permissions */
			MAP_SHARED,    /* Accessible by another process */
			fid,           /* which file is associated with mmap */
			(off_t) 0);    /* Offset in page frame */
		if (mmap_ptr == MAP_FAILED)
		{
			printf("Child Memory Map Failed\n");
			exit(ERROR_PIPE_MAP);
		}

		for(i = 0; i <= numhosts; i++)
		{
			/* clear each stats */
			mmap_ptr[i * MAX_SHARE_MEM_ELEMENT + 2] = 0; // set up UP time
			mmap_ptr[i * MAX_SHARE_MEM_ELEMENT + 6] = 0; // set up Down time
		}

		currhost = -1;
		/* This is the child process */
                while (1)
                {
			/* Main ping loop */

printf("%d / %d\n", currhost, numhosts);

			/* pick a host and ping it */
			if (currhost >= numhosts)
			{
				currhost = 0;
				sleep(DELAY_BETWEEN); /* make this 5 mins 1000 ms / 1 s */
			}
			else
			{
				currhost++;
			}

			for (i = 0; i < MAX_TIME_LEN; i++)
			{
				/* loop through the buff, and stick it */
				mmap_ptr[(currhost * MAX_SHARE_MEM_ELEMENT) + 10 + i] = 0;
			}

			if (ports[currhost] == 0) /* icmp probe */
			{
				//TODO
				continue;
			}
			else /* tcp probe */
			{
				printf("- tcpprobe: %s port: %d", ips[currhost], ports[currhost]);

				// clear timer
				(void)gettimeofday(&tp, (struct timezone *)NULL);
				if (tcpprobe(ips[currhost], ports[currhost]) == 1)
				{
				 	/* Can't connect to tcp host */
					/* check the dep */
					if (tcpprobe("www.google.ca", 80) == 1)
					{
						/* can't connect to dep, don't count this */
						continue;
					}

					/* count this in the status shared memory */
					mmap_ptr[currhost * MAX_SHARE_MEM_ELEMENT + 6]++;

					if (mmap_ptr[currhost * MAX_SHARE_MEM_ELEMENT] != DOWN_ID)
					{
						mmap_ptr[currhost * MAX_SHARE_MEM_ELEMENT] = DOWN_ID;
						numberdown[currhost] = 1; /* number of times down */
					}
					else /* it was down last time it was checked */
					{
						if (numberdown[currhost] < 2) /* NOTE: on third try it will email */
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
					/* count this in the status shared memory */
					mmap_ptr[currhost * MAX_SHARE_MEM_ELEMENT + 2]++;

					if (mmap_ptr[currhost * MAX_SHARE_MEM_ELEMENT] != DOWN_ID)
					{
						mmap_ptr[currhost * MAX_SHARE_MEM_ELEMENT] = UP_ID;
					}
					if (mmap_ptr[currhost * MAX_SHARE_MEM_ELEMENT] == DOWN_ID)
					{
						mmap_ptr[currhost * MAX_SHARE_MEM_ELEMENT] = UP_ID;
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
					(void)gettimeofday(&tv, (struct timezone *)NULL);
					timersub(&tv, &tp, &tv);
					triptime = (tv.tv_sec * 1000000) + tv.tv_usec;
					snprintf(timebuf, MAX_TIME_LEN, "%d.%03d ms\n", (int)(triptime / 1000), (int)(triptime % 1000));
					strncpy(&mmap_ptr[(currhost * MAX_SHARE_MEM_ELEMENT) + 10], timebuf, MAX_TIME_LEN);
			}
                }
		close(fid); /* close the mmap file */
	}

	/* This is the parent Process */
	sleep(1);
	/* allocate a shared memory region using mmap which can be inherited
		by child processes */
	mmap_ptr = mmap((caddr_t) 0,   /* Memory Location */
		MAX_HOSTS * MAX_SHARE_MEM_ELEMENT,/* How many bytes to mmap */
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
			//usleep(75); /*sleep for 50 ms to fake a GET */

			
			if(j == read(new_fd, buf, MAXDATASIZE) == -1)
			{
				perror("read");
			}

			buf[j-1] = 0; // null terminate string

			printf("%s\n\n", buf);

			if(write(new_fd, HTML_HEADER,strlen(HTML_HEADER)) == -1)
			{
				perror("send");
			}

			for (j = 0; j <= numhosts; j++)
			{
				bufout[0] = ' ';
				bufout[1] = 0;

				if (j % 6 == 0)
				{
					strlcat(bufout, "</tr><tr>", MAXDATASIZE);
				}

				switch (mmap_ptr[j * MAX_SHARE_MEM_ELEMENT])
                                {
                                        case UP_ID : strlcat(bufout, "<td bgcolor=#254117>", MAXDATASIZE);
                                                break;
                                        case DOWN_ID : strlcat(bufout, "<td bgcolor=#C11B17>", MAXDATASIZE);
                                                break;
                                        default : strlcat(bufout, "<td>", MAXDATASIZE);
                                }

				strlcat(bufout, "<table><tr><td>", MAXDATASIZE);
				//strlcat(bufout, names[j], MAXDATASIZE);
				strlcat(bufout, names[j], MAXDATASIZE);
				strlcat(bufout, "</td></tr><tr><td>", MAXDATASIZE);
				strlcat(bufout, ips[j], MAXDATASIZE);
				snprintf(bufout, MAXDATASIZE, "%s:%d", bufout, ports[j]);
				strlcat(bufout, "</td></tr><tr><td>", MAXDATASIZE);
				/* print out the date col */
				if (mmap_ptr[(j * MAX_SHARE_MEM_ELEMENT) + 10] == 0)
				{
					/* no date data for this host */
					strlcat(bufout, "n/a", MAXDATASIZE);
				}
				else
				{
					for (i = 0; i < MAX_RPC_LEN; i++)
					{
						if (mmap_ptr[(j * MAX_SHARE_MEM_ELEMENT) + 10 + i] == '|')
						{
							/* end of output */
							break;
						}
						snprintf(bufout, MAXDATASIZE, "%s%c", bufout, mmap_ptr[(j * MAX_SHARE_MEM_ELEMENT) + 10 + i]);
					}
				}
				strlcat(bufout, "</td></tr><tr><td>", MAXDATASIZE);

				/* calculate the % uptime */
				uptime = 100 - ((float)mmap_ptr[(j * MAX_SHARE_MEM_ELEMENT) + 6] / (float)mmap_ptr[(j * MAX_SHARE_MEM_ELEMENT) + 2] * 100.0);
				if ((uptime < 0) || (uptime > 100))
				{
					uptime = 0.00;
				}
				snprintf(bufout, MAXDATASIZE, "%s%f%", bufout, uptime);

				strlcat(bufout, "</td></tr></table></td>\n", MAXDATASIZE);
				if(write(new_fd, bufout, strlen(bufout)) == -1)
				{
					perror("send");
				}
			}

			/* grab current time so this event can be logged */
			(void)time(&nowp);
			(void)strftime(timebuf, sizeof(timebuf) - 1, "%c", localtime(&nowp));
			timebuf[sizeof(timebuf) - 1] = '\0';
			
			if(write(new_fd, timebuf, strlen(timebuf)) == -1)
			{
				perror("send");
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
