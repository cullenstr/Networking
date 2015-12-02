// You will build this in project part B - this is merely a
// stub that does nothing but integrate into the stack

// For project parts A and B, an appropriate binary will be 
// copied over as part of the build process
//#define DEBUGGING
#define DEBUGGING1
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
#include <list> //needed for write record list
#include <algorithm> //needed for min function.  Could live without

#include "Minet.h"
#include "tcpstate.h" //The provided one should work for now

using namespace std;

//int update_map_state(const TCPState state, unsigned int new_last_acked, int sent_size, int recv_size);
Packet make_packet(const Connection c, const unsigned int &seqnum, const unsigned int &acknum, unsigned char &flags, int data_size, Buffer &data_buffer, unsigned short rwnd);
int packet_send(const MinetHandle &handle, Packet &p);
int packet_receive(MinetHandle &handle, Packet &p);


/*struct TCPState {
    // need to write this
    std::ostream & Print(std::ostream &os) const { 
	os << "TCPState()" ; 
	return os;
    }
};*/

struct write_record {
	Connection connection;
	Buffer write_buffer;
	unsigned int size_total;
};

int main(int argc, char * argv[]) {
    MinetHandle mux;
    MinetHandle sock;
    
    ConnectionList<TCPState> clist;
	list<write_record> wr_list;

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
    double timeout = 5;
	unsigned char flags=0;
	unsigned char prev_flags;
	unsigned int ack_num=0;
	unsigned int initial_seq_num=0;
	unsigned short recv_datasize=0;
	
	unsigned int &aptr = ack_num;
	unsigned char &fptr = flags;
	unsigned int &sptr = initial_seq_num;
	
	Buffer empty_buff;
	empty_buff.Clear();
	Buffer &ebptr = empty_buff;
	
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
			
			//extract connection information from headers
			bool checksum_ok = false;
			IPHeader iph;
			iph=p.FindHeader(Headers::IPHeader);
			TCPHeader tcph;
			tcph=p.FindHeader(Headers::TCPHeader);
			tcph.GetFlags(prev_flags);
			tcph.GetSeqNum(aptr);
	        tcph.GetAckNum(sptr);
			iph.GetDestIP(c.src);
			iph.GetSourceIP(c.dest);
			tcph.GetDestPort(c.srcport);
			tcph.GetSourcePort(c.destport);
			c.protocol = IP_PROTO_TCP;
			checksum_ok = tcph.IsCorrectChecksum(p);

			
			//get data
			unsigned char iph_len;
			unsigned char tcph_len;
			iph.GetTotalLength(recv_datasize);
			iph.GetHeaderLength(iph_len);
			tcph.GetHeaderLen(tcph_len);
			iph_len<<=2;
			tcph_len<<=2;
			recv_datasize = recv_datasize - (tcph_len + iph_len);
			Buffer &recv_buffer = p.GetPayload().ExtractFront(recv_datasize);
			recv_datasize = recv_buffer.GetSize();
			ConnectionList<TCPState>::iterator cs = clist.FindMatching(c);
			
			#ifdef DEBUGGING1
			cerr<<"\n\n===================RCVD PACKET INFO=============";
			cerr << "\n***Packet IP header*** " << iph << "\n\n***TCP header*** " << tcph;
			cerr << "\n\n***Buffer*** " << recv_buffer << "\n";
			cerr<<"=====================END RCVD PACKET INFO===========\n\n";
	        cerr << "Connection last_sent " << cs->state.last_sent << " last acked " << cs->state.last_acked << " last recieved " << cs->state.last_recvd << "\n";
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
						sptr=rand(); //this connection hasn't set a sequence number yet
						flags = 0;
						SET_SYN(flags);
						SET_ACK(flags);
				
						//connection saved.  Now we send an syn_ack
						cerr << "Set flags for syn-ack\n";
						cerr << "Ready to make packet\n";
						//make_packet( connection, seq, acknum, flags, datasize)
						Packet send_p = make_packet(c, sptr, aptr+1, fptr, 0, ebptr, (*cs).state.GetRwnd());
						packet_send(mux, send_p);
						(*cs).state.SetLastSent(sptr);
						(*cs).state.SetLastAcked(sptr);
						(*cs).state.SetLastRecvd(aptr + 1);
					}
				} break;
				case SYN_RCVD: 
				{
					if (IS_ACK(prev_flags)) //i believe we just need to set state to established here
					{
						//set state to established
						cerr << "ESTABLISHED\n";
						(*cs).state.SetState(ESTABLISHED);
						//notify socket
						cerr << "connection state: established\n";
						SockRequestResponse write(WRITE, (*cs).connection, ebptr, 0, EOK); //notify socket connection established
						MinetSend(sock, write);
					} 
					else if ((IS_SYN(prev_flags) && !IS_ACK(prev_flags)) || IS_RST(prev_flags))
					{ //is there ever a case where this will happen --- happening in current version
						cerr << "SEND SYNACK AGAIN\n";    
						SET_SYN(flags);
						SET_ACK(flags);
						Packet send_p = make_packet(c, sptr, aptr+1, fptr, 0, ebptr, (*cs).state.GetRwnd());
						packet_send(mux, send_p);
						(*cs).state.SetLastRecvd(aptr);
						(*cs).state.SetLastAcked(sptr);
					}
				} break;
				case SYN_SENT:
				{
					cerr << "connection state: SYN_SENT\n";
					if (IS_ACK(prev_flags) && IS_SYN(prev_flags)) { //should be active open
						cerr << "recieved SYN ACK, send ACK";
						//transition to established
						(*cs).state.SetState(ESTABLISHED);
						cerr << "connection state: established\n";
						//we won't have any data to write yet, but we should ack the packet
						SET_ACK(flags);
						SockRequestResponse write(WRITE, (*cs).connection, ebptr, 0, EOK); //notify socket connection established
						MinetSend(sock, write);
						(*cs).state.SetLastRecvd(aptr+1);
						
						Packet send_p = make_packet(c, sptr, aptr+recv_datasize+1, fptr, 0, ebptr, (*cs).state.GetRwnd());
						packet_send(mux, send_p);

					}
					else if (IS_SYN(prev_flags) && !IS_ACK(prev_flags)) { //should be passive open
						//transition to SYN_RCVD
						(*cs).state.SetState(SYN_RCVD);
						//send ack
						SET_ACK(flags);
						SET_SYN(flags);
						Packet send_p = make_packet(c, sptr, aptr+1, fptr, 0, ebptr, (*cs).state.GetRwnd());
						packet_send(mux, send_p);
						(*cs).state.SetLastAcked(aptr+1);
					} 
				} break;
				case ESTABLISHED: //NEEDS SOME WORK! /*possible scenarios: IS_FIN is set, IS_RST is set, or data packet is rcvd */
				{	
					cerr << "is fin: " << IS_FIN(prev_flags) << " is rst " << IS_RST(prev_flags) << " checksum okay?" << checksum_ok;
					if (!IS_FIN(prev_flags) && !IS_RST(prev_flags) && checksum_ok) //rcvd data packet
					{
						if (IS_PSH(prev_flags))
						{
							if(recv_datasize>0) {
								(*cs).state.RecvBuffer.AddBack(recv_buffer);
								write_record new_wr {c, recv_buffer, recv_datasize};
								wr_list.push_back(new_wr);
								SockRequestResponse write(WRITE, (*cs).connection, recv_buffer, recv_datasize, EOK);
								MinetSend(sock, write);
							}
							
							SET_ACK(flags);
							Packet send_p = make_packet(c, sptr, aptr+recv_datasize, fptr, 0, ebptr, 14600);
							(*cs).state.SetLastRecvd(aptr + recv_datasize);
							packet_send(mux, send_p);
						}
						
						else if (IS_ACK(prev_flags)) 
						{
							//if this is an ack packet, send data if we have it
							(*cs).timeout = Time()+5;
							int send_datasize=0;
							Buffer send_buffer;
							send_buffer.Clear();
							if (aptr==(*cs).state.GetLastSent()+1) { //if this isn't a duplicate ack, we can remove data from the send buffer
								//remove acked data from send buffer
								int send_remove=0;
								send_remove = sptr - (*cs).state.GetLastAcked(); //will be their new ack# to our old last ack
								(*cs).state.SendBuffer.Erase(0, send_remove);
								(*cs).state.N+=send_remove;
							}
							unsigned short r_windowsize;
							tcph.GetWinSize(r_windowsize);
							//add data to packet
							if ((*cs).state.SendBuffer.GetSize()>0) { //figure out how much data we can send
								if (r_windowsize<TCP_MAXIMUM_SEGMENT_SIZE && r_windowsize<(*cs).state.SendBuffer.GetSize()) //if the recieve window is smaller than the data we have
									send_datasize = (*cs).state.GetRwnd(); 
								else if (r_windowsize>TCP_MAXIMUM_SEGMENT_SIZE && (*cs).state.SendBuffer.GetSize()>TCP_MAXIMUM_SEGMENT_SIZE)
									send_datasize = TCP_MAXIMUM_SEGMENT_SIZE;
								else  //send entire send buffer
									send_datasize = min((size_t)r_windowsize, (*cs).state.SendBuffer.GetSize());
								send_buffer = (*cs).state.SendBuffer.ExtractFront(send_datasize);	
							}
							cerr << "\nsetting last acked to: " << sptr << "\nsetting last sent to: " << (send_datasize+(*cs).state.GetLastSent()) << endl;
							(*cs).state.SetLastRecvd(aptr);
							(*cs).state.SetLastAcked(sptr); //the last ack we recieved
							(*cs).state.SetLastSent(send_datasize+(*cs).state.GetLastSent()); //last byte we sent
						}
						
					}
					else if (IS_FIN(prev_flags))
					{
						cerr<< "\n\n...FIN FOUND... sending ack\n";
						(*cs).state.SetState(CLOSE_WAIT);
						SET_ACK(flags);
						Packet send_p = make_packet(c, sptr, aptr+1, fptr, 0, ebptr, (*cs).state.GetRwnd());
						packet_send(mux, send_p);
						(*cs).state.SetLastRecvd(aptr+1);
					}
				} break;
				case CLOSE_WAIT:
				{
					cerr<<"\nIN CLOSE WAIT\n";
					cerr<<"sending fin, transitioning to LAST_ACK\n";
					(*cs).state.SetState(LAST_ACK);
					SET_FIN(flags);
					Packet send_p = make_packet(c, sptr, aptr+1, fptr, 0, ebptr, (*cs).state.GetRwnd());
					packet_send(mux, send_p);
					
					/* 
						//SET_RST(flags);
						//(*cs).state.SetState(LISTEN);
						
						'cheap' way to close connection, and allows another nc without having to restart minet
						I'm not sure if we can actually test that the connection is closing properly while only using nc, even if logic is correct
					*/
				} break;
				case FIN_WAIT1:
				{
					cerr << "connection state: FIN_WAIT1\n";
					//if recieved ACK, transition to FIN_WAIT2
					if(IS_ACK(prev_flags) && !IS_PSH(prev_flags)) {
						cerr << "transitioning to FIN_WAIT2\n";
						(*cs).state.SetState(FIN_WAIT2);
						(*cs).state.SetLastRecvd(aptr + 1);
					}
				} break;
				case FIN_WAIT2:
				{
					cerr << "connection state: FIN_WAIT2\n";
					//recieve FIN
					if(IS_FIN(prev_flags)) {
						cerr << "Recieved FIN, transitioning to TIME_WAIT\n";
						SET_ACK(flags);
						Packet send_p = make_packet(c, sptr, aptr+1, fptr, 0, ebptr, (*cs).state.GetRwnd());
						packet_send(mux, send_p);
						(*cs).state.SetLastRecvd(aptr+1);
						(*cs).state.SetState(TIME_WAIT);
						//set a timeout of 2MSL	
						(*cs).timeout = Time()+MSL_TIME_SECS;
					}
				} break; 
				case CLOSING: 
				{
					cerr << "connection state: CLOSING\n";
					//recieve ACK
					if(IS_ACK(prev_flags)) {
						cerr << "Recieved ACK, transitioning to TIME_WAIT\n";
						(*cs).state.SetState(TIME_WAIT);
						//set a timeout of 2MSL
						(*cs).timeout = Time()+MSL_TIME_SECS;
					}
				} break;
				case LAST_ACK:
				{
					cerr << "connection state: LAST_ACK\n";
					//recieve ACK
					if(IS_ACK(prev_flags)) {
						cerr << "Recieved ACK, closing connection\n";
						(*cs).state.SetState(CLOSED);
						SockRequestResponse close(CLOSE, (*cs).connection, ebptr, 14600, EOK); 
						MinetSend(sock, close);
						//remove from list
						clist.erase(cs);
					}
				} break;
				
			} //END TCPState switch
	    } //ENDIF event.handle == mux

	    if (event.handle == sock) {
		// socket request or response has arrived
			SockRequestResponse req;
			SockRequestResponse response;
			MinetReceive(sock,req);
			cerr << "\nSocket request has arrived...req.type is " << req.type << "\n";
			ConnectionList<TCPState>::iterator cs = clist.FindMatching(req.connection);
			ConnectionToStateMapping<TCPState> c_mapping;
			unsigned char flags = 0;

			if(req.type==CONNECT) {
				MinetSendToMonitor(MinetMonitoringEvent("Connect request"));
				cerr << "Connect request : Active Open\n";
				//active open, change state to SYN_SENT
				int seq_num = rand();
				TCPState new_state(seq_num, SYN_SENT, 1);
				ConnectionToStateMapping<TCPState> new_mapping(req.connection, timeout, new_state, false);
				//send status response immediately
				SockRequestResponse status(STATUS, req.connection, ebptr, 0, EOK);
				MinetSend(sock, status);
				//make a SYN packet
				flags = 0;
				SET_SYN(flags);
				Packet send_p = make_packet(req.connection, seq_num, 0, flags, 0, ebptr, new_state.TCP_BUFFER_SIZE);
				//send SYN packet
				packet_send( mux, send_p);
				sleep(1);
				packet_send(mux, send_p);
				clist.push_back(new_mapping);
				cs = clist.FindMatching(req.connection);
				#ifdef DEBUGGING
				cerr << "new mapping " << new_mapping << "\n";
				#endif
			}
			else if (cs == clist.end()) 
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
					else if(req.type==WRITE) {
						SockRequestResponse status(STATUS, req.connection, ebptr, 0, ENOMATCH);
						MinetSend(sock, status);
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
						Packet send_p = make_packet(req.connection, (*cs).state.GetLastSent(), (*cs).state.GetLastAcked(), fptr, 0, ebptr, (*cs).state.GetRwnd());
						packet_send(mux, send_p);
						cerr << "Recieved SEND from LISTEN state";
					}
				} break;
				

				
				case ESTABLISHED: {
					if(req.type==WRITE){
						cerr << "write request in established state\n";
						//socket is sending us information to put into the send buffer
						//if send buffer is full, send EBUFSPACE
						if( (*cs).state.N==0 ) {
							SockRequestResponse status(STATUS, req.connection, ebptr, 0, EBUF_SPACE);
							MinetSend(sock, status);
						}
						//else buffer as much of write as we can
						else {
							Buffer new_buff;
							if( req.data.GetSize() > (*cs).state.N ) {
								new_buff = req.data.ExtractFront((*cs).state.N);
								(*cs).state.N = 0;
								SockRequestResponse status(STATUS, req.connection, ebptr, (*cs).state.N, EOK);
								MinetSend(sock, status);
							}
							else {
								new_buff = req.data;
								(*cs).state.N = (*cs).state.N - req.data.GetSize();
								SockRequestResponse status(STATUS, req.connection, ebptr, req.data.GetSize(), EOK);
								MinetSend(sock, status);
							}
							(*cs).state.SendBuffer.AddBack(new_buff);
							cerr << "Our send buffer: " << (*cs).state.SendBuffer << endl;
						}
					}
					if(req.type==STATUS){
						cerr << "status request in established state with byte size: " << req.bytes << "\n";
						list<write_record>:: iterator wr_it;
						bool exit=false;
						if (req.bytes!=0) {	//want to exclude non-write response status requests
							for(wr_it = wr_list.begin(); exit==false && wr_it!=wr_list.end(); wr_it++) 
							{
								if((*wr_it).connection.Matches(req.connection)) { 
									exit=true;
									if((*wr_it).size_total == req.bytes) { //all of the data we sent to the socket was accepted 
										(*cs).state.rwnd+=(*wr_it).size_total;
										(*cs).state.RecvBuffer.Erase(0, (*wr_it).size_total); //should we use extract front here?  does extract front erase or only fetch
										wr_list.erase(wr_it); //can delete record
									}
									//else some data needs to be resent
									else if((*wr_it).size_total > req.bytes) {
										(*wr_it).write_buffer.Erase(0, req.bytes);
										SockRequestResponse write(WRITE, req.connection, (*wr_it).write_buffer, (*wr_it).write_buffer.GetSize(), EOK);
										MinetSend(sock, write);
										wr_list.push_back(*wr_it); //move to the back of the list
										wr_list.erase(wr_it);
									}
								}
							}
						}
					}
					if(req.type==CLOSE){
						//send FIN
						cerr << "request close in established state\n";
						SockRequestResponse status(STATUS, req.connection, ebptr, req.data.GetSize(), EOK);
						MinetSend(sock, status);
						SET_FIN(flags);
						Packet send_p = make_packet(req.connection, (*cs).state.GetLastAcked(), (*cs).state.GetLastRecvd(), fptr, 0, ebptr, (*cs).state.GetRwnd()); 
						packet_send(mux, send_p);
					}
				} break;
				
				case CLOSE_WAIT:
				case SYN_RCVD:
				case SYN_SENT: {
					if(req.type==CLOSE){
						SET_FIN(flags);
						sptr = c_mapping.state.GetLastSent();
						sptr++;
						(*cs).state.SetLastSent(sptr);
						aptr = c_mapping.state.GetLastRecvd();
						(*cs).state.SetLastRecvd(aptr);
						Packet send_p = make_packet(req.connection, sptr, aptr+1, fptr, 0, ebptr, (*cs).state.GetRwnd()); //needs edit -> can't use sptr or aptr in sock section (not set)
						packet_send( mux, send_p);
					}
			} break;
				
			}//END TCPState switch
	    } //ENDIF event.handle == sock
	}

	if (event.eventtype == MinetEvent::Timeout) {
	    // timeout ! probably need to resend some packets
	    //cerr << "Timeout!\n";
		ConnectionList<TCPState>::iterator cs;
		unsigned int send_datasize;
		Buffer send_buffer;
		for(cs = clist.begin(); cs!=clist.end(); cs++) {
			if ((*cs).timeout<Time() && (*cs).state.SendBuffer.GetSize()>0) { //if we have data to send and timed out
				//first check that this connection has timed out (still need to write timeout handling)
				//resend earliest unacked packet
				if ((*cs).state.SendBuffer.GetSize()>0) { //figure out how much data we can send: if we have data
					if ((*cs).state.SendBuffer.GetSize()>TCP_MAXIMUM_SEGMENT_SIZE)
						send_datasize = TCP_MAXIMUM_SEGMENT_SIZE;
					else  //send whole buffer
						send_datasize = (*cs).state.SendBuffer.GetSize();
					send_buffer = (*cs).state.SendBuffer.ExtractFront(send_datasize);	
					cerr << "We're sending this buffer: " << send_buffer;
				}
				//don't need to update state, probably need to reset timeout
				(*cs).timeout = Time()+5;
				//send packet
				(*cs).state.SetLastSent(send_datasize+(*cs).state.GetLastSent());
				SET_ACK(flags);
				Packet send_p = make_packet((*cs).connection, (*cs).state.GetLastAcked()+1, (*cs).state.GetLastRecvd(), flags, send_datasize, send_buffer, (*cs).state.GetRwnd());	
				packet_send(mux, send_p);
			}
		}
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

Packet make_packet(const Connection c, const unsigned int &seqnum, const unsigned int &acknum, unsigned char &flags, int data_size, Buffer &data_buffer, unsigned short rwnd) {
	//Packet send_p(data_buffer, data_size);
	Packet send_p(data_buffer);
	IPHeader iph;
	TCPHeader tcph;
	
	iph.SetProtocol(IP_PROTO_TCP);
	iph.SetSourceIP(c.src);
	iph.SetDestIP(c.dest);
	iph.SetTotalLength(data_size + IP_HEADER_BASE_LENGTH + TCP_HEADER_BASE_LENGTH);
	send_p.PushFrontHeader(iph);

	tcph.SetSourcePort(c.srcport, send_p);
	tcph.SetDestPort(c.destport, send_p);
	tcph.SetHeaderLen(5, send_p); 
	tcph.SetFlags(flags, send_p);
	tcph.SetSeqNum(seqnum, send_p);
	tcph.SetAckNum(acknum, send_p);
	tcph.SetWinSize(rwnd, send_p); 
	tcph.SetUrgentPtr(0, send_p);
	send_p.PushBackHeader(tcph);
	#ifdef DEBUGGING1
	unsigned short recv_datasize = 0;
	recv_datasize = recv_datasize - (TCP_HEADER_BASE_LENGTH + IP_HEADER_BASE_LENGTH);
	Buffer &recv_buffer = send_p.GetPayload().ExtractFront(recv_datasize);
	cerr<<"\n\n===================SEND PACKET INFO=============";
	cerr << "\n***Packet IP header*** " << iph << "\n\n***TCP header*** " << tcph;
	cerr << "\n\n***Buffer*** " << recv_buffer << "\n";
	cerr<<"=====================END SEND PACKET INFO===========\n\n";
	#endif
	cerr << "Made packet, returning...\n";
	return send_p;
}
