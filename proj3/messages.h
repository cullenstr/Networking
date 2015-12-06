#ifndef _messages
#define _messages

#include <iostream>
#include "node.h"
#include "link.h"

class Node;

struct RoutingMessage {
    RoutingMessage();
    RoutingMessage(const RoutingMessage &rhs);
    RoutingMessage &operator=(const RoutingMessage &rhs);

    ostream & Print(ostream &os) const;

    // Anything else you need
    #if defined(LINKSTATE)
    #endif
	
	#if defined(DISTANCEVECTOR)
    RoutingMessage(int s,  map <int, TopoLink> v);
	map <int, TopoLink> vects;
    int src;
    #endif
	};

inline ostream & operator<<(ostream &os, const RoutingMessage & m) { return m.Print(os);}

#endif
