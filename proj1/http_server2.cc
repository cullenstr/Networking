#include "minet_socket.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>

#define BUFSIZE 1024
#define FILENAMESIZE 100
using namespace std;

int handle_connection(int sock);

int main(int argc, char * argv[]) 
{
    int server_port = -1;
    int rc          =  0;
    int accept_sock = -1;
	int sock_a      = -1;
    int fdmax       = -1;
    int connections;
    struct sockaddr_in sa;
    struct timeval t;
    t.tv_usec = 0; t.tv_sec = 5; // 5 sec timeout
	
    /* parse command line args */
    if (argc != 3) {
	fprintf(stderr, "usage: http_server1 k|u port\n");
	exit(-1);
    }

    server_port = atoi(argv[2]);

    if (server_port < 1500) {
	fprintf(stderr, "INVALID PORT NUMBER: %d; can't be < 1500\n", server_port);
	exit(-1);
    }
    
    /* initialize */
    if (toupper(*(argv[1])) == 'K') { 
		minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') { 
		minet_init(MINET_USER);
    } else {
	fprintf(stderr, "First argument must be k or u\n");
	exit(-1);
    }

    /* initialize and make socket */
	accept_sock=minet_socket(SOCK_STREAM); //accept socket will monitor for and accept connections
	if (accept_sock==-1) {
		minet_perror("Error on socket(): ");
		exit(-1);
	}
	fdmax = accept_sock;
	
    /* set server address*/
	memset(&sa, 0, sizeof(sa));
	sa.sin_port = htons(server_port);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_family = AF_INET;
	
    /* bind listening socket */
	rc = minet_bind(accept_sock, &sa);
	if (rc==-1) {
		minet_perror("Error on minet_bind(): ");
		minet_close(accept_sock); //important to close socket before exiting so port isn't tied up
		exit(-1);
	}
	
    /* start listening */
	rc = minet_listen(accept_sock,5);
	if (rc==-1) {
		minet_perror("Error on listen(): ");
		minet_close(accept_sock); 
		exit(-1);
	}
		
    /* connection handling loop: wait to accept connection */
	cout<<"Server status: Waiting...\n";
	
	//initialize list of connections
	fd_set connection_list; //list of non-minet sockets, currently 0
	FD_ZERO(&connection_list);
	FD_SET(accept_sock, &connection_list);

	/* create read list */
	fd_set read_list;
	FD_ZERO(&read_list);

    while (1) 
	{
		/* create read list */
		memcpy(&read_list, &connection_list, sizeof(&connection_list));
		
		/* do a select */
		connections = minet_select( fdmax+1, &read_list, NULL, NULL, &t);
		if (connections < 0) 
		{ 
			minet_perror("Select failed.\n"); exit(-1);
		}	
		
		/* process sockets that are ready */
		for(int i=0; (i <= fdmax) && (connections>0); ++i) 
		{
			if(FD_ISSET(i,&read_list))
			{
				connections--;
				/* for the accept socket, add accepted connection to connections */
				if(i == accept_sock) 
				{
					if ((sock_a = minet_accept(i, &sa)) <0) 
					{
						minet_perror("Accept failed.\n"); exit (-1);
					}
					cout<<"Added an accepted connection to connections.\n";

					/* for a connection socket, handle the connection */
					FD_SET(sock_a, &connection_list);
					if (sock_a > fdmax)
						fdmax = sock_a;
				}
				else 
				{
					rc = handle_connection(i);
					if (rc < 0) 
						perror("handle_connection error.\n"); 
					minet_close(i);
					FD_CLR(i, &connection_list); //remove i from set_socks
					cout<<"Removed a connection from the list of connections.\n";
					if (i == fdmax) 
					{
						while ( !(FD_ISSET(fdmax, &connection_list))) fdmax--; 
					}
				}
			}
		}
	}
}

int handle_connection(int connect_sock) {
    bool ok = false;
    FILE *fp;
    int size;
    const char * ok_response_f = "HTTP/1.0 200 OK\r\n"	\
	"Content-type: text/plain\r\n"			\
	"Content-length: %d \r\n\r\n";
 
    const char * notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"	\
	"Content-type: text/html\r\n\r\n"			\
	"<html><body bgColor=black text=white>\n"		\
	"<h2>404 FILE NOT FOUND</h2>\n"
	"</body></html>\n";
    
    /* first read loop -- get request and headers*/
    char buffer[BUFSIZE];
    int n =0;
    fd_set sockfd;
    FD_SET(connect_sock,&sockfd);
    minet_select(connect_sock+1, &sockfd, NULL, NULL, NULL);
    n = minet_read(connect_sock, buffer, BUFSIZE);
    /* parse request to get file name */
    /* Assumption: this is a GET request and filename contains no spaces*/
    int end_index =-1;
    for (int i = 4; i < strlen(buffer); ++i)
    {
        if(buffer[i]==' ')
        {
            end_index = i; break;
        }
    }
    if(end_index==-1 || end_index>n-8)
    {
        minet_close(connect_sock);
        return -1;
    }
    char filename[end_index-4];
    strncpy(filename, &buffer[4], end_index-4);
    filename[end_index-4] = '\0';
	
    /* try opening the file */
    if((fp = fopen(filename, "r"))!=NULL)
        ok=true;

    /* send response */
    if (ok) {
	/* send headers */
	    struct stat st;
        stat(filename, &st);
        size = st.st_size;
        char response[128];
        int val = sprintf(response, ok_response_f,size);
        n=minet_write(connect_sock, response, strlen(response));
	/* send file */
		do
		{
			n = read(fileno(fp), buffer,BUFSIZE);
			val = minet_write(connect_sock,buffer,n);
		} while(n>0);
    } 
	else {
	// send error response
        n=minet_write(connect_sock,(char*)notok_response,strlen(notok_response));
    }
    
    /* close socket and free space */
    close(connect_sock);
    if (ok) {
	return 0;
    } else {
	return -1;
    }
}
