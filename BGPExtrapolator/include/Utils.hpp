#pragma once

#include <vector>
#include <string>

#include "Defines.h"

class Util {
public:
	/**
	 * Converts the string representing a list of ASNs into a vector, with preserved ordering. The origin is placed at the end of the vector.
	 *
	 * Input Format:
	 *  "{1,2,3}"
	 *
	 * Where 3 is the origin if the input is an AS_PATH. The returned list will place 3 at the end of the vector (index being size - 1).
	 *
	 * This may also be used as a general parser for a list of 32 bit unsigned integers.
	 *
	 * @param as_path_string -> String representing the path
	 * @return A vector containing the path, with ordering preserved. Origin is at the end of the vector.
	*/
	static std::vector<ASN> parseASNList(const std::string& asPathString);
};