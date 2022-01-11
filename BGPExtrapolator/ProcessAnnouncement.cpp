#include "BGPExtrapolator.h"

namespace BGPExtrapolator {
	void AnnouncementDynamicData::fill(const ASN_ID& received_from_id, const ASN& received_from_asn, const Priority& priority, AnnouncementStaticData* static_data) {
		this->received_from_id = received_from_id;
		this->received_from_asn = received_from_asn;
		this->priority = priority;
		this->static_data = static_data;
	}

	void AnnouncementDynamicData::reset() {
		this->received_from_id = 0;
		this->received_from_asn = 0;
		this->priority.allFields = 0;
		this->static_data = nullptr;
	}

	void as_process_customer_announcements(ASProcessInfo& reciever, std::vector<ASProcessInfo*>& customers, bool timestamp_tiebrake, bool prefer_new_timestamp, bool random_tiebraking) {
		Priority customer_priority;
		customer_priority.allFields = 0;
		customer_priority.relationship = RELATIONSHIP_PRIORITY_CUSTOMER;

		for (ASProcessInfo* customer : customers) {
			for (int i = 0; i < reciever.loc_rib.size(); i++) {
				if (customer->loc_rib[i].priority.allFields == 0)
					continue;

				customer_priority.pathLength = customer->loc_rib[i].priority.pathLength - 1;//See priority struct for why this is subtraction

				as_process_announcement(reciever, *customer, customer->asn_id, i, customer->loc_rib[i], customer_priority, timestamp_tiebrake, prefer_new_timestamp, random_tiebraking);
			}
		}
	}

	void as_process_peer_announcements(ASProcessInfo& reciever, std::vector<ASProcessInfo*>& peers, bool timestamp_tiebrake, bool prefer_new_timestamp, bool random_tiebraking) {
		Priority peer_priority;
		peer_priority.allFields = 0;
		peer_priority.relationship = RELATIONSHIP_PRIORITY_PEER;

		for (ASProcessInfo* peer : peers) {
			for (int i = 0; i < reciever.loc_rib.size(); i++) {
				if (peer->loc_rib[i].priority.allFields == 0)
					continue;

				peer_priority.pathLength = peer->loc_rib[i].priority.pathLength - 1;

				as_process_announcement(reciever, *peer, peer->asn_id, i, peer->loc_rib[i], peer_priority, timestamp_tiebrake, prefer_new_timestamp, random_tiebraking);
			}
		}
	}

	void as_process_provider_announcements(ASProcessInfo& reciever, std::vector<ASProcessInfo*>& providers, bool timestamp_tiebrake, bool prefer_new_timestamp, bool random_tiebraking) {
		Priority provider_priority;
		provider_priority.allFields = 0;

		for (ASProcessInfo* provider : providers) {
			for (int i = 0; i < reciever.loc_rib.size(); i++) {
				if (provider->loc_rib[i].priority.allFields == 0)
					continue;

				provider_priority.pathLength = provider->loc_rib[i].priority.pathLength - 1;
				provider_priority.relationship = provider->loc_rib[i].priority.relationship;

				as_process_announcement(reciever, *provider, provider->asn_id, i, provider->loc_rib[i], provider_priority, timestamp_tiebrake, prefer_new_timestamp, random_tiebraking);
			}
		}
	}

	void as_process_announcement(ASProcessInfo& reciever, const ASProcessInfo& sender, const ASN_ID& recieved_from_id, const uint32_t& prefix_block_id, const AnnouncementDynamicData& other_announcement, const Priority& temp_priority, bool timestamp_tiebrake, bool prefer_new_timestamp, bool random_tiebraking) {
		if (temp_priority.allFields > reciever.loc_rib[prefix_block_id].priority.allFields) {
			reciever.loc_rib[prefix_block_id].fill(recieved_from_id, sender.asn, temp_priority, other_announcement.static_data);
		} else if (temp_priority.allFields == reciever.loc_rib[prefix_block_id].priority.allFields) {
			if (timestamp_tiebrake) {
				if (prefer_new_timestamp) {
					if (reciever.loc_rib[prefix_block_id].static_data->timestamp > other_announcement.static_data->timestamp)
						return;
				} else {
					if (reciever.loc_rib[prefix_block_id].static_data->timestamp < other_announcement.static_data->timestamp)
						return;
				}
			}

			if (reciever.loc_rib[prefix_block_id].static_data->timestamp == other_announcement.static_data->timestamp) {
				if (random_tiebraking) {
					if (rand() % 2 == 0)
						return;
				} else {//lowest ASN wins
					if (reciever.loc_rib[prefix_block_id].received_from_asn < sender.asn)
						return;
				}
			}

			reciever.loc_rib[prefix_block_id].fill(recieved_from_id, sender.asn, temp_priority, other_announcement.static_data);
		}
	}
}