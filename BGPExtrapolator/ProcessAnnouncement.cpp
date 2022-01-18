#include "BGPExtrapolator.h"

namespace BGPExtrapolator {
	void AnnouncementDynamicData::fill(const ASN& received_from_asn, const Priority& priority, AnnouncementStaticData* static_data) {
		this->received_from_asn = received_from_asn;
		this->priority = priority;
		this->static_data = static_data;
	}

	void AnnouncementDynamicData::reset() {
		this->received_from_asn = 0;
		this->priority.allFields = 0;
		this->static_data = nullptr;
	}

	void as_process_customer_announcements(ASProcessInfo& reciever, const std::vector<ASProcessInfo*>& customers, const PropagationConfiguration& config) {
		as_process_relationship(reciever, customers, RELATIONSHIP_PRIORITY_CUSTOMER_TO_PROVIDER, config);
	}

	void as_process_peer_announcements(ASProcessInfo& reciever, const std::vector<ASProcessInfo*>& peers, const PropagationConfiguration& config) {
		as_process_relationship(reciever, peers, RELATIONSHIP_PRIORITY_PEER_TO_PEER, config);
	}

	void as_process_provider_announcements(ASProcessInfo& reciever, const std::vector<ASProcessInfo*>& providers, const PropagationConfiguration& config) {
		as_process_relationship(reciever, providers, RELATIONSHIP_PRIORITY_PROVIDER_TO_CUSTOMER, config);
	}

	void as_process_relationship(ASProcessInfo& reciever, const std::vector<ASProcessInfo*>& neighbors, const uint8_t& relationship_priority, const PropagationConfiguration& config) {
		Priority temp_priority;
		temp_priority.allFields = 0;
		temp_priority.relationship = relationship_priority;

		for (ASProcessInfo* neighbor : neighbors) {
			for (uint32_t i = 0; i < reciever.loc_rib.size(); i++) {
				//Skip if the neighbor has nothing for this prefix
				if (neighbor->loc_rib[i].priority.allFields == 0)
					continue;

				temp_priority.pathLength = neighbor->loc_rib[i].priority.pathLength - 1;

				if(compare_announcements(reciever, *neighbor, i, temp_priority, config))
					reciever.loc_rib[i].fill(neighbor->asn, temp_priority, neighbor->loc_rib[i].static_data);
			}
		}
	}

	//TODO: This needs a better approach than all of this branching. Though, this branching is *predictable* since it is the same every time
	bool compare_announcements(const ASProcessInfo& reciever, const ASProcessInfo& sender, const uint32_t& prefix_block_id, const Priority& temp_priority, const PropagationConfiguration& config) {
		const AnnouncementDynamicData& current_announcement = reciever.loc_rib[prefix_block_id];
		const AnnouncementDynamicData& sent_announcement = sender.loc_rib[prefix_block_id];

		if (temp_priority.allFields > current_announcement.priority.allFields) {
			return true;
		} else if (temp_priority.allFields == current_announcement.priority.allFields) {
			if (config.timestamp_comparison == TIMESTAMP_COMPARISON::PREFER_NEWER && current_announcement.static_data->timestamp > sent_announcement.static_data->timestamp)
				return false;
			else if (config.timestamp_comparison == TIMESTAMP_COMPARISON::PREFER_OLDER && current_announcement.static_data->timestamp < sent_announcement.static_data->timestamp)
				return false;

			if (current_announcement.static_data->timestamp == sent_announcement.static_data->timestamp) {
				if (config.tiebraking_method == TIEBRAKING_METHOD::RANDOM) {
					if (rand() % 2 == 0)
						return false;
				} else {//lowest ASN wins
					if (current_announcement.received_from_asn < sent_announcement.received_from_asn)
						return false;
				}
			}

			return true;
		}

		return false;
	}
}