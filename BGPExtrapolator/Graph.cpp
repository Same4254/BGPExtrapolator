#include "BGPExtrapolator.h"

namespace BGPExtrapolator {

	Graph::Graph(const std::string &file_path_relationships) {
		rapidcsv::Document relationship_csv(file_path_relationships, rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams(SEPARATED_VALUES_DELIMETER));

		as_process_info.resize(relationship_csv.GetRowCount());
		as_id_to_providers.resize(relationship_csv.GetRowCount());
		as_id_to_peers.resize(relationship_csv.GetRowCount());
		as_id_to_customers.resize(relationship_csv.GetRowCount());
		as_id_to_relationship_info.resize(relationship_csv.GetRowCount());

		size_t maximum_rank = 0;
		ASN_ID next_id = 0;

		//Store the relationships, assign IDs, and find the maximum rank
		for (int row_index = 0; row_index < relationship_csv.GetRowCount(); row_index++) {
			ASRelationshipInfo& relationship_info = as_id_to_relationship_info[row_index];

			relationship_info.asn = relationship_csv.GetCell<ASN>("asn", row_index);
			relationship_info.asn_id = next_id;

			asn_to_asn_id.insert({ relationship_info.asn, as_id_to_relationship_info[row_index].asn_id });

			int rank = relationship_csv.GetCell<int>("propagation_rank", row_index);
			as_id_to_relationship_info[row_index].rank = rank;

			if (rank > maximum_rank)
				maximum_rank = rank;

			std::vector<ASN> providers = parse_path(relationship_csv.GetCell<std::string>("providers", row_index));
			std::vector<ASN> peers = parse_path(relationship_csv.GetCell<std::string>("peers", row_index));
			std::vector<ASN> customers = parse_path(relationship_csv.GetCell<std::string>("customers", row_index));

			for (auto asn : providers)
				relationship_info.providers.insert(asn);

			for (auto asn : peers)
				relationship_info.peers.insert(asn);

			for (auto asn : customers)
				relationship_info.customers.insert(asn);

			next_id++;
		}

		// Allocate space for the rank structure and point its content to the corresponding data
		// Also put the pointer to other AS data in relationship structures 
		as_process_info_by_rank.resize(maximum_rank + 1);
		for (int i = 0; i < as_id_to_relationship_info.size(); i++) {
			as_process_info[i].asn = as_id_to_relationship_info[i].asn;
			as_process_info[i].asn_id = as_id_to_relationship_info[i].asn_id;
			as_process_info[i].rand_tiebrake_value = tiny_hash(as_process_info[i].asn) % 2 == 0;

			as_process_info_by_rank[as_id_to_relationship_info[i].rank].push_back(&as_process_info[i]);

			for (ASN provider : as_id_to_relationship_info[i].providers)
				as_id_to_providers[i].push_back(&as_process_info[asn_to_asn_id[provider]]);

			for (ASN peer : as_id_to_relationship_info[i].peers)
				as_id_to_peers[i].push_back(&as_process_info[asn_to_asn_id[peer]]);

			for (ASN customer : as_id_to_relationship_info[i].customers)
				as_id_to_customers[i].push_back(&as_process_info[asn_to_asn_id[customer]]);
		}
	}

	void Graph::reset_announcements() {
		for (auto& info : as_process_info)
			for (auto& dynamic_ann : info.loc_rib)
				dynamic_ann.priority.allFields = 0;
	}

	void Graph::seed_from_csv(size_t block, std::string file_path_announcements, bool origin_only, bool random_tiebraking) {
		rapidcsv::Document announcements(file_path_announcements, rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams(SEPARATED_VALUES_DELIMETER));

		//TODO: make a check here to avoid doing this every time (EDIT: though this will likely only be called once in each process... Maybe it does not even need to be its own function)
		allocate_loc_ribs(announcements);

		//Allocate space for all of the static information. Don't resize again so that the pointers to the data do not change
		announcement_static_data.resize(announcements.GetRowCount());

		for (size_t row_index = 0; row_index < announcements.GetRowCount(); row_index++) {
			//***** PARSING

			if (announcements.GetCell<int64_t>("block_id", row_index) != block)
				continue;

			std::string prefix_string = announcements.GetCell<std::string>("prefix", row_index);
			std::string as_path_string = announcements.GetCell<std::string>("as_path", row_index);

			Prefix prefix = cidr_string_to_prefix(prefix_string);
			std::vector<ASN> as_path = parse_path(as_path_string);

			int64_t timestamp = announcements.GetCell<int64_t>("timestamp", row_index);
			ASN origin = announcements.GetCell<ASN>("origin", row_index);

			uint32_t prefix_id = announcements.GetCell<uint32_t>("prefix_id", row_index);
			uint32_t prefix_block_id = announcements.GetCell<uint32_t>("prefix_block_id", row_index);

			prefix.id = prefix_id;
			prefix.block_id = prefix_block_id;
		}
	}

