// BGPExtrapolator.cpp : Defines the entry point for the application.

#include "BGPExtrapolator.h"

int main() {
	std::vector<ASN> path = BGPExtrapolator::parse_ASN_list("{1,2,3}");

	std::cout << "{ ";
	for (int i = 0; i < path.size(); i++) {
		if (i == path.size() - 1)
			std::cout << path[i] << " ";
		else
			std::cout << path[i] << ", ";
	}
	std::cout << "}" << std::endl;

	return 0;
}