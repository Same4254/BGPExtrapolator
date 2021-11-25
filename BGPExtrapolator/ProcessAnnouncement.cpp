#include "BGPExtrapolator.h"

void extrapolator_as_process_customer_announcements(ASProcessInfo& reciever, std::vector<ASProcessInfo*>& customers) {
	Priority customer_priority;
	customer_priority.allFields = 0;
	customer_priority.relationship = CUSTOMER_RELATIONSHIP_PRIORITY;

	for (ASProcessInfo* customer : customers) {
		for (int i = 0; i < reciever.loc_rib.size(); i++) {
			customer_priority.pathLength = customer->loc_rib[i].priority.pathLength - 1;//See priority struct for why this is subtraction

			if (customer_priority.allFields >= reciever.loc_rib[i].priority.allFields) {
				reciever.loc_rib[i] = customer->loc_rib[i];
				reciever.loc_rib[i].received_from_asn = customer->asn;
				reciever.loc_rib[i].priority = customer_priority;
			}
		}
	}
}

void extrapolator_as_process_peer_announcements(ASProcessInfo& reciever, std::vector<ASProcessInfo*>& peers) {
	Priority peer_priority;
	peer_priority.allFields = 0;
	peer_priority.relationship = PEER_RELATIONSHIP_PRIORITY;

	for (ASProcessInfo* peer : peers) {
		for (int i = 0; i < reciever.loc_rib.size(); i++) {
			peer_priority.pathLength = peer->loc_rib[i].priority.pathLength - 1;

			if (peer_priority.allFields >= reciever.loc_rib[i].priority.allFields) {
				reciever.loc_rib[i] = peer->loc_rib[i];
				reciever.loc_rib[i].received_from_asn = peer->asn;
				reciever.loc_rib[i].priority = peer_priority;
			}
		}
	}
}

void extrapolator_as_process_provider_announcements(ASProcessInfo& reciever, std::vector<ASProcessInfo*>& providers) {
	Priority provider_priority;
	provider_priority.allFields = 0;

	for (ASProcessInfo* provider : providers) {
		for (int i = 0; i < reciever.loc_rib.size(); i++) {
			provider_priority.pathLength = provider->loc_rib[i].priority.pathLength - 1;
			provider_priority.relationship = provider->loc_rib[i].priority.relationship;

			if (provider_priority.allFields >= reciever.loc_rib[i].priority.allFields) {
				reciever.loc_rib[i] = provider->loc_rib[i];
				reciever.loc_rib[i].received_from_asn = provider->asn;
				reciever.loc_rib[i].priority = provider_priority;
			}
		}
	}
}