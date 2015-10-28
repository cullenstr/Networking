// You will build this in project part B - this is merely a
// stub that does nothing but integrate into the stack

// For project parts A and B, an appropriate binary will be 
// copied over as part of the build process
//#define DEBUGGING
//Uncomment above line to see debugging statements

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


#include <iostream>
#include <string>

#include "Minet.h"
#include "tcpstate.h" //The provided one should work for now

using namespace std;

Packet make_packet(const Connection c, const unsigned int &seqnum, const unsigned int &acknum, unsigned char &flags, int data_size);
int packet_send(const MinetHandle &handle, Packet &p);
int packet_receive(const MinetHandle &handle, Packet &p);

/*struct TCPState {
    // need to write this
    std::ostream & Print(std::ostream &os) const { 
	os << "TCPState()" ; 
	return os;
    }
};*/


int main(int argc, char * argv[]) {
    MinetHandle mux;
    MinetHandle sock;
    
    ConnectionList<TCPState> clist;

    MinetInit(MINET_TCP_MODULE);

    mux = MinetIsModuleInConfig(MINET_IP_MUX) ?  
	MinetConnect(MINET_IP_MUX) : 
	MINET_NOHANDLE;
    
    sock = MinetIsModuleInConfig(MINET_SOCK_MODULE) ? 
	MinetAccept(MINET_SOCK_MODULE) : 
	MINET_NOHANDLE;

    if ( (mux == MINET_NOHANDLE) && 
	 (MinetIsModuleInConfig(MINET_IP_MUX)) ) {

	MinetSendToMonitor(MinetMonitoringEvent("Can't connect to ip_mux"));

	return -1;
    }

    if ( (sock == MINET_NOHANDLE) && 
	 (MinetIsModuleInConfig(MINET_SOCK_MODULE)) ) {

	MinetSendToMonitor(MinetMonitoringEvent("Can't accept from sock_module"));

	return -1;
    }
    
    cerr << "tcp_module handling tcp traffic.......\n";

    MinetSendToMonitor(MinetMonitoringEvent("tcp_module handling tcp traffic........"));

	srand(time(NULL));
	
    MinetEvent event;
    double timeout = 1000;
	unsigned char flags=0;
	unsigned char prev_flags;
	unsigned int ack_num = 0;
	unsigned int initial_seq_num = 0;
	
	unsigned int &aptr = ack_num;
	unsigned char &fptr = flags;
	unsigned int &sptr = initial_seq_num;
	
	//const IPAddress this_ip_addr = '192.168.160.9';
	//const short this_port = 80;
	
    while (MinetGetNextEvent(event, timeout) == 0) {
	if ((event.eventtype == MinetEvent::Dataflow) && 
	    (event.direction == MinetEvent::IN)) {
	
	    if (event.handle == mux) { // ip packet has arrived!
			MinetSendToMonitor(MinetMonitoringEvent("Packet has arrived..."));
			cerr << "Packet has arrived...\n";
			flags = 0; //set flags to 0;
			
			Connection c;
			Packet p;
			MinetReceive(mux,p);
			p.ExtractHeaderFromPayload<TCPHeader>(TCPHeader::EstimateTCPHeaderLength(p));
			
			IPHeader iph;
			iph=p.FindHeader(Headers::IPHeader);
			TCPHeader tcph;
			tcph=p.FindHeader(Headers::TCPHeader);
			tcph.GetFlags(prev_flags);
			tcph.GetSeqNum(aptr);
	        tcph.GetAckNum(sptr);
			
			iph.GetDestIP(c.src); //fill out connection
			iph.GetSourceIP(c.dest);
			tcph.GetDestPort(c.srcport);
			tcph.GetSourcePort(c.destport);
			c.protocol = IP_PROTO_TCP;
			cerr << "TCPHeader: "<<tcph << "\n\n";
			ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
			if (initial_seq_num==0) //if not set yet
				initial_seq_num = rand(); //placeholder.  How do we want to set our seq number?

			#ifdef DEBUGGING
	        cerr << "TCPHeader: "<<tcph << "\n\n";
			cerr << "Connection state " << cs->state.GetState() << "\n";
			#endif
			switch ((*cs).state.GetState()) 
			{
				cerr << "We found the state\n";
				case LISTEN:
				{
					if( (IS_SYN(prev_flags) && !IS_ACK(prev_flags)) || IS_RST(prev_flags)) 
					{ //this is our syn request
						cerr << "SYN request in a passive open state\n";
						(*cs).state.SetState(SYN_RCVD);
						(*cs).state.SetLastSent(initial_seq_num);
						flags=0;
						SET_SYN(flags);
						SET_ACK(flags);
				
						//connection saved.  Now we send an syn_ack
						cerr << "Set flags for syn-ack\n";
						cerr << "Ready to make packet\n";
						Packet send_p = make_packet(c, sptr, aptr+1, fptr, 0);
						packet_send(mux, send_p);
						(*cs).state.SetLastRecvd(aptr + 1);
					} break;
				}	
				case SYN_RCVD: 
				{
					if (IS_ACK(prev_flags)) 
					{
						//set state to established
						cerr << "ESTABLISHED\n";
						(*cs).state.SetState(ESTABLISHED);
						//notify socket
						cerr << "connection state: established\n";
						Buffer data;
						data = p.GetPayload();
						SockRequestResponse write(WRITE, (*cs).connection, data, 0, EOK); //send some data
						MinetSend(sock, write);
					} 
					else if ((IS_SYN(prev_flags) && !IS_ACK(prev_flags)) || IS_RST(prev_flags))
					{ //is there ever a case where this will happen
						cerr << "SEND SYNACK\n";    
						SET_SYN(flags);
						SET_ACK(flags);
						Packet send_p = make_packet(c, sptr, aptr+1, fptr, 0);
						packet_send(mux, send_p);
						(*cs).state.SetLastRecvd(aptr+1);
					}
					break;	
				}
				case SYN_SENT:
				{
					cerr << "connection state: SYN_SENT\n";
					if (IS_ACK(prev_flags) && IS_SYN(prev_flags)) {
						cerr << "recieved SYN ACK, send ACK";
						//transition to established
						(*cs).state.SetState(ESTABLISHED);
						//send ack
						SET_ACK(flags);
						Packet send_p = make_packet(c, sptr, aptr+1, fptr, 0);
						packet_send(mux, send_p);
						(*cs).state.SetLastRecvd(aptr+1);
						//established, notify socket
						cerr << "connection state: established\n";
						Buffer data;
						data = p.GetPayload();
						SockRequestResponse write(WRITE, (*cs).connection, data, 0, EOK); //send some data
						MinetSend(sock, write);
					}
					if (IS_SYN(prev_flags) && !IS_ACK(prev_flags)) {
						//transition to SYN_RCVD
						(*cs).state.SetState(SYN_RCVD);
						//send ack
						SET_ACK(flags);
						Packet send_p = make_packet(c, sptr, aptr+1, fptr, 0);
						packet_send(mux, send_p);
						(*cs).state.SetLastRecvd(aptr+1);
					} 
					else {
						cerr << "Did not recieve expected SYN ACK or SYN response";
					}break;
				}
				
				case ESTABLISHED:
				{	cerr << "connection state: established\n";
					Buffer data;
					data = p.GetPayload();
					SockRequestResponse write(WRITE, (*cs).connection, data, 0, EOK); //send some data
					MinetSend(sock, write);
					break;
				}
				
				case FIN_WAIT1:
				{
					cerr << "connection state: FIN_WAIT1\n";
					//recieve ACK, transition to FIN_WAIT2
					if(IS_ACK(prev_flags)) {
						cerr << "transitioning to FIN_WAIT2\n";
						(*cs).state.SetState(FIN_WAIT2);
					}
				break;
				}
				
				case FIN_WAIT2:
				{
					cerr << "connection state: FIN_WAIT2\n";
					//recieve FIN
					if(IS_FIN(prev_flags)) {
						cerr << "Recieved FIN, transitioning to TIME_WAIT\n";
						SET_ACK(flags);
						Packet send_p = make_packet(c, sptr, aptr+1, fptr, 0);
						packet_send(mux, send_p);
						(*cs).state.SetLastRecvd(aptr+1);
						(*cs).state.SetState(TIME_WAIT);
						//set a timeout of 2MSL	
					} break;
				} 
				
				case CLOSING: 
				{
					cerr << "connection state: CLOSING\n";
					//recieve ACK
					if(IS_ACK(prev_flags)) {
						cerr << "Recieved ACK, transitioning to TIME_WAIT\n";
						(*cs).state.SetState(TIME_WAIT);
						//set a timeout of 2MSL
					} break;
				} 
				
				case LAST_ACK:
				{
					cerr << "connection state: LAST_ACK\n";
					//recieve ACK
					if(IS_ACK(prev_flags)) {
						cerr << "Recieved ACK, closing connection\n";
						(*cs).state.SetState(CLOSED);
						//remove from list
					} break;
				} 
			}
	    } //ENDIF event.handle == mux

	    if (event.handle == sock) {
		// socket request or response has arrived
			cerr << "Socket request has arrived...\n";
			
			SockRequestResponse req;
			SockRequestResponse response;
			MinetReceive(sock,req);
			ConnectionList<TCPState>::iterator cs = clist.FindMatching(req.connection);
			ConnectionToStateMapping<TCPState> c_mapping;
			unsigned char flags = 0;

			if(req.type==CONNECT) {
				MinetSendToMonitor(MinetMonitoringEvent("Connect request"));
				cerr << "Connect request : Active Open\n";
				//active open, change state to SYN_SENT
				int seq_num = rand();
				//&sptr = seq_num;
				TCPState new_state(seq_num, SYN_SENT, 1);
				ConnectionToStateMapping<TCPState> new_mapping(req.connection, timeout, new_state, false);
				//send status response immediately
				/*
				response.type = STATUS;
				response.connection = req.connection;
				response.bytes = 0;
				response.error = EOK;
				MinetSend(sock, response); */
				//make a SYN packet
				flags = 0;
				SET_SYN(flags);
				Packet send_p = make_packet(req.connection, sptr, aptr+1, flags, 0);
				//send SYN packet
				packet_send( mux, send_p);
				sleep(1);
				packet_send(mux, send_p); //according to prof Lange, you need to send this twice.  
				//I wasn't really clear on the reasoning, but it does seem to work
				clist.push_back(new_mapping);
				cs = clist.FindMatching(req.connection);
				#ifdef DEBUGGING
				cerr << "new mapping " << new_mapping << "\n";
				#endif
			}
			else if (cs == clist.end()) //separated from CONNECT because it was overwriting the new connection mapping in the list
			{
	            c_mapping.connection = req.connection;
	            c_mapping.state.SetState(CLOSED);
	            clist.push_back(c_mapping);
				cs = clist.FindMatching(req.connection);
	        }
			
			switch ((*cs).state.GetState()) {
				case CLOSED: {
					if(req.type==CONNECT) { //active open
						//handled above
					}
					else if(req.type==ACCEPT) { //passive open
						cerr << "Passive Open\n";
						(*cs).state.SetState(LISTEN);
						cerr << "Accept request. Set state to LISTEN for passive open\n";
					}
				} break;
				
				case LISTEN: {
					if(req.type==CLOSE){
						//delete TCB
						cerr << "Recieved CLOSE from LISTEN state";
					}
					else if(req.type==WRITE){
						//send SYN
						SET_SYN(flags);
						Packet send_p = make_packet(req.connection, sptr, aptr+1, fptr, 0);
						cerr << "Recieved SEND from LISTEN state";
						
					}
				} break;
				
				case SYN_RCVD: {
					if(req.type==CLOSE){
						//send FIN
						SET_FIN(flags);
						Packet send_p = make_packet(req.connection, sptr, aptr+1, fptr, 0);
					}
				} break;
				
				case ESTABLISHED: {
					if(req.type==CLOSE){
						//send FIN
						SET_FIN(flags);
						Packet send_p = make_packet(req.connection, sptr, aptr+1, fptr, 0);
					}
				} break;
				
				case CLOSE_WAIT: {
					if(req.type==CLOSE){
						//send FIN
						SET_FIN(flags);
						Packet send_p = make_packet(req.connection, sptr, aptr+1, fptr, 0);
					}
				} break;
			}
	    }
	}

	if (event.eventtype == MinetEvent::Timeout) {
	    // timeout ! probably need to resend some packets
	}

    }

    MinetDeinit();

    return 0;
}