	void Graph::seed_path(std::vector<ASN>& as_path, Prefix& prefix, int64_t timestamp, AnnouncementStaticData* static_data, bool origin_only, bool random_tiebraking) {
		if (as_path.size() == 0)
			return;

		int end_index = origin_only ? as_path.size() - 1 : 0;
		for (int i = as_path.size() - 1; i >= end_index; i--) {
			// If AS not in the graph, skip it
			// TODO: This should be an error
			auto asn_search = asn_to_asn_id.find(as_path[i]);
			if (asn_search == asn_to_asn_id.end())
				continue;

			//If there is prepending, then just keep going along the path. The length is accounted for.
			if (i < as_path.size() - 1 && as_path[i] == as_path[i + 1])
				continue;

			ASN_ID asn_id = asn_search->second;
			ASProcessInfo& recieving_as = as_process_info[asn_id];

			uint8_t relationship = RELATIONSHIP_PRIORITY_ORIGIN;
			if (i != as_path.size() - 1) {
				if (as_id_to_relationship_info[asn_id].providers.find(as_path[i + 1]) == as_id_to_relationship_info[asn_id].providers.end()) {
					relationship = RELATIONSHIP_PRIORITY_PROVIDER;
				} else if (as_id_to_relationship_info[asn_id].peers.find(as_path[i + 1]) == as_id_to_relationship_info[asn_id].peers.end()) {
					relationship = RELATIONSHIP_PRIORITY_PEER;
				} else if (as_id_to_relationship_info[asn_id].customers.find(as_path[i + 1]) == as_id_to_relationship_info[asn_id].customers.end()) {
					relationship = RELATIONSHIP_PRIORITY_CUSTOMER;
				}
			}

			Priority priority;
			priority.allFields = 0;
			priority.pathLength = MIN_PATH_LENGTH - (i - (as_path.size() - 1));
			priority.relationship = relationship;
			priority.seeded = 1;

			ASN_ID recieved_from_id = asn_id;
			if (i < as_path.size() - 1)
				recieved_from_id = asn_to_asn_id.at(as_path[i + 1]);

			AnnouncementDynamicData new_announcement;
			new_announcement.static_data = static_data;

			//If there exists an announcement for this prefix already
			if (recieving_as.loc_rib[prefix.block_id].priority.allFields != 0) {
				AnnouncementDynamicData& current_announcement = recieving_as.loc_rib[prefix.block_id];
				if (timestamp > current_announcement.static_data->timestamp) {
					continue;
				} else if (timestamp == current_announcement.static_data->timestamp) {
					//TODO: perhaps find a better way than to branch here.
					if (random_tiebraking)
						as_process_announcement_random_tiebrake(recieving_as, recieved_from_id, prefix.block_id, new_announcement, priority, recieving_as.rand_tiebrake_value);
					else
						as_process_announcement(recieving_as, recieved_from_id, prefix.block_id, new_announcement, priority);
				} else {
					new_announcement.received_from_id = recieved_from_id;
					new_announcement.priority = priority;

					recieving_as.loc_rib[prefix.block_id] = new_announcement;
				}
			} else {
				//TODO We don't need this additional check here. We know that there is no announcement in the loc_rib, we can just copy the data into the announcement
				as_process_announcement(recieving_as, recieved_from_id, prefix.block_id, new_announcement, priority);
			}
		}
	}

	void Graph::propagate() {
		// ************ Propagate Up ************//

		// start at the second rank because the first has no customers
		for (size_t i = 1; i < as_process_info_by_rank.size(); i++)
			for (auto& provider : as_process_info_by_rank[i])
				as_process_customer_announcements(*provider, as_id_to_customers[provider->asn_id]);

		for (size_t i = 0; i < as_process_info_by_rank.size(); i++)
			for (auto& as : as_process_info_by_rank[i])
				as_process_peer_announcements(*as, as_id_to_peers[as->asn_id]);

		// ************ Propagate Down ************//
		for (int i = as_process_info_by_rank.size() - 2; i >= 0; i--)
			for (auto& customer : as_process_info_by_rank[i])
				as_process_peer_announcements(*customer, as_id_to_peers[customer->asn_id]);
	}

	void Graph::generate_results_csv(std::string results_file_path) {

	}

	void Graph::allocate_loc_ribs(rapidcsv::Document& announcements) {
		size_t maximum_prefix_block_id = 0;
		for (size_t row_index = 0; row_index < announcements.GetRowCount(); row_index++) {
			size_t prefix_block_id = announcements.GetCell<size_t>("prefix_block_id", row_index);
			if (prefix_block_id > maximum_prefix_block_id)
				maximum_prefix_block_id = prefix_block_id;
		}

		for (auto& process_info : as_process_info)
			process_info.loc_rib.resize(maximum_prefix_block_id);

		reset_announcements();
	}
}