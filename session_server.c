
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <netdb.h>

#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define	QLEN 32	/* maximum connection queue length	*/
#define BUF_SIZE 1050 /* max buffer length */
#define MAX_CLIENT 100 /* max number of clients */

extern int	errno;
int		errexit(const char *format, ...);
int		passivesock(const char *portnum, int qlen);
int		handle(int fd, char *cname);

/*------------------------------------------------------------------------
 * main - TCP server for Chat Session service
 *------------------------------------------------------------------------
 */
int main(int argc, char *argv[])
{
	char	*portnum = "5050";	/* Standard server port number	*/
	char	*ccport = "8888";	/* Chat Coordinator port number	*/
	struct sockaddr_in fsin;	/* the from address of a client	*/
	int	msock = -1;			/* master server socket		*/
	fd_set	rfds;			/* read file descriptor set	*/
	fd_set	afds;			/* active file descriptor set	*/
	unsigned int	alen;		/* from-address length		*/
	int	fd, nfds;
	
	/* table to store adresses and ports of clients */
	char client_name [MAX_CLIENT][25];
	
	/* table to store file descriptors corresponding to names */
	int fd_table [MAX_CLIENT];
	
	/* total clients connected so far */
	int current_number = 0;
	
	switch (argc) {
	case	1:
		break;
	case	2:
		portnum = argv[1];
		break;
	case    3:
	    portnum = argv[1];
	    msock = atoi(argv[2]);
        break;
    case    4:
        portnum = argv[1];
	    msock = atoi(argv[2]);
	    ccport = argv[3];
	    break;
	default:
		errexit("Wrong Usage- %d arguments given\n", argc);
	}
    
    if (msock == -1) {
        msock = passivesock(portnum, QLEN);
    }
	

	nfds = getdtablesize();
	FD_ZERO(&afds);
	FD_SET(msock, &afds);
    struct timeval timeout;
    timeout.tv_sec = 60;
	timeout.tv_usec = 0;
	while (1) {
		memcpy(&rfds, &afds, sizeof(rfds));
        int readsocks = select(nfds, &rfds, (fd_set *)0, (fd_set *)0, &timeout);
		if (readsocks < 0)
			errexit("select: %s\n", strerror(errno));
		else if(readsocks == 0) {
		    /* Send Terminate to Chat Coordinator */
		    //TODO
		}
		if (FD_ISSET(msock, &rfds)) {
			int	ssock;

			alen = sizeof(fsin);
			ssock = accept(msock, (struct sockaddr *)&fsin, &alen);
			
			/* get clientname */
            char cname [25];
            char port [8];
            strcpy(cname,(char *)inet_ntoa(fsin.sin_addr));
            strcat(cname,":");
            snprintf(port, sizeof(port), "%d", ntohs(fsin.sin_port));
            strcat(cname,port);
            
            if (current_number >= MAX_CLIENT) {
                errexit("%s\n", "Max number of clients limit reached");
            }
            else {
                strcpy(client_name[current_number],cname);
                fd_table[current_number] = ssock;
                current_number++;
            }
            
			if (ssock < 0)
				errexit("accept: %s\n",
					strerror(errno));
			FD_SET(ssock, &afds);
		}
		for (fd=0; fd<nfds; ++fd) {
			if (fd != msock && FD_ISSET(fd, &rfds)){
			    int index =-1, i=0;
			    for(i=0;i<=current_number;i++) {
			        if (fd == fd_table[i]) {
			            index = i;
			            break;
			        }
			    }
			    if (index == -1) {
			        errexit("%s\n", "Client name not found");
			        break;
			    }
				if (handle(fd,client_name[index]) == 0) {
					(void) close(fd);
					FD_CLR(fd, &afds);
				}
			}
		}
	}
}

/*------------------------------------------------------------------------
 * Handle Messages
 *------------------------------------------------------------------------
 */
