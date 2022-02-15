#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <map>

#include <rapidcsv.h>

#include "Utils.hpp"
#include "Announcement.hpp"
#include "LocalRibs.hpp"
#include "LocalRibsTransposed.hpp"

enum TIMESTAMP_COMPARISON {
	DISABLED,
	PREFER_NEWER,
	PREFER_OLDER
};

enum TIEBRAKING_METHOD {
	RANDOM,
	PREFER_LOWEST_ASN
};

struct SeedingConfiguration {
	bool originOnly;
	TIMESTAMP_COMPARISON timestampComparison;
	TIEBRAKING_METHOD tiebrakingMethod;
};

//Circular dependency
class PropagationPolicy;

class Graph {
protected:
	std::unordered_map<ASN, ASN_ID> asnToID;
	std::map<std::pair<ASN, ASN>, uint8_t> relationshipPriority;

	//Do not move or append to these lists after initialization!!!
	//TODO: could replace with a custom allocator and specific data structure for rank iteration and random access
	std::vector<PropagationPolicy*> idToPolicy;

	std::vector<std::vector<PropagationPolicy*>> rankToPolicies;
	std::vector<std::vector<PropagationPolicy*>> asIDToProviders;
	std::vector<std::vector<PropagationPolicy*>> asIDToPeers;
	std::vector<std::vector<PropagationPolicy*>> asIDToCustomers;

	std::vector<AnnouncementStaticData> announcementStaticData;

	LocalRibs localRibs;
	//LocalRibsTransposed localRibs;

public:
	Graph(rapidcsv::Document& relationshipsCSV, size_t maximumPrefixBlockID, size_t maximumSeededAnnouncements);
	~Graph();

	void ResetAllAnnouncements();
	void ResetAllNonSeededAnnouncements();

	void SeedBlock(const std::string& filePathAnnouncements, const SeedingConfiguration& config);

	void Propagate();

	std::vector<ASN> Traceback(const ASN& startingASN, const uint32_t& prefixBlockID);

	void GenerateResultsCSV(const std::string& resultsFilePath, std::vector<ASN> localRibsToDump);

	inline size_t GetNumPrefixes() {
		return localRibs.numPrefixes;
	}

	inline AnnouncementCachedData& GetCachedData(const ASN_ID& asnID, const uint32_t& prefixBlockID) {
		return localRibs.GetAnnouncement(asnID, prefixBlockID);
	}

	inline AnnouncementStaticData& GetStaticData(const size_t& index) {
		return announcementStaticData[index];
	}

protected:
	void SeedPath(const std::vector<ASN>& asPath, size_t staticDataIndex, const Prefix& prefix, const std::string& prefixString, int64_t timestamp, const SeedingConfiguration& config);
};