//function to wrap MinetSend in order to simulate packet loss
//still need to implement: reordering
int packet_send(const MinetHandle &handle, Packet &p) {
	int return_val=-1;
	bool packet_loss=false; //do we want to simulate packet loss?
	bool corruption=false; //do we want to simulate corruption?
	int loss_1_out_of=10; //if loss==true, drop/corrupt 1 packet out of every x
	int drop = (rand()%loss_1_out_of) + 1;
	#ifdef DEBUGGING
		cerr << "Using packet_send function: loss is " << packet_loss << " corruption is " << corruption << " drop is " << drop << "\n";
	#endif
	if (corruption==true && drop==1) { //for now we'll only simulate corruption with packets
		unsigned short check = 0;
		TCPHeader tcph;
		tcph=p.FindHeader(Headers::TCPHeader);
		tcph.GetChecksum(check);
		check+=10; //corrupt the checksum
		tcph.SetChecksum(check);
		p.PushBackHeader(tcph);
		cerr << "Packet should be corrupted";
	}
	if (packet_loss==true && drop==1) {
		//do nothing, packet is not sent
		cerr << "Packet dropped";
	}
	else {
		return_val = MinetSend(handle, p);
		cerr << "Packet sent\n";
	}
	return return_val;
}

//function to wrap MinetRecieve in order to simulate packet loss
//still need to implement: reordering
int packet_receive(const MinetHandle &handle, Packet &p) {
	int return_val=-1;
	bool packet_loss=false; //do we want to simulate packet loss?
	bool corruption=false; //do we want to simulate corruption?
	int loss_1_out_of = 10; //if loss==true, drop/corrupt 1 packet out of every x
	int drop = (rand()%loss_1_out_of) + 1;
	if (packet_loss==true && drop==1) {
		//do nothing, packet is not recieved
	}
	else {
		return_val = MinetReceive(handle, p);
	}
	if( corruption==true && drop==1) {
		unsigned short check = 0;
		TCPHeader tcph;
		tcph=p.FindHeader(Headers::TCPHeader);
		tcph.GetChecksum(check);
		check+=10; //corrupt the checksum
		tcph.SetChecksum(check);
		p.PushBackHeader(tcph);
	}
	return return_val;
}

