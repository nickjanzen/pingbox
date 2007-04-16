int readconf(char *fname)
{
	/* open the configuration file and configure the server */

	FILE *fstream; /* file for config file */
	int i, j; /* temp counter */
	int fid, child, mstatus; /* varibles for shared memory */

        if ((fstream = fopen(fname, "r")) == NULL)
        {
                puts("Cannot Configuration file\n");
                exit(ERROR_NOTFOUND);
        }

        /* Create a new file for read/write access with permissions restricted
		to owner rwx access only */
        if ((fid = open("pingbox.pipe", O_RDWR | O_CREAT | O_TRUNC, (mode_t) 0700 )) < 0)
        {
                printf("Error opening the pipe file\n");
                exit(ERROR_PIPE_OPEN);
        }

        /* make the file the buffer size */
        if ((mstatus = ftruncate(fid, MAX_HOSTS * MAX_SHARE_MEM_ELEMENT)) == 1)
        {
                printf("Could Not allocate space on pipe file\n");  
                exit(ERROR_PIPE_SIZE);
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

                i++; /* i = 1 more than we want */
        }
        numhosts = i - 1; /* because i = 1 more than we want */
        fclose(fstream);

	/* initalize the Hosts */
        for (i = 0; i < MAX_HOSTS; i++)
        {
                numberdown[i] = 0;
                numberdown2[i] = 0;
        }

        /* debug - print hosts table */
        printf("NAME IP             PORTS OPTIONS  DEPEND  NOTIFY\n");
        for (j = 0; j <= numhosts; j++)
        {
                printf("%s %s %d %d %s %s\n",
                        names[j], ips[j], ports[j], options[j], depend[j], notify[j]);
        }
        /* end debug */


	return fid;
}
