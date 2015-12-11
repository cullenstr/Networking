#include "linkstate.h"

LinkState::LinkState(unsigned n, SimulationContext* c, double b, double l) :
    Node(n, c, b, l)
{}

LinkState::LinkState(const LinkState & rhs) :
    Node(rhs)
{
    *this = rhs;
}

LinkState & LinkState::operator=(const LinkState & rhs) {
    Node::operator=(rhs);
    return *this;
}

LinkState::~LinkState() {}


/** Write the following functions.  They currently have dummy implementations **/
void LinkState::LinkHasBeenUpdated(Link* l) {
   if( routing_table.initialized == false )
		Initialize();
	cerr << (*this).GetNumber() << ": Link Update: " << *l << endl;
	//find cost change
	int cost_diff = 0;
	int dest = (*l).GetDest();
	int latency = (*l).GetLatency();
	int src = (*this).GetNumber();
	//replace in topo -> [n][l.dst].cost = l.cost
	cost_diff = routing_table.topo[src][dest].cost - latency;
	routing_table.topo[src][dest].cost = latency;
	//update all costs in next hop based on change -> iterate down next_hop, changing totoal costs
	map<int, pair<int, TopoLink> >::const_iterator hop_it;
	for(hop_it=routing_table.next_hop.begin(); hop_it!=routing_table.next_hop.end(); hop_it++)
	{
		cerr << "in_loop";
		pair<int, TopoLink> hop_pair = hop_it->second;
		//if is a valid entry and if goes through the changed link, update cost
		if( hop_pair.second.cost!=-1 && hop_pair.first==dest )
		{
			hop_pair.second.cost -= cost_diff;
			int mod = hop_it->first;
			routing_table.next_hop[mod] = hop_pair;
		}
	}
	//send updated advertisement
	routing_table.seq_num++;
	SendToNeighbors(new RoutingMessage(src, routing_table.seq_num, routing_table.next_hop));
	return;
}


void LinkState::ProcessIncomingRoutingMessage(RoutingMessage *m) {
    //cerr << *this << " got a routing message: " << *m << endl;
	if( routing_table.initialized == false )
		Initialize();
	
	//extract ls_advertisement information
	int msg_source = m->source;
	double seq_num = m->seq_num;
	map <int, pair<int, TopoLink> > paths = m->paths;
	map <int, pair<int, TopoLink> >::const_iterator paths_it;
	map <int, map< int, TopoLink> >::const_iterator unused_it;
	//check if this is a relevant message: is the seq_num higher than our last recorded entry?
	if( (unsigned int)msg_source!=(*this).GetNumber() && routing_table.ls_records[msg_source] < seq_num) //pretty sure the value will be initialized to 0 if this is the first msg
	{
		cerr << (*this).GetNumber() << " got a routing message: " << *m << endl;
		//first flood to neighbors
		SendToNeighbors(m);
		//update our records
		routing_table.ls_records[msg_source] = seq_num;
		//update our tables to reflect new values.  Start by adding to candidates.
		for(paths_it=paths.begin(); paths_it!=paths.end(); paths_it++)
		{
			if(paths_it->second.second.cost != -1) //if this is a valid entry
			{
				cerr << "\nadding paths to candidate: D(" << paths_it->first << ")=" <<paths_it->second.second.cost;
				routing_table.unused[msg_source][paths_it->first] = paths_it->second.second;
			}
		}
		
		//iterate through candidates, adding candidate with lowest cost to maps until remaining candidates not in next_hop/candidates empty
		bool finished = false;
		bool altered = false;
		while( !finished) {
			int candidate = -1;
			int lowest_cost = -1;
			//cerr << "In while loop: ";
			for(unused_it=routing_table.unused.begin(); unused_it!=routing_table.unused.end(); unused_it++)
			{
				if(routing_table.next_hop[unused_it->first].second.cost!=-1 && (lowest_cost==-1 || routing_table.next_hop[unused_it->first].second.cost<lowest_cost))
				{
					candidate = unused_it->first;
					lowest_cost = routing_table.next_hop[unused_it->first].second.cost;
					//cerr << "\nnew candidate: " << candidate << " new lowest cost: " << lowest_cost;
				}
				cerr << lowest_cost;
			}
			if (lowest_cost==-1)
			{
				//cerr << "\nExiting while loop.\n";
				finished=true;
			}
			else {
				//Add to maps: every node reachable by candidate if shortest path
				//current_cost[reachable] < cost_to_candidate+cost_candidate_to_reachable
				//cerr << "\nAdding candidate " << candidate << " to table\n";
				for(unused_it=routing_table.unused.begin(); unused_it->first!=candidate; unused_it++) {}
				map<int, TopoLink>::const_iterator col; //iterates over the nodes each unused node links to
				for(col=unused_it->second.begin(); col!=unused_it->second.end(); col++)
				{
					//cerr << "candidate can get to " << (*col).first << " with a cost of " << (*col).second.cost;
					//cerr << "\n our current cost is: " << routing_table.next_hop[col->first].second.cost;
					if ((*col).second.cost!=-1 && (routing_table.next_hop[col->first].second.cost==-1 || (*col).second.cost+lowest_cost < routing_table.next_hop[col->first].second.cost)) //if lowest cost route
					{
						//cerr << "Put it in the table!";
						altered = true; //we will have to remake the next_hop table before sending our message
						routing_table.next_hop[col->first].second.cost = col->second.cost+lowest_cost;
						//don't need to update the next hop value here; it'll be fixed when we remake the table
						routing_table.topo[candidate][col->first].cost = col->second.cost; //make a link to the node from the candidate
						map<int, map< int, TopoLink> >::const_iterator topo_it; //iterate over source
						for( topo_it=routing_table.topo.begin(); topo_it!=routing_table.topo.end(); topo_it++ ) //find the old path to the candidate and remove it
						{ 
							if(topo_it->second.find(col->first)!=topo_it->second.end()); //if we found an old path
						}
					}
				}
				//remove candidate from unused
				routing_table.unused.erase(candidate); //erase by key value the candidate we've added to topo
			}
		}	
		if(altered) {
			//cerr << "Link altered : see new routing table ";
			//update next_hop table
			map<int, map<int, TopoLink> >::const_iterator node_row = routing_table.topo.find((*this).GetNumber());
			map<int, TopoLink>::const_iterator child_it;
			for(child_it=node_row->second.begin(); child_it!=node_row->second.end(); child_it++) {
				if(child_it->second.cost!=-1) {
					//cerr << "Calling recursive build on node " << child_it->first << " first hop " << child_it->first << " current cost " << child_it->second.cost;
					RecursiveBuild(child_it->first, child_it->first, child_it->second);
				}
			}
			//send updated advertisement
			routing_table.seq_num++;
			cerr << *this;
			SendToNeighbors(new RoutingMessage((*this).GetNumber(), routing_table.seq_num, routing_table.next_hop));
		}
	}
	return;
}

