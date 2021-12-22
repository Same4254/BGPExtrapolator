// BGPExtrapolator.cpp : Defines the entry point for the application.

#include "BGPExtrapolator.h"

/***************************** PROPAGATION ******************************/

void extrapolator_propagate(GraphHandle& gHandle) {
	// ************ Propagate Up ************//

	// start at the second rank because the first has no customers
	for (size_t i = 1; i < gHandle.as_process_info_by_rank.size(); i++)
		for (auto& provider : gHandle.as_process_info_by_rank[i])
			extrapolator_as_process_customer_announcements(*provider, gHandle.as_id_to_customers[provider->asn_id]);

	for (size_t i = 0; i < gHandle.as_process_info_by_rank.size(); i++)
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

std::vector<ASN> extrapolator_parse_path(std::string as_path_string) {
	std::vector<ASN> as_path;
	// Remove brackets from string
	as_path_string.erase(std::find(as_path_string.begin(), as_path_string.end(), '}'));
	as_path_string.erase(std::find(as_path_string.begin(), as_path_string.end(), '{'));

	// Fill as_path vector from parsing string
	std::stringstream str_stream(as_path_string);
	std::string tokenN;
	while (getline(str_stream, tokenN, ',')) {
		as_path.push_back(std::stoul(tokenN));
	}

	return as_path;
}

Prefix extrapolator_cidr_string_to_prefix(const std::string& s) {
	Prefix p;

	size_t slash_index = s.find("/");
	p.netmask = (uint32_t) pow(2, std::stoi(s.substr(slash_index + 1, s.length() - slash_index - 1))) - 1;

	p.address = 0;

	size_t startIndex = 0;
	for (int i = 0; i < 4; i++) {
		size_t nextIndex = s.find('.', startIndex);
		if (nextIndex == std::string::npos)
			nextIndex = s.find('/', startIndex);

		p.address |= std::stoi(s.substr(startIndex, startIndex - nextIndex)) << (32 - (8 * (i + 1)));

		startIndex = nextIndex + 1;
	}

	return p;
}

std::string extrapolator_prefix_to_cidr_string(const Prefix& prefix) {
	uint32_t temp_netmask = prefix.netmask;
	int num_bits = 0;

	while (temp_netmask != 0) {
		temp_netmask = temp_netmask >> 1;
		num_bits++;
	}

	uint8_t segment1 = (prefix.address & (0xff << 24)) >> 24;
	uint8_t segment2 = (prefix.address & (0xff << 16)) >> 16;
	uint8_t segment3 = (prefix.address & (0xff << 8)) >> 8;
	uint8_t segment4 = (prefix.address & (0xff << 0)) >> 0;

	std::ostringstream oss;
	oss << std::to_string(segment1) << "." << std::to_string(segment2) << "." << std::to_string(segment3) << "." << std::to_string(segment4) << "/" << std::to_string(num_bits);

	return oss.str();
}

uint8_t extrapolator_tiny_hash(ASN asn) {
	uint8_t mask = 0xFF;
	uint8_t value = 0;
	for (size_t i = 0; i < sizeof(asn); i++)
		value = (value ^ (mask & (asn >> (i * 8)))) * 3;

	return value;
}

int main() {
	//ASProcessInfo reciever;
	//ASProcessInfo customer1, customer2;

	//reciever.loc_rib.push_back (Announcement{ 0, 0, Prefix {0, 0, 0, 0}, Priority {0, 250, 1, 1}, 0 });
	////customer1.loc_rib.push_back(Announcement{ 0, 0, Prefix {0, 0, 0, 0}, Priority {0, 254, 1, 0}, 0 });
	//customer2.loc_rib.push_back(Announcement{ 0, 0, Prefix {0, 0, 0, 0}, Priority {0, 253, 1, 0}, 0 });

	//std::vector<ASProcessInfo*> customers = { &customer1, &customer2 };

	//extrapolator_as_process_customer_announcements(reciever, customers);

	//GraphHandle gHandle = extrapolator_graph_from_relationship_csv("C:\\Users\\sns17003\\Downloads\\CaidaCollector.tsv");

	Prefix p = extrapolator_cidr_string_to_prefix("1.0.4.0/24");
	std::cout << extrapolator_prefix_to_cidr_string(p) << std::endl;

	return 0;
}
