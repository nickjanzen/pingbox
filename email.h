#define MAX_EMAIL_LEN 2000

/***************************************************************
* email() takes a int type, a string name, and email addresses *
* No need to modify unless /usr/bin/mail is elsewhere          *
****************************************************************/
int email(int type, char *name, char *addr)
{
        char msgf[MAX_EMAIL_LEN]; /* varible for building the email */

        if (type == DOWN_ID)
        {
                snprintf(msgf, MAX_EMAIL_LEN, "/bin/echo '\n' | /usr/bin/mail -s 'PB: Down -> %s' %s \n", name, addr);
        }
        else if (type == UP_ID)
        {
                snprintf(msgf, MAX_EMAIL_LEN, "/bin/echo '\n' | /usr/bin/mail -s 'PB: Alive -> %s' %s \n", name, addr);
        }

        system(msgf);

        return 0;
}

int email1(int type, char *name, char *addr)
{
	const char *hostname = "127.0.0.1";
        const char *servername = "HELO pingbox.secure5.net\n";

	char msgf[2000]; /* varible for building the email */

	int sockfd, code = 0;
	struct hostent *he;
	struct sockaddr_in remote;

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
	remote.sin_port = htons(25);
	remote.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(remote.sin_zero), 8);

	if (connect(sockfd, (struct sockaddr *)&remote, sizeof(struct sockaddr)) == -1)
	{
		code = 1;
	}

        if (type == DOWN_ID)
        {
                snprintf(msgf, MAX_EMAIL_LEN, "Subject: PB: Down -> %s \n", name);
        }
        else if (type == UP_ID)
        {
                snprintf(msgf, MAX_EMAIL_LEN, "Subject: PB: Alive -> %s \n", name);
        }


	if (send(sockfd, servername, strlen(servername), 0) == -1)
	{
		perror("send");
		exit(1);
	}

	if (send(sockfd, "MAIL FROM: ", strlen("MAIL FROM: "), 0) == -1)
	{
		perror("send");
		exit(1);
	}

	if (send(sockfd, EMAIL_FROM, strlen(EMAIL_FROM), 0) == -1)
	{
		perror("send");
		exit(1);
	}

	if(send(sockfd, "\nRCPT TO: ", strlen("\nRCPT TO: "), 0) == -1)
	{
		perror("send");
		exit(1);
	}

	if(send(sockfd, addr, strlen(addr), 0) == -1)
	{
		perror("send");
		exit(1);
	}

	if(send(sockfd, "\nDATA\n", strlen("\nDATA\n"), 0) == -1)
	{
		perror("send");
		exit(1);
	}

	if(send(sockfd, msgf, strlen(msgf), 0) == -1)
	{
		perror("send");
		exit(1);
	}

	if(send(sockfd, "\n.\n", strlen("\n.\n"), 0) == -1)
	{
		perror("send");
		exit(1);
	}

	close(sockfd);

        return 0;
}

