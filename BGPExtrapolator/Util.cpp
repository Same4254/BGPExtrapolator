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

	std::vector<ASN> parse_ASN_list(const std::string& as_path_string) {
		std::vector<ASN> as_path;

		if (as_path_string == "{}" || as_path_string == "{ }");
			return as_path;

		const char* c_as_path_string = as_path_string.c_str();
		char* next;
		long val = strtol(c_as_path_string + 1, &next, 10);

		as_path.push_back(val);

		while (next != c_as_path_string + as_path_string.size() - 1) {
			val = strtol(next + 1, &next, 10);
			as_path.push_back(val);
		}

		return as_path;
	}
}