int handle(int ssock, char *cname)
{
    /* buffer to store data for communication */
    static char buf[BUF_SIZE];
    
	/*  table that stores all messages */
    static char message_table [BUF_SIZE][BUF_SIZE];
    
    /* current max index in table */
    static int total_messages = 0;
    
    /* table to store client names */
    static char client_info [MAX_CLIENT][25];
    
    /* table to store previous messages index sent to each client */
    static int previous_info [MAX_CLIENT];
    
    /* maximum number of clients connected */
    static int max_client = 0;
    
    bzero(buf, BUF_SIZE);
    int n = read(ssock, buf, BUF_SIZE);
    if (n < 0) {
        errexit("ERROR reading from socket");
    }
    if (strncmp(buf,"Submit",6)==0) {
        /* Submit request */
        
        /* message start index */
        int si = 9;
        if (buf[si] == ' ') si++;
         
        /* print the message received */
        printf("%s - %s\n", cname, buf+si);
        
        /* check if client is new */
        int index = -1;
        int i;
        for(i=0;i<max_client;i++) {
            if (strcmp(client_info[i],cname)==0) {
                index=i;
                break;
            }
        }
        
        /* If client is new, make an entry for it in the client_info & previous_info tables */
        if (index == -1) {
            if (max_client < MAX_CLIENT) {
                /* case when there is more space in table */
                strcpy(client_info[max_client],cname);
                previous_info[max_client] = 0;
                max_client++;
                index = 1;
                printf("Added new Client-%s\n",cname);
            }
        }
        if (index != -1) {
            /* Store the message */
            if (total_messages < BUF_SIZE) {
                char mes [BUF_SIZE];
                strcpy(message_table[total_messages],cname);
                strcat(message_table[total_messages]," : ");
                strcat(message_table[total_messages],buf+si);
                total_messages++;
            }
        }
    }
    else if (strncmp(buf,"GetNext",7)==0) {
        /* print the message received */
        /* check if client is new */
        int index = -1;
        int i;
        for(i=0;i<max_client;i++) {
            if (strcmp(client_info[i],cname)==0) {
                index=i;
                break;
            }
        }
        
        if ((index == -1) || (total_messages <= previous_info[index])){
            bzero(buf, BUF_SIZE);
            strcpy(buf,"-1");
            
            /* send message to server */
            n = write(ssock, buf, strlen(buf)+1);
            if (n < 0) {
                error("ERROR writing to socket");
            }
        }
        else {
            bzero(buf, BUF_SIZE);
            /* Insert message length */
            char message_length [3];
            snprintf(message_length, sizeof(message_length), "%d", strlen(message_table[previous_info[index]])+1);
            strcpy(buf,message_length);
            strcat(buf," ");
            strcat(buf,message_table[previous_info[index]]);
            previous_info[index]+=1;
            
            /* send to client */
            n = write(ssock, buf, strlen(buf)+1);
            if (n < 0) {
                error("ERROR writing to socket");
            }
        }
    }
    else if (strncmp(buf,"GetAll",6)==0) {
        /* check if client is new */
        int index = -1;
        int i;
        for(i=0;i<max_client;i++) {
            if (strcmp(client_info[i],cname)==0) {
                index=i;
                break;
            }
        }
        int tot =0;
        /* received a GetAll command */
        bzero(buf, BUF_SIZE);
        if (index == -1) {
            strcpy(buf,"0");
        }
        else {
            tot = total_messages - previous_info[index];
            /* Send Total number of messages */
            snprintf(buf, sizeof(buf), "%d", tot);
        }
        /* send to client */
        n = write(ssock, buf, strlen(buf)+1);
        if (n < 0) {
            error("ERROR writing to socket");
        }
        
        /* send all messages one by one now */
        for(i=0;i<tot;i++) {
            sleep(1);
            bzero(buf, BUF_SIZE);
            /* Insert message length */
            char message_length [3];
            snprintf(message_length, sizeof(message_length), "%d", strlen(message_table[previous_info[index]])+1);
            strcpy(buf,message_length);
            strcat(buf," ");
            strcat(buf,message_table[previous_info[index]]);
            previous_info[index]+=1;
            n = write(ssock, buf, strlen(buf)+1);
            if (n < 0) {
                error("ERROR writing to socket");
            }
            printf("sending- %s\n",buf);
        }
    }
    else if (strncmp(buf,"Leave",5)==0) {
        return 0;
    }
    else {
            printf("Incorrect Format/Unsupported Method\n");
    }
    return 1;
}

/*------------------------------------------------------------------------
 * errexit - print an error message and exit
 * IMPORTANT - This method has not been implemented by me.
 * This method has been taken from echoClient.c that was given as a sample
 * to students studying "Network Systems" course taught by Prof. S. Mishra
 * at University of Colorado Boulder Fall 2014
 *------------------------------------------------------------------------
 */
int errexit(const char *format, ...)
{
        va_list args;

        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        exit(1);
}

/*------------------------------------------------------------------------
 * passivesock - allocate & bind a server socket using TCP
 * IMPORTANT - This method has not been implemented by me.
 * This method has been taken from echoClient.c that was given as a sample
 * to students studying "Network Systems" course taught by Prof. S. Mishra
 * at University of Colorado Boulder Fall 2014
 *------------------------------------------------------------------------
 */
int passivesock(const char *portnum, int qlen)
/*
 * Arguments:
 *      portnum   - port number of the server
 *      qlen      - maximum server request queue length
 */
{
        struct sockaddr_in sin; /* an Internet endpoint address  */
        int     s;              /* socket descriptor             */

        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = INADDR_ANY;

    /* Map port number (char string) to port number (int) */
        if ((sin.sin_port=htons((unsigned short)atoi(portnum))) == 0)
                errexit("can't get \"%s\" port number\n", portnum);

    /* Allocate a socket */
        s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s < 0)
            errexit("can't create socket: %s\n", strerror(errno));

    /* Bind the socket */
        if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
            fprintf(stderr, "can't bind to %s port: %s; Trying other port\n",
                portnum, strerror(errno));
            sin.sin_port=htons(0); /* request a port number to be allocated
                                   by bind */
            if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
                errexit("can't bind: %s\n", strerror(errno));
            else {
                int socklen = sizeof(sin);

                if (getsockname(s, (struct sockaddr *)&sin, &socklen) < 0)
                        errexit("getsockname: %s\n", strerror(errno));
                printf("New server port number is %d\n", ntohs(sin.sin_port));
            }
        }

        if (listen(s, qlen) < 0)
            errexit("can't listen on %s port: %s\n", portnum, strerror(errno));
        return s;
}

