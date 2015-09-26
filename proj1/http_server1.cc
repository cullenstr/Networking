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
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#define BUFSIZE 1024
#define FILENAMESIZE 100

int handle_connection(int sock);

int main(int argc, char * argv[]) {
    int server_port = -1;
    int rc          =  0;
    int accept_sock = -1;

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
		minet_perror("Error on minet_socket(): ");
		exit(-1);
	}
    /* set server address*/
	struct sockaddr_in sa;
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
    while (1) {
	    /* handle connections */
    	struct sockaddr_in sa_connect;
        int connect_sock;
        int size=sizeof(struct sockaddr_in); 
    	if((connect_sock = minet_accept(accept_sock, &sa_connect))==-1)
            minet_perror("error in accept(): ");
        else
    	   rc = handle_connection(connect_sock);
    }
    minet_close(accept_sock);
    minet_deinit();
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
        n=minet_write(connect_sock,notok_response,strlen(notok_response));
    }
    
    /* close socket and free space */
    minet_close(connect_sock);
    if (ok) {
	return 0;
    } else {
	return -1;
    }
}
