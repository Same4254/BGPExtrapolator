#include "BGPExtrapolator.h"

namespace BGPExtrapolator {
	void as_process_customer_announcements(ASProcessInfo& reciever, std::vector<ASProcessInfo*>& customers) {
		Priority customer_priority;
		customer_priority.allFields = 0;
		customer_priority.relationship = RELATIONSHIP_PRIORITY_CUSTOMER;

		for (ASProcessInfo* customer : customers) {
			for (int i = 0; i < reciever.loc_rib.size(); i++) {
				customer_priority.pathLength = customer->loc_rib[i].priority.pathLength - 1;//See priority struct for why this is subtraction

				as_process_announcement(reciever, customer->asn_id, i, customer->loc_rib[i], customer_priority);
			}
		}
	}

	void as_process_peer_announcements(ASProcessInfo& reciever, std::vector<ASProcessInfo*>& peers) {
		Priority peer_priority;
		peer_priority.allFields = 0;
		peer_priority.relationship = RELATIONSHIP_PRIORITY_PEER;

		for (ASProcessInfo* peer : peers) {
			for (int i = 0; i < reciever.loc_rib.size(); i++) {
				peer_priority.pathLength = peer->loc_rib[i].priority.pathLength - 1;

				as_process_announcement(reciever, peer->asn_id, i, peer->loc_rib[i], peer_priority);
			}
		}
	}

	void as_process_provider_announcements(ASProcessInfo& reciever, std::vector<ASProcessInfo*>& providers) {
		Priority provider_priority;
		provider_priority.allFields = 0;

		for (ASProcessInfo* provider : providers) {
			for (int i = 0; i < reciever.loc_rib.size(); i++) {
				provider_priority.pathLength = provider->loc_rib[i].priority.pathLength - 1;
				provider_priority.relationship = provider->loc_rib[i].priority.relationship;

				as_process_announcement(reciever, provider->asn_id, i, provider->loc_rib[i], provider_priority);
			}
		}
	}

	void as_process_announcement(ASProcessInfo& reciever, const ASN_ID& recieved_from_id, const uint32_t& prefix_block_id, const AnnouncementDynamicData& other_announcement, const Priority& temp_priority) {
		if (temp_priority.allFields >= reciever.loc_rib[prefix_block_id].priority.allFields) {
			reciever.loc_rib[prefix_block_id] = other_announcement;
			reciever.loc_rib[prefix_block_id].received_from_id = recieved_from_id;
			reciever.loc_rib[prefix_block_id].priority = temp_priority;
		}
	}

	void as_process_announcement_random_tiebrake(ASProcessInfo& reciever, const ASN_ID& recieved_from_id, const uint32_t& prefix_block_id, const AnnouncementDynamicData& other_announcement, const Priority& temp_priority, bool tiebrake_keep_original_ann) {
		if (temp_priority.allFields > reciever.loc_rib[prefix_block_id].priority.allFields || (!tiebrake_keep_original_ann && temp_priority.allFields == reciever.loc_rib[prefix_block_id].priority.allFields)) {
			reciever.loc_rib[prefix_block_id] = other_announcement;
			reciever.loc_rib[prefix_block_id].received_from_id = recieved_from_id;
			reciever.loc_rib[prefix_block_id].priority = temp_priority;
		}
	}
}