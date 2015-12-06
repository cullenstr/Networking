#include "distancevector.h"
#define UNUSED(x) (void)(x) //gets rid of annoying compiler warning

DistanceVector::DistanceVector(unsigned n, SimulationContext* c, double b, double l) :
    Node(n, c, b, l)
{}

DistanceVector::DistanceVector(const DistanceVector & rhs) :
    Node(rhs)
{
    *this = rhs;
}

DistanceVector & DistanceVector::operator=(const DistanceVector & rhs) {
    Node::operator=(rhs);
    return *this;
}

DistanceVector::~DistanceVector() {}

/** Write the following functions.  They currently have dummy implementations **/
void DistanceVector::LinkHasBeenUpdated(Link* l) 
{
    cerr << *this << ": Link Update: " << *l << endl;
	int dest = (*l).GetDest();
	int latency = (*l).GetLatency();

	// update costs
	routing_table.neighbors[dest].cost = latency;
	routing_table.topo[dest][dest].cost = 0;
	routing_table.distance[dest].cost = -1;

	bool isDifferent = updateDV();
	if(isDifferent)
		SendToNeighbors(new RoutingMessage(number, routing_table.distance));
}

void DistanceVector::ProcessIncomingRoutingMessage(RoutingMessage *m) 
{	
	cerr << *this << " got a routing message: " << *m << " (ignored)" << endl;
	int source = m->src;
	map <int, TopoLink> vectors = m->vects;
	map <int, TopoLink>::const_iterator msg_it;
	
	//update distances
	for(msg_it=vectors.begin(); msg_it!=vectors.end(); msg_it++)
	{
		routing_table.topo[source][msg_it->first].cost = msg_it->second.cost;
		int new_node;
		UNUSED(new_node);
		new_node = routing_table.distance[msg_it->first].cost; //needs to be instantiated
	}
	
	bool isDifferent = updateDV();
	if(isDifferent)
		SendToNeighbors(new RoutingMessage(number, routing_table.distance));
}

void DistanceVector::TimeOut() {
    cerr << *this << " got a timeout: (ignored)" << endl;
}

Node* DistanceVector::GetNextHop(Node *destination) 
{ 
	unsigned n = routing_table.hop_map[(*destination).GetNumber()];
	cout << "next hop from " << number << " to " << (*destination).GetNumber() << " ==> " << n << "\n";

	Node *r_node;
	deque<Node*> neighbors = *GetNeighbors();
	deque<Node*>::iterator neighbor_it;
	
	//iterate neighbors to get next hop
	for(neighbor_it = neighbors.begin(); neighbor_it!=neighbors.end(); neighbor_it++)
	{
		if((*neighbor_it)->GetNumber() == n)
			r_node = new Node((*neighbor_it)->GetNumber(), context, (*neighbor_it)->GetBW(), (*neighbor_it)->GetLatency());
	}
	return r_node;
}

Table* DistanceVector::GetRoutingTable() 
{
    Table* t = new Table(routing_table);
    return t;
}

ostream & DistanceVector::Print(ostream &os) const 
{ 
    Node::Print(os);
    return os;
}

bool DistanceVector::updateDV() //calculate dist vectors and next hop for each dest
{
	map<int, TopoLink>::const_iterator dist_it;
	map<int, TopoLink>::const_iterator neighbor_it;
	bool isDifferent = false;
	
	for(dist_it=routing_table.distance.begin(); dist_it!=routing_table.distance.end(); dist_it++)
	{
		unsigned current = dist_it->first;
		if(current==number) //we are current node
			routing_table.distance[current].cost = 0;
		else
		{
			int neighbor = -1, dist = -1, min_neighbor = -1, min_dist = -1;
			bool isFirst = true;
			
			//iterate neighbors to find min cost+distvector to dest
			for(neighbor_it = routing_table.neighbors.begin(); neighbor_it != routing_table.neighbors.end(); neighbor_it++)
			{
				neighbor = neighbor_it->first;
				if(routing_table.neighbors[neighbor].cost != -1 && routing_table.topo[neighbor][current].cost != -1) //is neighbor
				{
					dist = routing_table.neighbors[neighbor].cost + routing_table.topo[neighbor][current].cost;
					if(isFirst || dist<min_dist) //new mins
					{
						min_dist = dist;
						min_neighbor = neighbor;
						isFirst = false;
					}
				}
			}

			if(!isFirst && min_dist != routing_table.distance[current].cost) //distance vector isDifferent
			{
				routing_table.distance[current].cost = min_dist;
				routing_table.hop_map[current] = min_neighbor;
				isDifferent = true;
			}
		}
	}
	return isDifferent;
}
