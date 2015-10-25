// You will build this in project part B - this is merely a
// stub that does nothing but integrate into the stack

// For project parts A and B, an appropriate binary will be 
// copied over as part of the build process



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

#include "Minet.h"
#include "tcpstate.h" //The provided one should work for now

using namespace std;

void make_packet(Packet &send_p, ConnectionToStateMapping<TCPState> &c, unsigned char flags, int data_size);
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
    
    cerr << "tcp_module STUB VERSION handling tcp traffic.......\n";

    MinetSendToMonitor(MinetMonitoringEvent("tcp_module STUB VERSION handling tcp traffic........"));

    MinetEvent event;
    double timeout = 1;
	

	
    while (MinetGetNextEvent(event, timeout) == 0) {

	if ((event.eventtype == MinetEvent::Dataflow) && 
	    (event.direction == MinetEvent::IN)) {
	
	    if (event.handle == mux) {
			MinetSendToMonitor(MinetMonitoringEvent("Packet has arrived..."));
			cerr << "Packet has arrived...\n";
			//step 12 prereq: have a hardcoded connection
			Connection c_listen;
			Connection c;
			TCPState listen_state(1, LISTEN, 1);
			ConnectionToStateMapping<TCPState> c_l_mapping (c_listen, timeout, listen_state, false ); //passive listen will not timeout
			clist.push_back(c_l_mapping);
			Packet p;
			Packet send_p;
			packet_receive( mux, p);
			p.ExtractHeaderFromPayload<TCPHeader>(20);
			IPHeader iph;
			iph=p.FindHeader(Headers::IPHeader);
			TCPHeader tcph;
			tcph=p.FindHeader(Headers::TCPHeader);
			unsigned char flags=0;
			tcph.GetFlags(flags);
			cerr << "We found the headers and got flags\n";
			iph.GetDestIP(c.src); //fill out connection
			iph.GetSourceIP(c.dest);
			iph.GetProtocol(c.protocol);
			tcph.GetDestPort(c.srcport);
			tcph.GetSourcePort(c.destport);
			if(IS_SYN(flags)==true) {
				//this is our syn request
				cerr << "SYN request\n";
				ConnectionList<TCPState>::iterator cs = clist.FindMatching(c_listen); //see if we are listening
				if (cs!=clist.end()) { //we are listening
					cerr << "We are in a passive open state\n";
					int initial_seq_num = rand(); //placeholder.  How do we want to set our seq number?
					TCPState new_state(initial_seq_num, SYN_RCVD, 1);
					ConnectionToStateMapping<TCPState> c_mapping (c, timeout, new_state, true ); //default timeout of 1 second
					clist.push_back(c_mapping); //add connection to connection list
					cerr << "Connection added to list\n";
					//connection saved.  Now we send an syn_ack
					flags = 0;
					SET_ACK(flags);
					SET_SYN(flags);
					cerr << "Set flags for syn-ack\n";
					c_mapping.state.SetLastAcked(initial_seq_num); //this will be sent as our sequence number
					cerr << "Ready to make packet\n";
					make_packet( send_p, c_mapping, flags, 0);
					packet_send(mux, send_p);
				}
				else {
					cerr << "SYN request, but we aren't listening for connections";
				}
			}
			else if(IS_ACK(flags)==true) {
				cerr << "ACK packet\n";
				ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
				if (cs!=clist.end()) {
					cerr << "Connection already open, go ahead\n";
				}
			}
		// ip packet has arrived!
	    }

	    if (event.handle == sock) {
		// socket request or response has arrived
			MinetSendToMonitor(MinetMonitoringEvent("Socket request/response has arrived..."));
			cerr << "Socket request has arrived...\n";
			SockRequestResponse req;
			MinetReceive(sock,req);
			switch (req.type) {
				case CONNECT: {
					MinetSendToMonitor(MinetMonitoringEvent("Connect request"));
					cerr << "Connect request\n";
				}
				case ACCEPT: {
					MinetSendToMonitor(MinetMonitoringEvent("Accept request"));
					cerr << "Accept request\n";
				}
				case STATUS: {
					MinetSendToMonitor(MinetMonitoringEvent("Status request"));
					cerr << "Status request\n";
				}
				case WRITE: {
					MinetSendToMonitor(MinetMonitoringEvent("Write request"));
					cerr << "Write request\n";
				}
				case FORWARD: {
					MinetSendToMonitor(MinetMonitoringEvent("Forward request"));
					cerr << "Forward request\n";
				}
				case CLOSE: {
					MinetSendToMonitor(MinetMonitoringEvent("Close request"));
					cerr << "Close request\n";
				}
				default: {
					MinetSendToMonitor(MinetMonitoringEvent("Not a valid request"));
				}
			}
	    }
	    
	    else{
			MinetSendToMonitor(MinetMonitoringEvent("other request??..."));
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
	cerr << "Using packet_send function: loss is " << packet_loss << " corruption is " << corruption << " drop is " << drop << "\n";
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
		cerr << "Packet sent";
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

void make_packet(Packet &send_p, ConnectionToStateMapping<TCPState> &c, unsigned char flags, int data_size) {
	cerr << "making a packet\n";
	IPHeader iph;
	TCPHeader tcph;
	
	iph.SetSourceIP(c.connection.src);
	iph.SetDestIP(c.connection.dest);
	iph.SetTotalLength(data_size + IP_HEADER_BASE_LENGTH + TCP_HEADER_BASE_LENGTH);
	iph.SetProtocol(IP_PROTO_TCP);
	send_p.PushFrontHeader(iph);

	tcph.SetSourcePort(c.connection.src, send_p);
	tcph.SetDestPort(c.connection.dest, send_p);
	tcph.SetHeaderLen(TCP_HEADER_BASE_LENGTH, send_p);
	tcph.SetFlags(flags, send_p);
	tcph.SetAckNum(c.state.GetLastRecvd(), send_p);
	tcph.SetSeqNum(c.state.GetLastAcked(), send_p);
	tcph.SetWinSize(c.state.GetN(), send_p);
	tcph.SetUrgentPtr(0, send_p);
	send_p.PushBackHeader(tcph);
	cerr << "Made packet, returning...\n";
	return;
}
