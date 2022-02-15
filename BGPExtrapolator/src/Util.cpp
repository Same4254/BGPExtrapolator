#include "Utils.hpp"

std::vector<ASN> Util::parseASNList(const std::string& as_path_string) {
	std::vector<ASN> as_path;

	if (as_path_string == "{}" || as_path_string == "{ }")
		return as_path;

	const char* c_as_path_string = as_path_string.c_str();
	char* next;
	long val = strtol(c_as_path_string + 1, &next, 10);

	as_path.push_back(val);

	while (next < c_as_path_string + as_path_string.size() - 1) {
		val = strtol(next + 1, &next, 10);
		as_path.push_back(val);
	}

	return as_path;
}