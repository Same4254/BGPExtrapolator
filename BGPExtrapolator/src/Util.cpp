#include "Utils.hpp"

std::vector<ASN> Util::parseASNList(const std::string& asPathString) {
	std::vector<ASN> asPath;

	if (asPathString == "{}" || asPathString == "{ }")
		return asPath;

	const char* asPathStringC = asPathString.c_str();
	char* next;
	long val = strtoul(asPathStringC + 1, &next, 10);

	asPath.push_back(val);

	while (next < asPathStringC + asPathString.size() - 1) {
		val = strtoul(next + 1, &next, 10);
		asPath.push_back(val);
	}

	return asPath;
}