#pragma once

#include <vector>

#include "Announcement.hpp"

class LocalRibsTransposed {
private:
	std::vector<std::vector<AnnouncementCachedData>> localRibs;

public:
	const size_t numAses;
	const size_t numPrefixes;

	LocalRibsTransposed(const size_t& numASes, const size_t& numPrefixes) : numAses(numASes), numPrefixes(numPrefixes) {
		localRibs.resize(numPrefixes);
		for (auto& v : localRibs)
			v.resize(numASes);
	}

	inline AnnouncementCachedData& GetAnnouncement(const ASN_ID& asnID, const uint32_t& prefixBlockID) {
		return localRibs[prefixBlockID][asnID];
	}
};