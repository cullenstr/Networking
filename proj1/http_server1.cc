/* UNCOMMENT FOR MINET 
 * #include "minet_socket.h"
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h> //needed to access close function under gcc 4.7+


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
	/* UNCOMMENT FOR MINET 
	 * minet_init(MINET_KERNEL);
         */
    } else if (toupper(*(argv[1])) == 'U') { 
	/* UNCOMMENT FOR MINET 
	 * minet_init(MINET_USER);
	 */
    } else {
	fprintf(stderr, "First argument must be k or u\n");
	exit(-1);
    }


    /* initialize and make socket */
	accept_sock=socket(AF_INET, SOCK_STREAM,0);
	if (accept_sock==-1) {
		perror("Error on socket(): ");
		exit(-1);
	}
    /* set server address*/
	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	sa.sin_port = htons(server_port);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_family = AF_INET;
    /* bind listening socket */
	rc = bind(accept_sock, (struct sockaddr *)&sa,sizeof(sa));
	if (rc==-1) {
		perror("Error on bind(): ");
		exit(-1);
	}
    /* start listening */
	rc = listen(accept_sock, 5); //here we can vary the number of queued connections
	if (rc==-1) {
		perror("Error on listen(): ");
		exit(-1);
	}
    /* connection handling loop: wait to accept connection */
    while (1) {
	/* handle connections */
	struct sockaddr_storage sa_connect;
	socklen_t addr_size;
	addr_size = sizeof(sa_connect);
	int connect_sock = accept(accept_sock, (struct sockaddr*)&sa_connect, &addr_size);
	if (connect_sock==-1){
		perror("Connection error: "); }
	rc = handle_connection(connect_sock);
	//should we print an error message on -1 return?
    }
}

int handle_connection(int connect_sock) {
    bool ok = false;
	char * buffer;
	int rc = 0; //used to test return values of syscalls
	//set to be BUFSIZE
	buffer = new char[BUFSIZE];
	
    const char * ok_response_f = "HTTP/1.0 200 OK\r\n"	\
	"Content-type: text/plain\r\n"			\
	"Content-length: %d \r\n\r\n";
 
    const char * notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"	\
	"Content-type: text/html\r\n\r\n"			\
	"<html><body bgColor=black text=white>\n"		\
	"<h2>404 FILE NOT FOUND</h2>\n"
	"</body></html>\n";
    
    /* first read loop -- get request and headers*/
    rc = recv(connect_sock, buffer, sizeof(buffer), 0);
    if(rc==-1){
		perror("Error on recv() in handle function: ");
		return(-1);
	}
	//test recieved length compared to expected
    /* parse request to get file name */
    /* Assumption: this is a GET request and filename contains no spaces*/
	char *filename;
	filename = strtok(buffer, " \t"); //split request on white space
	filename = strtok(NULL, " \t"); //look at second word (GET ->filename HTTP/1.0)
    /* try opening the file */
	FILE *req_file = fopen(filename, "r");  //multiple ways to do this, either as FILE* or as ifstream, FILE seems easier to use with reads/writes
	if (req_file == NULL) {
		ok = false; //file could not be opened
	}
    /* send response */
    if (ok) {
	/* send headers */
		send( connect_sock, ok_response_f, strlen(ok_response_f), 0);
	/* send file */
		memset(buffer, 0, sizeof(buffer));
		while(fread( buffer, 1, sizeof(buffer), req_file)>0) { //not totally sure this is the best way to loop through file.  Ideas?
			send(connect_sock, buffer, strlen(notok_response), 0);
		}	
		fclose(req_file);
    } 
    else {
	// send error response
	send( connect_sock, notok_response, strlen(notok_response), 0);
    }
    /* close socket and free space */
	close(connect_sock);
	delete(buffer);
	
    if (ok) {
	return 0;
    } else {
	return -1;
    }
}
