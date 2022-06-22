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

bool Util::ASPathContainCycle(const std::vector<ASN> &asPath) {
    bool cycle = false;

    for (size_t i = 0; i < asPath.size(); i++) {
        bool changed = false;
        for (size_t j = i + 1; j < asPath.size(); j++) {
            if (!changed && asPath[i] != asPath[j]) {
                changed = true;
            } else if (changed && asPath[i] == asPath[j]) {
                changed = true;
            }
        }
    }

    return cycle;
}