Packet make_packet(const Connection c, const unsigned int &seqnum, const unsigned int &acknum, unsigned char &flags, int data_size) {
	Packet send_p("", data_size);
	IPHeader iph;
	TCPHeader tcph;
	
	iph.SetProtocol(IP_PROTO_TCP);
	iph.SetSourceIP(c.src);
	iph.SetDestIP(c.dest);
	iph.SetTotalLength(data_size + IP_HEADER_BASE_LENGTH + TCP_HEADER_BASE_LENGTH); //why did we take data size out of the header
	send_p.PushFrontHeader(iph);

	tcph.SetSourcePort(c.srcport, send_p);
	tcph.SetDestPort(c.destport, send_p);
	tcph.SetHeaderLen(5, send_p); //isn't the header base length 20?  Why did we set to 5?
	tcph.SetFlags(flags, send_p);
	tcph.SetSeqNum(seqnum, send_p);
	tcph.SetAckNum(acknum, send_p);
	tcph.SetWinSize(14600, send_p); //wont the window size change variably?
	tcph.SetUrgentPtr(0, send_p);
	send_p.PushBackHeader(tcph);
	#ifdef DEBUGGING
	cerr << "Packet IP header " << iph << " TCP header " << tcph << "\n\n\n";
	#endif
	cerr << "Made packet, returning...\n";
	return send_p;
}
