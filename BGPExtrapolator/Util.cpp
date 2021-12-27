#include "BGPExtrapolator.h"

namespace BGPExtrapolator {
	bool path_contains_loop(const std::vector<ASN>& as_path) {
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

	std::vector<ASN> parse_path(std::string as_path_string) {
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

	Prefix cidr_string_to_prefix(std::string s) {
		Prefix p;

		size_t slash_index = s.find("/");
		p.netmask = (uint32_t)pow(2, std::stoi(s.substr(slash_index + 1, s.length() - slash_index - 1))) - 1;

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

	uint8_t tiny_hash(const ASN& asn) {
		uint8_t mask = 0xFF;
		uint8_t value = 0;
		for (size_t i = 0; i < sizeof(asn); i++)
			value = (value ^ (mask & (asn >> (i * 8)))) * 3;

		return value;
	}
}