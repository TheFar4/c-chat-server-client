#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/errno.h>

#define BUF_SIZE 1024

extern int	errno;

int	errexit(const char *format, ...);
void talk_to_ss(char*, char*);
int cc_start (int, int, int, char*, struct sockaddr *, struct sockaddr_in server);
int cc_join (int, int, int, char*, struct sockaddr *, struct sockaddr_in server);
int	connectsock(const char*, const char*);

/*------------------------------------------------------------------------
 * main - Client for chat service
 *------------------------------------------------------------------------
 */
int main(int argc, char *argv[]) {
    /* Struct to store address and port of server */
    struct sockaddr_in server;
    
    /* Length contains size of sockaddr_in in bytes */ 
    int len = sizeof(struct sockaddr_in);
    
    /* buffer to store data for communication */
    char buf[BUF_SIZE];
    
    /* struct to store hostname */
    struct hostent *host;
    
    /* integers to store length of data received, socket descriptor and port number of server */ 
    int n, s, port;

    /* usage  "./chat_client localhost 8888" */
    if (argc < 3) {
	    fprintf(stderr, "Usage: %s <host> <port> \n", argv[0]);
	    return 1;
    }
    
    /* fill hostent struct */
    host = gethostbyname(argv[1]);
    if (host == NULL) {
	    perror("gethostbyname");
	    return 1;
    }

    /* convert portnum taken from command line as srting to integer */  
    port = atoi(argv[2]);

    /* initialize socket */
    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
	    perror("socket");
	    return 1;
    }

    /* set all bytes in struct sockaddr_in to 0 */
    memset((char *) &server, 0, sizeof(struct sockaddr_in));
    /* initialize server addr */
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr = *((struct in_addr*) host->h_addr);

    /* variable used for User Choice */
    int choice=0;
    
    /* Loop that runs and prompts user for input */
    while (choice!=9) {
    printf("Enter a number for desired operation:\n1.Start Chatroom\n2.Join Existing Chatroom\n3.Exit\n");
    char inp [20];
    scanf("%s", inp);
    choice = atoi(inp);
        switch(choice) {
            case 1:
                {
                    /* ask chat coordinator to start a new chat */
                    int portnum = cc_start(s,n,len,buf,(struct sockaddr *) &server,server);
                    if (portnum != -1) {
                        char port [7];
                        snprintf(port, sizeof(port), "%d",portnum);
                        talk_to_ss(argv[1], port); 
                    }
                    break;
                }
            case 2:
                {
                    /* ask chat coordinator to join a previous chat */
                    int portnum = cc_join(s,n,len,buf,(struct sockaddr *) &server,server);
                    if (portnum != -1) {
                        char port [7];
                        snprintf(port, sizeof(port), "%d",portnum);
                        talk_to_ss(argv[1], port); 
                    }
                    break;
                }
            case 3:
                {
                    choice = 9;
                    break;
                }
            default:
                {
                    printf("Invalid Choice\n");
                }
        }
    }
    close(s);
    return 0;
}

/*-------------------------------------------------------------------------------------
 * cc_start - Request chat cordinator to start a new chat with name taken as user input
 *-------------------------------------------------------------------------------------
 */
int cc_start (int s, int n, int len, char* buf, struct sockaddr *srv_ptr, struct sockaddr_in server) {
    char message[31];
    strcpy(message, "Start ");
    char sname [25];
    printf("Enter new chatroom name(24 charachters max, no spaces)\n");
    
    bzero(sname,sizeof(sname));
    /* take chatroom name input from user */
    scanf(" %24s",sname);
    sname[24] = '\0';
    
    strcat(message,sname);
    /* send message */
    if (sendto(s, message, strlen(message) , 0, srv_ptr, len) == -1) {
        perror("sendto()");
        exit(0);
    }
    /* receive a reply */
    n = recvfrom(s, buf, BUF_SIZE, 0, srv_ptr, &len);
    if (n != -1) {
        if(strcmp(buf,"-1")==0) {
            printf("Could not start new chat room. Chatroom with name %s exists.\n",sname);
        }
        else {
            int port = atoi(buf);
            printf("A new chat session %s has been created and you have joined this session.\n",sname);
            return port;
        }
    }
    return -1;
}

/*-------------------------------------------------------------------------------------
 * cc_join - Request chat cordinator to start a new chat with name taken as user input
 *-------------------------------------------------------------------------------------
 */
int cc_join (int s, int n, int len, char* buf, struct sockaddr *srv_ptr, struct sockaddr_in server) {
    char message[31];
    strcpy(message, "Find ");
    char sname [25];
    printf("Enter chatroom name(24 charachters max, no spaces)\n");
    
    bzero(sname,sizeof(sname));
    /* take chatroom name input from user */
    scanf(" %24s",sname);
    sname[24] = '\0';
    strcat(message,sname);
    
    /* send message */
    if (sendto(s, message, strlen(message) , 0, srv_ptr, len) == -1) {
        errexit("Send failed while Joining chat");
    }
    
    /* receive a reply */
    n = recvfrom(s, buf, BUF_SIZE, 0, srv_ptr, &len);
    if (n != -1) {
        if(strcmp(buf,"-1")==0) {
            printf("Could not join chat room with name %s.\n",sname);
        }
        else {
            int port = atoi(buf);
            printf("You have joined chat session with name %s.\n",sname);
            return port;
        }
    }
    return -1;
}

/*-------------------------------------------------------------------------------------
 * talk_to_ss - Function used to communicate with session server
 *-------------------------------------------------------------------------------------
 */
