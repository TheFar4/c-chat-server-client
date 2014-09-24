#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/errno.h>

#define	QLEN 32	/* maximum connection queue length	*/
#define BUF_SIZE 1024 /* max buffer length */
#define TOT_SERV 100 /* max number of chat sessions */

extern int	errno;
int		errexit(const char *format, ...);
int		passivesock(const char *portnum, int qlen);
void  ss_start (char*, int, int, struct sockaddr *,int,int);
void  ss_find (char*, int, int, struct sockaddr *,int);

/* store session_server names */
char ss_names [TOT_SERV][25];

/* store session_server ports */
int ss_ports [TOT_SERV];

/* current number of session servers */
int cur_serv = 0;

/*------------------------------------------------------------------------
 * main - UDP server for chat service
 *------------------------------------------------------------------------
 */
 
int main(int argc, char* argv[]) {
    /* buffer to use for sending data */
    char buf[BUF_SIZE];
    
    /* Addresses of server and client */
    struct sockaddr_in self, other;
    
    /* length of the address struct */
    int len = sizeof(struct sockaddr_in);
    
    /* integers to store bytes received, socket descriptor and server's port number */
    int n, s, port;
    
    /* Usage ./session_server 8888 */
    if (argc < 2) {
	    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
	    return 1;
    }
    /* initialize socket */
    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
	    perror("socket");
	    return 1;
    }

    /* get port from string */
    port = atoi(argv[1]);
    
    /* set all bytes to 0 */
    memset((char *) &self, 0, sizeof(struct sockaddr_in));
    
    /* initialize server family, port and ip address for server */  
    self.sin_family = AF_INET;
    self.sin_port = htons(port);
    self.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY receives packets from all interfaces
    
    /* bind server to port */
    if (bind(s, (struct sockaddr *) &self, sizeof(self)) == -1) {
	    perror("bind");
	    return 1;
    }
    
    printf("Server started on port %d\n",port);
    bzero(buf,BUF_SIZE);
    /* receive data message of size n. Loop while n is positive */
    while ((n = recvfrom(s, buf, BUF_SIZE, 0, (struct sockaddr *) &other, &len)) != -1) {
        printf("%s\n",buf);
	    /* Look at message content and take appropriate action */  
        if (strncmp(buf,"Start",5)==0) {
            /* call ss_start to Start Chat Session */
            ss_start(buf,s,n,(struct sockaddr *) &other,len,port);
        }
        else if (strncmp(buf,"Find",4)==0) {
            /* call ss_join to reply client with session server address correspoding to that channel */
            ss_find(buf,s,n,(struct sockaddr *) &other,len);
        }
        else if (strncmp(buf,"Terminate",9)==0) {
            /* TODO - Add support for TERMINATE */
        }
        else {
            printf("Incorrect Format/Unsupported Method");
        }
        bzero(buf,BUF_SIZE);
    }

    close(s);
    return 0;
}

/*-----------------------------------------------------------------------------------
 * ss_start - Method that starts a new server if no other is found with the same name
 *-----------------------------------------------------------------------------------
 */
void  ss_start (char *buf, int s, int n, struct sockaddr *refer_other,int len,int serv_port) {
    char sname [25];
    bzero(sname,sizeof(sname));
    strncpy(sname, buf+6, n-6);
    int i;
    for(i=0;i<cur_serv;i++) {
        if (strcmp(sname,ss_names[i])== 0) {
            break;
        }
    }
    if ((i==cur_serv) && (cur_serv < TOT_SERV)){
        
        
        
        char servport [8];
        bzero(servport,sizeof(servport));
        snprintf(servport, sizeof(servport), "%d", serv_port);
        
        int sn = passivesock("80",QLEN);
        
        char sd [8];
        bzero(sd,sizeof(sd));
        snprintf(sd, sizeof(sd), "%d", sn);
        
        struct sockaddr_in sin;
        int socklen = sizeof(sin);
        if (getsockname(sn, (struct sockaddr *)&sin, &socklen) < 0) {
            errexit("getsockname: %s\n", strerror(errno));
        }
        
        char port [8];
        bzero(port,sizeof(port));
        snprintf(port, sizeof(port), "%d", ntohs(sin.sin_port));
        
        pid_t pid = fork();
        if (pid == -1) {
            // When fork() returns -1, an error happened.
            perror("Fork failed");
        }
        else if (pid == 0) {
            // When fork() returns 0, we are in the child process.
            execl("session_server","session_server",port,sd,servport,NULL);
            exit(1);
        }
        else {
            // When fork() returns a positive number, we are in the parent process
            // and the return value is the PID of the newly created child process.
            n = sendto(s, port, strlen(port)+1, 0, refer_other, len);
            if (n == -1) {
                errexit("Could not send to client");
            }
        }
        printf("Starting chat with name %s on port %s.\n",sname, port);
        
        strcpy(ss_names[cur_serv],sname);
        ss_ports[cur_serv]=(int) ntohs(sin.sin_port);
        cur_serv++;
    }
    else {
        char *message = "-1";
        sendto(s, message, sizeof(message), 0, refer_other, len);
    }
}
/*-----------------------------------------------------------------------------------
 * ss_find - Method that sends the adress of an existing chat server to the client  
 *-----------------------------------------------------------------------------------
 */
void  ss_find (char *buf, int s, int n, struct sockaddr *refer_other,int len) {
    printf("we are inside ssjoin\n");
    char sname [25];
    bzero(sname,sizeof(sname));
    strncpy(sname, buf+5, n-5);
    int i;
    for(i=0;i<cur_serv;i++) {
        if (strcmp(sname,ss_names[i])== 0) {
            break;
        }
    }
    if (i < cur_serv) {
        char port [8];
        bzero(port,sizeof(port));
        snprintf(port, sizeof(port), "%d", ss_ports[i]);
        n = sendto(s, port, strlen(port)+1, 0, refer_other, len);
        if (n == -1) {
            errexit("Could not send to client");
        }
    }
    else {
        char *message = "-1";
        n = sendto(s, message, sizeof(message), 0, refer_other, len);
        if (n == -1) {
            errexit("Could not send to client");
        }
    }
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
            //fprintf(stderr, "can't bind to %s port: %s; Trying other port\n",portnum, strerror(errno));
            sin.sin_port=htons(0); /* request a port number to be allocated
                                   by bind */
            if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
                errexit("can't bind: %s\n", strerror(errno));
            else {
                int socklen = sizeof(sin);

                if (getsockname(s, (struct sockaddr *)&sin, &socklen) < 0)
                        errexit("getsockname: %s\n", strerror(errno));
                //printf("New server port number is %d\n", ntohs(sin.sin_port));
            }
        }

        if (listen(s, qlen) < 0)
            errexit("can't listen on %s port: %s\n", portnum, strerror(errno));
        return s;
}
