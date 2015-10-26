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
	
    while (MinetGetNextEvent(event, timeout) == 0) {

	if ((event.eventtype == MinetEvent::Dataflow) && 
	    (event.direction == MinetEvent::IN)) {
	
	    if (event.handle == mux) { // ip packet has arrived!
			MinetSendToMonitor(MinetMonitoringEvent("Packet has arrived..."));
			//cerr << "Packet has arrived...\n";
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
		
			switch ((*cs).state.GetState()) 
			{
				case LISTEN:
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
					
				case SYN_RCVD: 
					if (IS_ACK(prev_flags)) 
					{
						cerr << "ESTABLISHED\n";
						(*cs).state.SetState(ESTABLISHED);
					} 
					else if ((IS_SYN(prev_flags) && !IS_ACK(prev_flags)) || IS_RST(prev_flags))
					{
						cerr << "SEND SYNACK\n";    
						(*cs).state.SetLastSent(initial_seq_num);
						SET_SYN(flags);
						SET_ACK(flags);
						Packet send_p = make_packet(c, sptr, aptr+1, fptr, 0);
						packet_send(mux, send_p);
						(*cs).state.SetLastRecvd(aptr+1);
					}
					break;	
					
				case SYN_SENT:
				{
					cerr << "connection state: SYN_SENT\n";
				} break;
						
				case ESTABLISHED:
				{
					cerr << "connection state: established\n";
					Buffer data;
					data = p.GetPayload();
					SockRequestResponse write(WRITE, (*cs).connection, data, 0, EOK); //send some data
					MinetSend(sock, write);
				} break;
				
				case FIN_WAIT1:
				{
					cerr << "connection state: FIN_WAIT1\n";
				} break;
				
				case FIN_WAIT2:
				{
					cerr << "connection state: FIN_WAIT2\n";
				} break;
					
				case LAST_ACK:
				{
					cerr << "connection state: LAST_ACK\n";
				} break;
			}
	    } //ENDIF event.handle == mux

	    if (event.handle == sock) {
		// socket request or response has arrived
			cerr << "Socket request has arrived...\n";
			
			SockRequestResponse req;
			MinetReceive(sock,req);
			ConnectionList<TCPState>::iterator cs = clist.FindMatching(req.connection);
			ConnectionToStateMapping<TCPState> c_mapping;
			
			if (cs == clist.end()) 
			{
	            c_mapping.connection = req.connection;
	            c_mapping.state.SetState(CLOSED);
	            clist.push_back(c_mapping);
                cs = clist.FindMatching(req.connection);
	        }
			
			switch (req.type) {
				case CONNECT: { //active open
					MinetSendToMonitor(MinetMonitoringEvent("Connect request"));
					cerr << "Connect request\n";
				} break;
				case ACCEPT: { //passive open
					(*cs).state.SetState(LISTEN);
					cerr << "Accept request. Set state to LISTEN for passive open\n";
				} break;
				case STATUS: {
					MinetSendToMonitor(MinetMonitoringEvent("Status request"));
					cerr << "Status request\n";
				} break;
				case WRITE: {
					MinetSendToMonitor(MinetMonitoringEvent("Write request"));
					cerr << "Write request\n";
				} break;
				case FORWARD: {
					MinetSendToMonitor(MinetMonitoringEvent("Forward request"));
					cerr << "Forward request\n";
				} break;
				case CLOSE: {
					MinetSendToMonitor(MinetMonitoringEvent("Close request"));
					cerr << "Close request\n";
				} break;
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
	cerr << "making a packet\n";
	Packet send_p("", data_size);
	IPHeader iph;
	TCPHeader tcph;
	
	iph.SetProtocol(IP_PROTO_TCP);
	iph.SetSourceIP(c.src);
	iph.SetDestIP(c.dest);
	iph.SetTotalLength(IP_HEADER_BASE_LENGTH + TCP_HEADER_BASE_LENGTH);
	send_p.PushFrontHeader(iph);

	tcph.SetSourcePort(c.srcport, send_p);
	tcph.SetDestPort(c.destport, send_p);
	tcph.SetHeaderLen(5, send_p);
	tcph.SetFlags(flags, send_p);
	tcph.SetSeqNum(seqnum, send_p);
	tcph.SetAckNum(acknum, send_p);
	tcph.SetWinSize(14600, send_p);
	tcph.SetUrgentPtr(0, send_p);
	send_p.PushBackHeader(tcph);
	cerr << "Made packet, returning...\n";
	return send_p;
}