void talk_to_ss(char* host, char* portno) {
    
    char buf [BUF_SIZE];

    /* length of data read or written */ 
    int n = -1;
    
    /* connect to socket */
    int s = connectsock(host, portno);
    
    printf("Now talking to port %s\n",portno);
    
    int choice =0;
    while (choice != 9) {
    printf("Enter a number for desired operation:\n1.Submit Message\n2.Get Next Chat Message\n3.Get all chat messages\n4.Leave Chatroom\n");
    char inp [30];
    scanf(" %s", inp);
    choice = atoi(inp);
    switch(choice) {
            case 1:
                {
                    /* send a message to chat session server */
                    printf("Enter chat message to send(78 charachters maximum)\n");
                    bzero(buf, BUF_SIZE);
                    
                    /* Put Submit into buffer */
                    buf[0] = 'S';
                    buf[1] = 'u';
                    buf[2] = 'b';
                    buf[3] = 'm';
                    buf[4] = 'i';
                    buf[5] = 't';
                    buf[6] = ' ';
                    
                    char message [80];
                    /* take message input from user */  
                    scanf(" %78[^\n]s", message);
                    
                    /* insert <CR> */
                    //buf[strlen(message)+1] = '\r';
                    
                    /* Insert message length */
                    char message_length [3];
                    snprintf(message_length, sizeof(message_length), "%d", strlen(message)+2);
                    
                    if (strlen(message_length)==1) {
                        buf[7] = *message_length;
                        buf[8] = ' ';
                        strcpy(buf+9, message);
                    }
                    else if (strlen(message_length)==2) {
                        buf[7] = *message_length;
                        buf[8] = *(message_length + 1);
                        buf[9] = ' ';
                        strcpy(buf+10, message);
                    }
                    
                    
                    /* send message to server */
                    n = write(s, buf, strlen(buf)+1);
                    if (n < 0) {
                        error("ERROR writing to socket");
                    }
                    break;
                }
            case 2:
                {
                    /* contact chat session server and get next message from it */
                    bzero(buf, BUF_SIZE);
                    strcpy(buf, "GetNext");
                    
                    /* send message to server */
                    n = write(s, buf, strlen(buf)+1);
                    if (n < 0) {
                        error("ERROR writing to socket");
                    }
                    
                    n = read(s, buf, BUF_SIZE);
                    if (n < 0) {
                        errexit("ERROR reading from socket");
                    }
                    if (strncmp(buf,"-1",2)==0) {
                        //-1 stands for error
                        printf("No new messages\n");
                    }
                    else {
                        if (buf[1] == ' ') {
                            printf("%s\n",buf+2);
                        }
                        else if (buf[2] == ' '){
                            printf("%s\n",buf+3);
                        }
                        else if (buf[3] == ' '){
                            printf("%s\n",buf+4);
                        }
                        
                    }
                    break;
                }
            case 3:
                {
                    /* ask chat session server and get all next messages */
                    bzero(buf, BUF_SIZE);
                    strcpy(buf, "GetAll");
                    
                    /* send message to server */
                    n = write(s, buf, strlen(buf)+1);
                    if (n < 0) {
                        error("ERROR writing to socket");
                    }
                    /* No of messages */
                    n = read(s, buf, BUF_SIZE);
                    if (n < 0) {
                        errexit("ERROR reading from socket");
                    }
                    int tot = atoi(buf);
                    printf("Total %d new messages\n",tot);
                    int i;
                    for (i=0;i<tot;i++) {
                        bzero(buf, BUF_SIZE);
                        /* Read each message */
                        n = read(s, buf, BUF_SIZE);
                        if (n < 0) {
                            errexit("ERROR reading from socket");
                        }
                        if (buf[1] == ' ') {
                            printf("%s\n",buf+2);
                        }
                        else if (buf[2] == ' '){
                            printf("%s\n",buf+3);
                        }
                        else if (buf[3] == ' '){
                            printf("%s\n",buf+4);
                        }
                    }
                    break;
                }
            case 4:
                {
                    /* contact chat session server and get next message from it */
                    bzero(buf, BUF_SIZE);
                    strcpy(buf, "Leave");
                    
                    /* send message to server */
                    n = write(s, buf, strlen(buf)+1);
                    if (n < 0) {
                        error("ERROR writing to socket");
                    }
                    
                    choice = 9;
                    break;
                }
            default:
                {
                    printf("Invalid Choice\n");
                }
        } 
    }
    close(s);
}

/*------------------------------------------------------------------------
 * connectsock - allocate & connect a socket using TCP
 * IMPORTANT - This method has not been implemented by me.
 * This method has been taken from echoClient.c that was given as a sample
 * to students studying "Network Systems" course taught by Prof. S. Mishra
 * at University of Colorado Boulder Fall 2014
 *------------------------------------------------------------------------
 */
int connectsock(const char *host, const char *portnum)
/*
 * Arguments:
 *      host      - name of host to which connection is desired
 *      portnum   - server port number
 */
{
        struct hostent  *phe;   /* pointer to host information entry    */
        struct sockaddr_in sin; /* an Internet endpoint address         */
        int     s;              /* socket descriptor                    */


        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;

    /* Map port number (char string) to port number (int)*/
        if ((sin.sin_port=htons((unsigned short)atoi(portnum))) == 0)
                errexit("can't get \"%s\" port number\n", portnum);

    /* Map host name to IP address, allowing for dotted decimal */
        if ( phe = gethostbyname(host) )
                memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
        else if ( (sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE )
                errexit("can't get \"%s\" host entry\n", host);

    /* Allocate a socket */
        s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s < 0)
                errexit("can't create socket: %s\n", strerror(errno));

    /* Connect the socket */
        if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
                errexit("can't connect to %s.%s: %s\n", host, portnum,
                        strerror(errno));
        return s;
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
