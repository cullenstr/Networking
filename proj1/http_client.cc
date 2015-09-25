/* UNCOMMENT FOR MINET 
 * #include "minet_socket.h"
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h> //needed to access close function under gcc 4.7+
#include <iostream> //includes needed to manipulate and create strings
#include <string>
using namespace std;

#define BUFSIZE 1024

int gdb( int zero ){
	return 0;
}

int main(int argc, char * argv[]) {

    char * server_name = NULL;
    int server_port    = -1;
    char * server_path = NULL;
    char * req         = NULL;
	char buff_in[BUFSIZE];
    bool ok            = false;
	int return_val     = 0;

    /*parse args */
    if (argc != 5) {
	fprintf(stderr, "usage: http_client k|u server port path\n");
	exit(-1);
    }

    server_name = argv[2];
    server_port = atoi(argv[3]);
    server_path = argv[4];

    req = (char *)malloc(strlen("GET  HTTP/1.0\r\n\r\n") 
			 + strlen(server_path) + 1);  
	
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

    /* make socket */
	int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    /* get host IP address  */
	struct addrinfo hints; //hints used to specify type of socket type for getaddrinfo
	struct addrinfo *return_addresses; //where getaddrinfo will return the server address
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	char port_as_string[7]; //need port number as a string for this function
	sprintf( port_as_string, "%d", server_port);
	return_val = getaddrinfo(server_name, port_as_string, &hints, &return_addresses);
	if (return_val != 0) {
		fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(return_val));
		exit(-1);
	}
    /* set address */
	//struct sockaddr *sa; //server address
    /* connect to the server socket */
	return_val = connect(client_socket, return_addresses->ai_addr, return_addresses->ai_addrlen);  //attempt to connect to server address
	if (return_val != 0) {
		perror("Error on connection :");
		exit(-1);
	}
    /* send request message */
    sprintf(req, "GET %s HTTP/1.0\r\n\r\n", server_path); 
	send(client_socket, req, strlen(req) + 1, 0);
    /* wait till socket can be read. */
    /* Hint: use select(), and ignore timeout for now. */
	fd_set server_fd_set;
	FD_ZERO(&server_fd_set); //make sure sets start empty
	FD_SET(client_socket, &server_fd_set); //set server server set to contain our waiting socket
	return_val = select( client_socket+1, &server_fd_set, NULL, NULL, NULL); //return 
	if( return_val == -1) {
		perror("Error waiting for server");
		exit(-1);
	}
    /* first read loop -- read headers */
	memset(buff_in, 0, sizeof(buff_in));
	return_val = recv(client_socket, buff_in, sizeof(buff_in), 0); //if return_val is 0, we've got all the data
	//but we'll assume all the headers fit in the first buffer
	if (return_val < 0) {
		perror("Error reading from socket");
		exit(-1);
	}
	bool more_to_read = true;
	if (return_val == 0) {
		more_to_read = false;
	}
    /* examine return code */   
	string http_content;
	string return_code;
	http_content.assign(buff_in); //proper way to assign char* to string in c++?
    //Skip "HTTP/1.0"
    //remove the '\0' //not necessary when using c++ strings?
	return_code = http_content.substr(9, 3); //extract return code
	string all_ok = "200";
	ok = true;
    // Normal reply has return code 200
	int substring_marker;
	if (return_code.compare(all_ok) != 0) {
		ok = false;
		fprintf( stderr, "%s", http_content.c_str());
	}
	else {
		/* print first part of response: header, error code, etc. */
		substring_marker = http_content.find("Content-length:");
		substring_marker = http_content.find("\r\n\r\n", substring_marker);
		fprintf(stdout, "%s", (http_content.substr(0, substring_marker)).c_str());
	}
    /* second read loop -- print out the rest of the response: real web content */
    if(ok && substring_marker!=-2)
		fprintf(stdout, "%s", (http_content.substr(substring_marker)).c_str());
	while (more_to_read != false) {
		memset(buff_in, 0, sizeof(buff_in));
		//printf("there is more to read");
		return_val = recv(client_socket, buff_in, sizeof(buff_in), 0); 
		if (return_val < 0) {
			perror("Error reading from socket");
			exit(-1);
		}
		if (return_val == 0) {
			more_to_read = false;
		}
		if (ok)
			fprintf(stdout, "%s", buff_in);
		else
			fprintf(stderr, "%s", buff_in);
	}
    /*close socket and deinitialize */
	close(client_socket);
	freeaddrinfo(return_addresses);
    if (ok) {
	return 0;
    } else {
	return -1;
    }
}

