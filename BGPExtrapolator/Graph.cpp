#include "BGPExtrapolator.h"

namespace BGPExtrapolator {

	Graph::Graph(const std::string& file_path_relationships, const std::string& file_path_announcements) 
		: relationships_csv(file_path_relationships, rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams(SEPARATED_VALUES_DELIMETER))
		, announcements_csv(file_path_announcements, rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams(SEPARATED_VALUES_DELIMETER)) 
	{
		//***** Relationship Parsing *****//
		as_process_info.resize(relationships_csv.GetRowCount());
		as_id_to_providers.resize(relationships_csv.GetRowCount());
		as_id_to_peers.resize(relationships_csv.GetRowCount());
		as_id_to_customers.resize(relationships_csv.GetRowCount());
		as_id_to_relationship_info.resize(relationships_csv.GetRowCount());

		size_t maximum_rank = 0;
		ASN_ID next_id = 0;

		//Store the relationships, assign IDs, and find the maximum rank
		for (int row_index = 0; row_index < relationships_csv.GetRowCount(); row_index++) {
			ASRelationshipInfo& relationship_info = as_id_to_relationship_info[row_index];

			relationship_info.asn = relationships_csv.GetCell<ASN>("asn", row_index);
			relationship_info.asn_id = next_id;

			asn_to_asn_id.insert({ relationship_info.asn, as_id_to_relationship_info[row_index].asn_id });

			int rank = relationships_csv.GetCell<int>("propagation_rank", row_index);
			as_id_to_relationship_info[row_index].rank = rank;

			if (rank > maximum_rank)
				maximum_rank = rank;

			std::vector<ASN> providers = parse_ASN_list(relationships_csv.GetCell<std::string>("providers", row_index));
			std::vector<ASN> peers = parse_ASN_list(relationships_csv.GetCell<std::string>("peers", row_index));
			std::vector<ASN> customers = parse_ASN_list(relationships_csv.GetCell<std::string>("customers", row_index));

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
			as_process_info[i].rand_tiebrake_value = galois_hash(as_process_info[i].asn) % 2 == 0;

			as_process_info_by_rank[as_id_to_relationship_info[i].rank].push_back(&as_process_info[i]);

			for (ASN provider : as_id_to_relationship_info[i].providers)
				as_id_to_providers[i].push_back(&as_process_info[asn_to_asn_id[provider]]);

			for (ASN peer : as_id_to_relationship_info[i].peers)
				as_id_to_peers[i].push_back(&as_process_info[asn_to_asn_id[peer]]);

			for (ASN customer : as_id_to_relationship_info[i].customers)
				as_id_to_customers[i].push_back(&as_process_info[asn_to_asn_id[customer]]);
		}

		//***** Local RIB allocation *****//

		// Maps each block to the number of announcements in that block. 
		// Used to allocate the size of the static announcement data (which is the maximum number of announcements used in any block)
		std::map<int, int> block_to_num_announcements;

		size_t maximum_prefix_block_id = 0;
		for (size_t row_index = 0; row_index < announcements_csv.GetRowCount(); row_index++) {
			size_t prefix_block_id = announcements_csv.GetCell<size_t>("prefix_block_id", row_index);
			int block_id = announcements_csv.GetCell<int>("block_id", row_index);

			if (prefix_block_id > maximum_prefix_block_id)
				maximum_prefix_block_id = prefix_block_id;

			auto search = block_to_num_announcements.find(block_id);
			if (search == block_to_num_announcements.end())
				block_to_num_announcements.insert(std::make_pair(block_id, 1));
			else
				search->second++;
		}

		for (auto& process_info : as_process_info)
			process_info.loc_rib.resize(maximum_prefix_block_id);

		//Allocate space for all of the static information. Don't resize again so that the pointers to the data do not change
		int maximum_announcement_count = 0;
		for (auto it = block_to_num_announcements.begin(); it != block_to_num_announcements.end(); it++) {
			if (it->second > maximum_announcement_count)
				maximum_announcement_count = it->second;
		}

		announcement_static_data.resize(maximum_announcement_count);

		reset_announcements();
	}

	void Graph::reset_announcements() {
		for (auto& info : as_process_info) {
		for (auto& dynamic_ann : info.loc_rib) {
			dynamic_ann.priority.allFields = 0;
			dynamic_ann.static_data = nullptr;
		}}
	}

	void Graph::seed_block_from_csv(size_t block, bool origin_only, bool random_tiebraking) {
		size_t static_data_index = 0;

		for (size_t row_index = 0; row_index < announcements_csv.GetRowCount(); row_index++) {
			//***** PARSING
			if (announcements_csv.GetCell<int64_t>("block_id", row_index) != block)
				continue;

			std::string prefix_string = announcements_csv.GetCell<std::string>("prefix", row_index);
			std::string as_path_string = announcements_csv.GetCell<std::string>("as_path", row_index);

			Prefix prefix = cidr_string_to_prefix(prefix_string);
			std::vector<ASN> as_path = parse_ASN_list(as_path_string);

			int64_t timestamp = announcements_csv.GetCell<int64_t>("timestamp", row_index);
			ASN origin = announcements_csv.GetCell<ASN>("origin", row_index);

			uint32_t prefix_id = announcements_csv.GetCell<uint32_t>("prefix_id", row_index);
			uint32_t prefix_block_id = announcements_csv.GetCell<uint32_t>("prefix_block_id", row_index);

			prefix.global_id = prefix_id;
			prefix.block_id = prefix_block_id;

			seed_path(as_path, &announcement_static_data[static_data_index], prefix, timestamp, origin_only, random_tiebraking);
			static_data_index++;
		}
	}

	void Graph::seed_path(std::vector<ASN>& as_path, AnnouncementStaticData* static_data, Prefix& prefix, int64_t timestamp, bool origin_only, bool random_tiebraking) {
		if (as_path.size() == 0)
			return;

		static_data->origin = as_path[as_path.size() - 1];
		static_data->prefix = prefix;
		static_data->timestamp = timestamp;

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
			new_announcement.static_data = &announcement_static_data[prefix.block_id];

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

	void Graph::generate_results_csv(const std::string& results_file_path) {

	}
}