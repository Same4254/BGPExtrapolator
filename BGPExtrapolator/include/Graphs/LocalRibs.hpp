#pragma once

#include <vector>

#include "Announcement.hpp"

class LocalRibs {
private:
    std::vector<std::vector<AnnouncementCachedData>> localRibs;
    size_t numAses;
    size_t numPrefixes;

public:
    LocalRibs() : numAses(0), numPrefixes(0) {
    }

    inline AnnouncementCachedData& GetAnnouncement(const ASN_ID &asnID, const uint32_t &prefixBlockID) {
        return localRibs[asnID][prefixBlockID];
    }

    inline size_t GetNumASes() { return numAses; }
    
    inline void SetNumASes(size_t numASes) {
        this->numAses = numASes;
        
        size_t oldLength = localRibs.size();
        localRibs.resize(numASes);

        if (numAses > oldLength) {
            for (size_t i = oldLength; i < numASes; i++) {
                localRibs[i].resize(numPrefixes);
            }
        }
    }
    
    inline size_t GetNumPrefixes() { return numPrefixes; }

    inline void SetNumPrefixes(size_t numPrefixes) {
        this->numPrefixes = numPrefixes;

        for (auto &rib : localRibs) {
            rib.resize(numPrefixes);
        }
    }
};
