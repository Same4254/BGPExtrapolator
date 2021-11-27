#include "BGPExtrapolator.h"

struct ASMetaData {
	ASN asn;
	ASN_ID asn_id;
	int rank;
	std::vector<ASN> peers, customers, providers;
};

GraphHandle extrapolator_graph_from_relationship_csv(std::string file_path_relationships) {
	rapidcsv::Document relationship_csv(file_path_relationships, rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams(SEPARATED_VALUES_DELIMETER));

	GraphHandle gHandle;
	gHandle.as_process_info.resize(relationship_csv.GetRowCount());
	gHandle.as_id_to_providers.resize(relationship_csv.GetRowCount());
	gHandle.as_id_to_peers.resize(relationship_csv.GetRowCount());
	gHandle.as_id_to_customers.resize(relationship_csv.GetRowCount());

	size_t maximum_rank = 0;
	ASN_ID next_id = 0;

	std::vector<ASMetaData> metadata(relationship_csv.GetRowCount());
	for (int row_index = 0; row_index < relationship_csv.GetRowCount(); row_index++) {
		metadata[row_index].asn = relationship_csv.GetCell<ASN>("asn", row_index);
		metadata[row_index].asn_id = next_id;

		gHandle.asn_to_asn_id.insert({ metadata[row_index].asn, metadata[row_index].asn_id });

		int rank = relationship_csv.GetCell<int>("propagation_rank", row_index);
		metadata[row_index].rank = rank;

		if (rank > maximum_rank)
			maximum_rank = rank;

		metadata[row_index].providers = extrapolator_parse_path(relationship_csv.GetCell<std::string>("providers", row_index));
		metadata[row_index].peers = extrapolator_parse_path(relationship_csv.GetCell<std::string>("peers", row_index));
		metadata[row_index].customers = extrapolator_parse_path(relationship_csv.GetCell<std::string>("customers", row_index));

		next_id++;
	}

	gHandle.as_process_info_by_rank.resize(maximum_rank + 1);
	for (int i = 0; i < metadata.size(); i++) {
		gHandle.as_process_info[i].asn = metadata[i].asn;
		gHandle.as_process_info[i].asn_id = metadata[i].asn_id;

		gHandle.as_process_info_by_rank[metadata[i].rank].push_back(&gHandle.as_process_info[i]);

		for (ASN provider : metadata[i].providers)
			gHandle.as_id_to_providers[i].push_back(&gHandle.as_process_info[gHandle.asn_to_asn_id[provider]]);

		for (ASN peer : metadata[i].peers)
			gHandle.as_id_to_peers[i].push_back(&gHandle.as_process_info[gHandle.asn_to_asn_id[peer]]);

		for (ASN customer : metadata[i].customers)
			gHandle.as_id_to_customers[i].push_back(&gHandle.as_process_info[gHandle.asn_to_asn_id[customer]]);
	}

	return gHandle;
}

void extrapolator_graph_seed_from_csv(GraphHandle& gHandle, size_t block, std::string file_path_announcements) {
	rapidcsv::Document announcements(file_path_announcements, rapidcsv::LabelParams(0, -1), rapidcsv::SeparatorParams(SEPARATED_VALUES_DELIMETER));

	//Allocate space for all of the static information. Don't resize again so that the pointers to the data do not change
	gHandle.announcement_static_data.resize(announcements.GetRowCount());

	for (size_t row_index = 0; row_index < announcements.GetRowCount(); row_index++) {
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
	for (auto& info : gHandle.as_process_info) {
		for (auto& dynamic_ann : info.loc_rib) {
			dynamic_ann.priority.allFields = 0;
			dynamic_ann.priority.pathLength = MAX_PATH_LENGTH;
		}
	}
}