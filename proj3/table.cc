#include "table.h"

Table::Table() {
    topo.clear();
}

Table::Table(const Table & rhs) {
    *this = rhs;
}

Table & Table::operator=(const Table & rhs) {
    /* For now,  Change if you add more data members to the class */
    topo = rhs.topo;
	#if defined(LINKSTATE)
	ls_records = rhs.ls_records; 
    unused = rhs.unused; 
    next_hop = rhs.next_hop;
    initialized = rhs.initialized;
    seq_num = rhs.seq_num;
    seq_num = 0;
    initialized = false;
	#endif
    return *this;
}

#if defined(GENERIC)
ostream & Table::Print(ostream &os) const
{
  os << "Generic Table()";
  return os;
}
#endif

#if defined(LINKSTATE)
ostream & Table::Print(ostream &os) const
{
  os << "\n\n*********** Linkstate Table **********";
  os << "\n==== Destination\t:\tNext Hop\t:\tDistance ====\n";
  map <int, pair<int, TopoLink> >::const_iterator iter;
  for(iter=this->next_hop.begin(); iter!=this->next_hop.end(); iter++)
  {
	  if(iter->second.second.cost!=-1) //if a valid link
		os << "\t" << iter->first << "\t\t:\t" << iter->second.first << "\t\t:\t" << "D(" << iter->first << ")=" << iter->second.second.cost << "\t\n";
  }
  os << "\n\n*********** End Linkstate Table **********";
  return os;
}
#endif

#if defined(DISTANCEVECTOR)
ostream & Table::Print(ostream &os) const
{
	os << "\n\n*********** DV Table **********";
	os << "\n==== Distances ====\n";
	for(map <int, TopoLink>::const_iterator iter = this->distance.begin(); iter != this->distance.end(); iter++)
		os << "D(" <<	iter->first << ") ==> " << iter->second.cost << "\n";
	
	os << "\n\n==== Neighbors ====\n";
	for(map <int, TopoLink>::const_iterator iter = this->neighbors.begin(); iter != this->neighbors.end(); iter++)
		os << "D(" <<	iter->first << ") ==> " << iter->second.cost << "\n";

	os << "\n\n====== Hops ======\n";
	for(map <int, int>::const_iterator iter = this->hop_map.begin(); iter != this->hop_map.end(); iter++)
		os << "next hop to " << iter->first << " ==> " << iter->second << endl;
	
	os << "\n********* END DV Table *******\n\n";
	return os;
}
#endif
