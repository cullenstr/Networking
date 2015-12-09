#ifndef _table
#define _table

#include <iostream>
#include <map>
#include <deque>
#include "link.h"

using namespace std;

struct TopoLink {
    TopoLink(): cost(-1), age(0) {}

    TopoLink(const TopoLink & rhs) {
        *this = rhs;
    }

    TopoLink & operator=(const TopoLink & rhs) {
        this->cost = rhs.cost;
        this->age = rhs.age;

        return *this;
    }

    int cost;
    int age;
};

// Students should write this class
class Table {
    private:
    public:
        Table();
        Table(const Table &);
        Table & operator=(const Table &);
		map < int, map < int, TopoLink > > topo;
        ostream & Print(ostream &os) const;

        // Anything else you need
        #if defined(LINKSTATE)
        typedef pair<int, TopoLink> src_pair;
        map <int, double> ls_records;  //record src, seq# of recieved messages
        map <int, map <int, TopoLink > > unused; //record nodes not in N': src, <dest, link>
        map <int, src_pair > next_hop; //record next hop and cost as follows : dest, <src, total cost>
        bool initialized;
		int seq_num;
        #endif

        #if defined(DISTANCEVECTOR)
        map <int, TopoLink> distance;
        map <int, TopoLink> neighbors;
        map <int, int> hop_map;
        #endif
};

inline ostream & operator<<(ostream &os, const Table & t) { return t.Print(os);}


#endif
