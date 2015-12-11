#include "messages.h"

RoutingMessage::RoutingMessage()
{}

RoutingMessage::RoutingMessage(const RoutingMessage &rhs) {
	*this = rhs;
}

RoutingMessage & RoutingMessage::operator=(const RoutingMessage & rhs) {
	/* For now.  Change if you add data members to the struct */
	return *this;
}

#if defined(GENERIC)
ostream &RoutingMessage::Print(ostream &os) const
{
	os << "Generic RoutingMessage()";
	return os;
}
#endif

#if defined(LINKSTATE)
RoutingMessage::RoutingMessage(int src, double seq, map<int, pair<int, TopoLink> > p)
{
	paths = p;
	source  = src;
	seq_num = seq;
}


ostream &RoutingMessage::Print(ostream &os) const
{
	os << "LinkState RoutingMessage()\n";
	os << "Source: " << source << " Sequence Number: " << seq_num;
	map <int, pair<int, TopoLink> >::const_iterator iter;
	for(iter=this->paths.begin(); iter!=this->paths.end(); iter++)
	{
		if(iter->second.second.cost!=-1) //if a valid link
		os << "\ndestination: " << iter->first << " cost: " << iter->second.second.cost;
	}
	return os;
}
#endif

#if defined(DISTANCEVECTOR)
RoutingMessage::RoutingMessage(int s, map <int, TopoLink> v)
{
    src = s; 
	vects = v;
}

ostream &RoutingMessage::Print(ostream &os) const
{
    os << "DistanceVector RoutingMessage()";
    return os;
}
#endif