void LinkState::TimeOut() {
    if( routing_table.initialized == false )
		Initialize();
	cerr << *this << " got a timeout: (ignored)" << endl;
}

Node* LinkState::GetNextHop(Node *destination) { 
    if( routing_table.initialized == false )
		Initialize();
	//get neighbor number from next_hop mapping
	int n = routing_table.next_hop[(*destination).GetNumber()].first;
	
	Node *r_node;
	deque<Node*> neighbors = *GetNeighbors();
	deque<Node*>::iterator neighbor_it;
	
	//iterate neighbors to get next hop
	for(neighbor_it = neighbors.begin(); neighbor_it!=neighbors.end(); neighbor_it++)
	{
		if((*neighbor_it)->GetNumber() == (unsigned int)n)
			r_node = new Node((*neighbor_it)->GetNumber(), context, (*neighbor_it)->GetBW(), (*neighbor_it)->GetLatency());
	}
	return r_node;
}

Table* LinkState::GetRoutingTable() {
    if( routing_table.initialized == false )
		Initialize();
	Table* t = new Table(routing_table);
    return t;
}

ostream & LinkState::Print(ostream &os) const { 
	Node::Print(os);
    return os;
}

void LinkState::Initialize() {
    //initialize table
    deque<Link*> links = *GetOutgoingLinks();
	deque<Node*> neighbors = *GetNeighbors();
	deque<Link*>::iterator it_l;
	deque<Node*>::iterator it_n;
	int src_node = (*this).GetNumber();
	//not sure if this is necessary.  Does GetOutgoingLinks yield nodes that aren't neighbors?
	for (it_n = neighbors.begin(); it_n != neighbors.end(); it_n++) {
		for (it_l = links.begin(); it_l != links.end(); it_l++) {
			if ((*it_n)->GetNumber() == (*it_l)->GetDest()) {
				//map Node#, <GetDest, GetLatency> into topo
				routing_table.topo[src_node][(*it_l)->GetDest()].cost = (*it_l)->GetLatency();
				//maka pair <GetDest, GetLatency>
				//map GetDest to pair in next_hop
				TopoLink new_link;
				new_link.cost=(*it_l)->GetLatency();
				routing_table.next_hop[(*it_l)->GetDest()] = make_pair((*it_l)->GetDest(), new_link);
			}
		}
	}
	routing_table.initialized = true;
	SendToNeighbors(new RoutingMessage((*this).GetNumber(), routing_table.seq_num, routing_table.next_hop)); //we're sending some unnecessary information, but we'll just ignore that on the recieving end
}

//method to recalculate the first hops and total costs in next_hop after updating topo
int LinkState::RecursiveBuild(int node, int next_hop, TopoLink total_cost) {
	routing_table.next_hop[node] = make_pair(next_hop, total_cost); //update routing table entry
	//cerr << "Next hop updated! Destination " << node << " cost " << total_cost.cost << " next hop\n";
	map<int, map<int, TopoLink> >::const_iterator node_row;
	node_row = routing_table.topo.find(node);
	map<int, TopoLink>::const_iterator child_it;
	for(child_it=node_row->second.begin(); child_it!=node_row->second.end(); child_it++) {
		if(child_it->second.cost!=-1) //valid link to child node
		{
			TopoLink new_cost = total_cost;
			new_cost.cost += child_it->second.cost;
			//RecursiveBuild(child_it->first, next_hop, new_cost); Ran out of time debugging, had to disable recursive function because it was segfaulting
		}
	}
	return 0;
}
