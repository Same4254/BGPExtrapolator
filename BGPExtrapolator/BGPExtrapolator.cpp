// BGPExtrapolator.cpp : Defines the entry point for the application.

#include "BGPExtrapolator.h"

/***************************** PROPAGATION ******************************/

void extrapolator_propagate(GraphHandle& gHandle) {
	// ************ Propagate Up ************//

	// start at the second rank because the first has no customers
	for (int i = 1; i < gHandle.as_process_info_by_rank.size(); i++)
		for (auto& provider : gHandle.as_process_info_by_rank[i])
			extrapolator_as_process_customer_announcements(*provider, gHandle.as_id_to_customers[provider->asn_id]);

	for (int i = 0; i < gHandle.as_process_info_by_rank.size(); i++)
		for (auto& as : gHandle.as_process_info_by_rank[i])
			extrapolator_as_process_peer_announcements(*as, gHandle.as_id_to_peers[as->asn_id]);

	// ************ Propagate Down ************//
	for (int i = gHandle.as_process_info_by_rank.size() - 2; i >= 0; i--)
		for (auto& customer : gHandle.as_process_info_by_rank[i])
			extrapolator_as_process_peer_announcements(*customer, gHandle.as_id_to_peers[customer->asn_id]);
}

bool extrapolator_path_contains_loop(const std::vector<ASN>& as_path) {
	ASN previous = 0;
	bool containsLoop = false;

	for (int i = 0; i < (as_path.size() - 1) && !containsLoop; i++) {
		previous = as_path[i];
		for (int j = i + 1; j < as_path.size() && !containsLoop; j++) {
			containsLoop = as_path[i] == as_path[j] && as_path[j] != previous;
			previous = as_path[j];
		}
	}

	return containsLoop;
}

int main() {
	//ASProcessInfo reciever;
	//ASProcessInfo customer1, customer2;

	//reciever.loc_rib.push_back (Announcement{ 0, 0, Prefix {0, 0, 0, 0}, Priority {0, 250, 1, 1}, 0 });
	////customer1.loc_rib.push_back(Announcement{ 0, 0, Prefix {0, 0, 0, 0}, Priority {0, 254, 1, 0}, 0 });
	//customer2.loc_rib.push_back(Announcement{ 0, 0, Prefix {0, 0, 0, 0}, Priority {0, 253, 1, 0}, 0 });

	//std::vector<ASProcessInfo*> customers = { &customer1, &customer2 };

	//extrapolator_as_process_customer_announcements(reciever, customers);

	return 0;
}
