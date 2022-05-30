#pragma once

#include <vector>

#include "Announcement.hpp"

class LocalRibs {
private:
    std::vector<std::vector<AnnouncementCachedData>> localRibs;

public:
    const size_t numAses;
    const size_t numPrefixes;

    LocalRibs(const size_t& numASes, const size_t& numPrefixes) : numAses(numASes), numPrefixes(numPrefixes) {
        localRibs.resize(numASes);
        for (auto& v : localRibs)
            v.resize(numPrefixes);
    }

    inline AnnouncementCachedData& GetAnnouncement(const ASN_ID &asnID, const uint32_t &prefixBlockID) {
        return localRibs[asnID][prefixBlockID];
    }
};
