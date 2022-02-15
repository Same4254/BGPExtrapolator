#pragma once

#include <vector>
#include <string>

#include "Defines.h"

class Util {
public:
	static std::vector<ASN> parseASNList(const std::string& as_path_string);
};