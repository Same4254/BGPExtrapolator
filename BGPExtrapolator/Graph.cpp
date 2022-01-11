#include "BGPExtrapolator.h"

namespace BGPExtrapolator {
	Graph::Graph(const std::string& file_path_relationships, size_t maximum_prefix_block_id, size_t maximum_number_seeded_announcements) {
		rapidcsv::Document relationships_csv(file_path_relationships, rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams(SEPARATED_VALUES_DELIMETER));

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
		for (auto& process_info : as_process_info)
			process_info.loc_rib.resize(maximum_prefix_block_id);

		announcement_static_data.resize(maximum_number_seeded_announcements);

		reset_all_announcements();
	}

	void Graph::reset_all_announcements() {
		for (auto& info : as_process_info) {
		for (auto& dynamic_ann : info.loc_rib) {
			dynamic_ann.reset();
		}}
	}

	void Graph::reset_all_non_seeded_announcements() {
		for (auto& info : as_process_info) {
		for (auto& dynamic_ann : info.loc_rib) {
			if(dynamic_ann.priority.seeded == 0)
				dynamic_ann.reset();
		}}
	}

	void Graph::seed_block_from_csv(std::string& file_path_announcements, bool origin_only, bool prefer_new_timestamp, bool random_tiebraking) {
		rapidcsv::Document announcements_csv(file_path_announcements, rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams(SEPARATED_VALUES_DELIMETER));

		for (size_t row_index = 0; row_index < announcements_csv.GetRowCount(); row_index++) {
			//***** PARSING
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

			seed_path(as_path, &announcement_static_data[row_index], prefix, timestamp, origin_only, prefer_new_timestamp, random_tiebraking);
		}
	}

	void Graph::seed_path(std::vector<ASN>& as_path, AnnouncementStaticData* static_data, Prefix& prefix, int64_t timestamp, bool origin_only, bool prefer_new_timestamp, bool random_tiebraking) {
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
			if (i < as_path.size() - 1) {
				if (as_id_to_relationship_info[asn_id].providers.find(as_path[i + 1]) == as_id_to_relationship_info[asn_id].providers.end()) {
					relationship = RELATIONSHIP_PRIORITY_PROVIDER;
				} else if (as_id_to_relationship_info[asn_id].peers.find(as_path[i + 1]) == as_id_to_relationship_info[asn_id].peers.end()) {
					relationship = RELATIONSHIP_PRIORITY_PEER;
				} else if (as_id_to_relationship_info[asn_id].customers.find(as_path[i + 1]) == as_id_to_relationship_info[asn_id].customers.end()) {
					relationship = RELATIONSHIP_PRIORITY_CUSTOMER;
				} else {
					//TODO check for stub: https://github.com/c-morris/BGPExtrapolator/commit/364abb3d70d8e6aa752450e756348b2e1f82c739

					//broken relationship
					continue;
				}
			}

			Priority priority;
			priority.allFields = 0;
			priority.pathLength = MIN_PATH_LENGTH - (i - (as_path.size() - 1));//This should be fine because there *should* be no path longer than 254 in length
			priority.relationship = relationship;
			priority.seeded = 1;

			ASN_ID recieved_from_id = asn_id;
			if (i < as_path.size() - 1)
				recieved_from_id = asn_to_asn_id.at(as_path[i + 1]);

			//If there exists an announcement for this prefix already
			if (recieving_as.loc_rib[prefix.block_id].priority.allFields != 0) {
				AnnouncementDynamicData& current_announcement = recieving_as.loc_rib[prefix.block_id];
				if (prefer_new_timestamp) {
					if (timestamp > current_announcement.static_data->timestamp)
						continue;
				} else {
					if (timestamp < current_announcement.static_data->timestamp)
						continue;
				}

				if (timestamp == current_announcement.static_data->timestamp) {
					if (recieving_as.loc_rib[prefix.block_id].priority.allFields > priority.allFields)
						continue;

					if (recieving_as.loc_rib[prefix.block_id].priority.allFields == priority.allFields) {
						if (random_tiebraking) {
							if (rand() % 2 == 0)
								continue;
						} else {//lowest recieved_from ASN wins
							if (current_announcement.received_from_asn < as_path[i + 1])
								continue;
						}
					}
				}
			}

			//Recieve from itself if it is the origin
			ASN recieved_from_asn = i < as_path.size() - 1 ? as_path[i + 1] : recieving_as.asn;

			//accept the announcement
			recieving_as.loc_rib[prefix.block_id].fill(recieved_from_id, recieved_from_asn, priority, static_data);
		}
	}

	void Graph::propagate(bool timestamp_tiebrake, bool prefer_new_timestamp, bool random_tiebraking) {
		// ************ Propagate Up ************//

		// start at the second rank because the first has no customers
		for (size_t i = 1; i < as_process_info_by_rank.size(); i++)
			for (auto& provider : as_process_info_by_rank[i])
				as_process_customer_announcements(*provider, as_id_to_customers[provider->asn_id], timestamp_tiebrake, prefer_new_timestamp, random_tiebraking);

		for (size_t i = 0; i < as_process_info_by_rank.size(); i++)
			for (auto& as : as_process_info_by_rank[i])
				as_process_peer_announcements(*as, as_id_to_peers[as->asn_id], timestamp_tiebrake, prefer_new_timestamp, random_tiebraking);

		// ************ Propagate Down ************//
		//Customer looks up to the provider and looks at its data, that is why the - 2 is there
		for (int i = as_process_info_by_rank.size() - 2; i >= 0; i--)
			for (auto& customer : as_process_info_by_rank[i])
				as_process_peer_announcements(*customer, as_id_to_peers[customer->asn_id], timestamp_tiebrake, prefer_new_timestamp, random_tiebraking);
	}

	void Graph::generate_results_csv(const std::string& results_file_path) {

	}
}