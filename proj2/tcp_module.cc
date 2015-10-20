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
#include "tcpstate.h" //pretty sure we have to rewrite TCPState, but better check first.  The provided one should work for now

using namespace std;

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
			//step 12 prereq: have a hardcoded connection
			Connection c;
			TCPState listen_state(1, LISTEN, 1);
			ConnectionToStateMapping<TCPState> c_mapping (c, timeout, listen_state, false ); //need to set timeout.  What should initial value be?  Check rfc
			//we're assuming the current state is LISTEN
			Packet p;
			Packet send_p;
			IPHeader iph;
			iph=p.FindHeader(Headers::IPHeader);
			TCPHeader tcph;
			tcph=p.FindHeader(Headers::TCPHeader);
			unsigned char flags=0;
			tcph.GetFlags(flags);
			if(IS_SYN(flags)==True) {
				//this is our syn request
				iph.GetDestIP(c.src); //fill out connection
				iph.GetSourceIP(c.dest);
				iph.GetProtocol(c.protocol);
				tcph.GetDestPort(c.srcport);
				tcph.GetSourcePort(c.destport);
				c.state.SetState(SYN_RCVD);
				clist.push_back(c); //add connection to connection list
				//ConnectionToStateMapping<TCPState> c_mapping(c, timeout, c_state, false); 
				//connection saved.  Now we send an syn_ack
				//lets reuse the iph and tcph headers
				iph.SetSourceIP(c.connection.src);
				iph.SetDestIP(c.connection.dest);
				iph.SetTotalLength(IP_HEADER_BASE_LENGTH + TCP_HEADER_BASE_LENGTH); //there is no data in an syn_ack
				iph.SetProtocol(IP_PROTO_TCP);
				send_p.PushFrontHeader(iph); //I think IP should be the front header?  
				tcph.SetSourcePort(c.connection.src, send_p);
				tcph.SetDestPort(c.connection.dest, send_p);
				tcph.SetHeaderLen(TCP_HEADER_BASE_LENGTH, send_p);
				flags = 0;
				SET_ACK(flags);
				tcph.SetFlags(flags, send_p);
				//tcph.SetAckNum not sure how to set these yet?
				//tcph.SetSeqNum
				//tcph.SetWinSize
				send_p.PushBackHeader(tcph); //if IP is front, tcp is back
				//okay, now we have two headers 
				MinetSend(mux, send_p);
			}
			//not syn, could be ack after syn.  Let's check the list!
			//ConnectionList<UDPState>::iterator cs = clist.FindMatching(c);
		// ip packet has arrived!
	    }

	    if (event.handle == sock) {
		// socket request or response has arrived
			MinetSendToMonitor(MinetMonitoringEvent("Socket request/response has arrived..."));
			SockRequestResponse req;
			MinetReceive(sock,req);
			switch (req.type) {
				case CONNECT: {
					MinetSendToMonitor(MinetMonitoringEvent("Connect request"));
				}
				case ACCEPT: {
					MinetSendToMonitor(MinetMonitoringEvent("Accept request"));
				}
				case STATUS: {
					MinetSendToMonitor(MinetMonitoringEvent("Status request"));
				}
				case WRITE: {
					MinetSendToMonitor(MinetMonitoringEvent("Write request"));
				}
				case FORWARD: {
					MinetSendToMonitor(MinetMonitoringEvent("Forward request"));
				}
				case CLOSE: {
					MinetSendToMonitor(MinetMonitoringEvent("Close request"));
				}
				default: {
					MinetSendToMonitor(MinetMonitoringEvent("Not a valid request"));
				}
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
