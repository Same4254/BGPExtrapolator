#include "BGPExtrapolator.h"

GraphHandle extrapolator_graph_from_relationship_csv(std::string file_path_relationships) {
	rapidcsv::Document relationship_csv(file_path_relationships, rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams(SEPARATED_VALUES_DELIMETER));

	GraphHandle gHandle;
	gHandle.as_process_info.resize(relationship_csv.GetRowCount());
	gHandle.as_id_to_providers.resize(relationship_csv.GetRowCount());
	gHandle.as_id_to_peers.resize(relationship_csv.GetRowCount());
	gHandle.as_id_to_customers.resize(relationship_csv.GetRowCount());
	gHandle.as_id_to_relationship_info.resize(relationship_csv.GetRowCount());

	size_t maximum_rank = 0;
	ASN_ID next_id = 0;

	//Store the relationships, assign IDs, and find the maximum rank
	for (int row_index = 0; row_index < relationship_csv.GetRowCount(); row_index++) {
		ASRelationshipInfo& relationship_info = gHandle.as_id_to_relationship_info[row_index];

		relationship_info.asn = relationship_csv.GetCell<ASN>("asn", row_index);
		relationship_info.asn_id = next_id;

		gHandle.asn_to_asn_id.insert({ relationship_info.asn, gHandle.as_id_to_relationship_info[row_index].asn_id });

		int rank = relationship_csv.GetCell<int>("propagation_rank", row_index);
		gHandle.as_id_to_relationship_info[row_index].rank = rank;

		if (rank > maximum_rank)
			maximum_rank = rank;

		std::vector<ASN> providers = extrapolator_parse_path(relationship_csv.GetCell<std::string>("providers", row_index));
		std::vector<ASN> peers = extrapolator_parse_path(relationship_csv.GetCell<std::string>("peers", row_index));
		std::vector<ASN> customers = extrapolator_parse_path(relationship_csv.GetCell<std::string>("customers", row_index));

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
	gHandle.as_process_info_by_rank.resize(maximum_rank + 1);
	for (int i = 0; i < gHandle.as_id_to_relationship_info.size(); i++) {
		gHandle.as_process_info[i].asn = gHandle.as_id_to_relationship_info[i].asn;
		gHandle.as_process_info[i].asn_id = gHandle.as_id_to_relationship_info[i].asn_id;
		gHandle.as_process_info[i].rand_tiebrake_value = extrapolator_tiny_hash(gHandle.as_process_info[i].asn) % 2 == 0;

		gHandle.as_process_info_by_rank[gHandle.as_id_to_relationship_info[i].rank].push_back(&gHandle.as_process_info[i]);

		for (ASN provider : gHandle.as_id_to_relationship_info[i].providers)
			gHandle.as_id_to_providers[i].push_back(&gHandle.as_process_info[gHandle.asn_to_asn_id[provider]]);

		for (ASN peer : gHandle.as_id_to_relationship_info[i].peers)
			gHandle.as_id_to_peers[i].push_back(&gHandle.as_process_info[gHandle.asn_to_asn_id[peer]]);

		for (ASN customer : gHandle.as_id_to_relationship_info[i].customers)
			gHandle.as_id_to_customers[i].push_back(&gHandle.as_process_info[gHandle.asn_to_asn_id[customer]]);
	}

	return gHandle;
}

void extrapolator_graph_seed_from_csv(GraphHandle& gHandle, size_t block, std::string file_path_announcements, bool origin_only, bool random_tiebraking) {
	rapidcsv::Document announcements(file_path_announcements, rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams(SEPARATED_VALUES_DELIMETER));

	extrapolator_graph_reset_announcements(gHandle);

	//Allocate space for all of the static information. Don't resize again so that the pointers to the data do not change
	gHandle.announcement_static_data.resize(announcements.GetRowCount());

	for (size_t row_index = 0; row_index < announcements.GetRowCount(); row_index++) {
		//***** PARSING

		if (announcements.GetCell<int64_t>("block_id", row_index) != block)
			continue;

		std::string prefix_string = announcements.GetCell<std::string>("prefix", row_index);
		std::string as_path_string = announcements.GetCell<std::string>("as_path", row_index);

		Prefix prefix = extrapolator_cidr_string_to_prefix(prefix_string);
		std::vector<ASN> as_path = extrapolator_parse_path(as_path_string);

		int64_t timestamp = announcements.GetCell<int64_t>("timestamp", row_index);
		ASN origin = announcements.GetCell<ASN>("origin", row_index);

		uint32_t prefix_id = announcements.GetCell<uint32_t>("prefix_id", row_index);
		uint32_t prefix_block_id = announcements.GetCell<uint32_t>("prefix_block_id", row_index);

		prefix.id = prefix_id;
		prefix.block_id = prefix_block_id;

		//**** SEEDING
		if (as_path.size() == 0)
			continue;

		int end_index = origin_only ? as_path.size() - 1 : 0;
		for (int i = as_path.size() - 1; i >= end_index; i--) {
			// If AS not in the graph, skip it
			// TODO: This should be an error
			auto asn_search = gHandle.asn_to_asn_id.find(as_path[i]);
			if (asn_search == gHandle.asn_to_asn_id.end())
				continue;

			//If there is prepending, then just keep going along the path. The length is accounted for.
			if (i < as_path.size() - 1 && as_path[i] == as_path[i + 1])
				continue;

			ASN_ID asn_id = asn_search->second;
			ASProcessInfo &recieving_as = gHandle.as_process_info[asn_id];

			uint8_t relationship = RELATIONSHIP_PRIORITY_ORIGIN;
			if (i != as_path.size() - 1) {
				if (gHandle.as_id_to_relationship_info[asn_id].providers.find(as_path[i + 1]) == gHandle.as_id_to_relationship_info[asn_id].providers.end()) {
					relationship = RELATIONSHIP_PRIORITY_PROVIDER;
				} else if (gHandle.as_id_to_relationship_info[asn_id].peers.find(as_path[i + 1]) == gHandle.as_id_to_relationship_info[asn_id].peers.end()) {
					relationship = RELATIONSHIP_PRIORITY_PEER;
				} else if (gHandle.as_id_to_relationship_info[asn_id].customers.find(as_path[i + 1]) == gHandle.as_id_to_relationship_info[asn_id].customers.end()) {
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
				recieved_from_id = gHandle.asn_to_asn_id.at(as_path[i + 1]);

			AnnouncementDynamicData new_announcement;
			new_announcement.static_data = &gHandle.announcement_static_data[row_index];

			//If there exists an announcement for this prefix already
			if (recieving_as.loc_rib[prefix_block_id].priority.allFields != 0) {
				AnnouncementDynamicData& current_announcement = recieving_as.loc_rib[prefix_block_id];
				if (timestamp > current_announcement.static_data->timestamp) {
					continue;
				} else if(timestamp == current_announcement.static_data->timestamp) {
					extrapoaltor_as_process_announcement_random_tiebrake(recieving_as, recieved_from_id, prefix_block_id, new_announcement, priority, recieving_as.rand_tiebrake_value);
				} else {
					new_announcement.received_from_id = recieved_from_id;
					new_announcement.priority = priority;

					recieving_as.loc_rib[prefix_block_id] = new_announcement;
				}
			}
		}
	}
}

void extrapolator_graph_allocate_loc_ribs(GraphHandle& gHandle, std::string file_path_announcements) {
	rapidcsv::Document announcements(file_path_announcements, rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams(SEPARATED_VALUES_DELIMETER));

	size_t maximum_prefix_block_id = 0;
	for (size_t row_index = 0; row_index < announcements.GetRowCount(); row_index++) {
		size_t prefix_block_id = announcements.GetCell<size_t>("prefix_block_id", row_index);
		if (prefix_block_id > maximum_prefix_block_id)
			maximum_prefix_block_id = prefix_block_id;
	}

	for (auto& process_info : gHandle.as_process_info)
		process_info.loc_rib.resize(maximum_prefix_block_id);

	extrapolator_graph_reset_announcements(gHandle);
}

void extrapolator_graph_results_csv_from_handle(GraphHandle& gHandle, std::string results_file_path) {

}

void extrapolator_graph_reset_announcements(GraphHandle& gHandle) {
	for (auto& info : gHandle.as_process_info)
	for (auto& dynamic_ann : info.loc_rib)
		dynamic_ann.priority.allFields = 0;
}