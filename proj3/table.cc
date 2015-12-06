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
  os << "LinkState Table()";
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
