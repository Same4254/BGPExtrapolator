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

//NOTE. "TODO" marks code changes. "PERF_TODO" marks a *performance* suggestion that needs to be tested

/**
 * The Graph is a structure capable of building a topological graph from CAIDA relationships, and seeding ground-truth MRT announcements,
 *	and allowing that information to propagate while strictly obeying Gao-Rexford propagation rules.
 * 
 * Gao Rexford Paper: "Stable Internet Routing Without Global Coordination" by Lixin Gao and Jennifer Rexford.
 *
 * Results that are relevant during propagation: 
 *  AS Export to Provider:
 *   - Provider routes: no
 *   - Peer routes: no
 *   - Customer routes: yes
 *
 *  Export to Customer:
 *   - Provider routes: yes
 *   - Peer routes: yes
 *   - Customer routes: no
 *
 *  Export to Peer:
 *   - Provider routes: no
 *   - Peer routes: no
 *   - Customer routes: yes
 * 
 * Design and Performance Goals:
 *  - Allow different propgation policies to be distributed among the graph
 *  - Allow different seeding policies for a graph
 *  - Once propagation starts, there is no heap allocations or de-allocations (Minimize performance hits due to uneccesary heap activity)
 *  - Allow local ribs to be structured in memory in any way, easily. Such that different allocation methods can be easily compared and changed
 *  - Separate neccessary announcement data for propagation (cached data) and unecessary, or uncommonly used, data (static data) 
 *    - Allows more cached data to fit into the cache (increases performance)
*/
class Graph {
protected:
	//Each ASN is mapped to a corresponding index called an ID. The purpose to allow direct index lookups rather than mappings.
	std::unordered_map<ASN, ASN_ID> asnToID;
    std::vector<ASN> idToASN;

	/**
	 * Every realtionship has a priority, this stores the cost of moving from ASN1(first element of the pair) to ASN2(second element of the pair)
	 * Used during seeding to lookup the relationship priority between two ASes on the path
	*/
	std::map<std::pair<ASN, ASN>, uint8_t> relationshipPriority;

	//Do not move or append to these lists after initialization!!!
	//TODO: could replace with a custom allocator and specific data structure for rank iteration and random access
	std::vector<PropagationPolicy*> idToPolicy;

	std::vector<std::vector<ASN_ID>> rankToIDs;
	std::vector<std::vector<ASN_ID>> asIDToProviderIDs;
	std::vector<std::vector<ASN_ID>> asIDToPeerIDs;

	std::vector<std::vector<ASN_ID>> asIDToCustomerIDs;

	std::vector<AnnouncementStaticData> announcementStaticData;

    bool stubRemoval;
    std::unordered_map<ASN, ASN_ID> stubASNToProviderID;

    LocalRibs localRibs;

public:

	/**
	 * Constructs a graph from the given CAIDA relationship dataset. This will also do the one-time allocation of all local ribs and static data.
	 * 
	 * @param relationshipsCSV 
	 * @param maximumPrefixBlockID 
	 * @param maximumSeededAnnouncements 
	*/
	Graph(const std::string &relationshipsFilePath, bool stubRemoval);
	~Graph();

    /**
     * Tests if the two graphs are equivalent. "Equivalent" meaning that the local ribs have the same AS paths.
     * The seeding flag is ignored. Used for test cases
     */
    bool CompareTo(Graph &graph);

	/**
	 * Resets all announcements to an initial state. No memory is deallocated.
	*/
	void ResetAllAnnouncements();

	/**
	 * Resets all announcements, that are not seeded, to an initial state. No memory is deallocated
	*/
	void ResetAllNonSeededAnnouncements();

	/**
	 * Given a dataset of MRT announcements, and the method of seeding (configuration), this seeds the real-world data into the graph.
	 * All announcements inserted during this stage will be marked as seeded. And will not be replaced during propagation.
	 * 
	 * Special Cases:
	 *  - If an AS on the path does not exist in the graph (the CAIDA relationships), then that AS is skipped (while preserving path length).
	 *    This missing AS will thus not show up during traceback or the final results. Such improvement may be left in a future paper.
	 *  - Prepending will inflate the path length for the next unique AS on the path. Only the first occurence of the prepended ASN is seeded.
	 *  - The origin will be seeded to show it recieved from itself (marks the end of the path during traceback)
	 * 
	 * @param filePathAnnouncements 
	 * @param config 
	*/
	void SeedBlock(const std::string& filePathAnnouncements, const SeedingConfiguration& config, size_t maximumPrefixBlockID);

	/**
	 * Using Gao Rexford rules, this will propagate the announcements throughout the graph. 
	 * The propagation policies will be used to determine how an AS will compare incoming announcements with the accepted announcement already in the local rib.
	 * The propagated copy of a seeded announcement will not be marked as seeded. An incoming announcement cannot replace a seeded announcement (since that is the known truth).
	 * 
	 * This will not seed the announcements. Be sure to seed announcements before calling this.
	*/
	void Propagate();

	/**
	 * Given a starting ASN and a prefixID, this will traceback the AS_PATH of that announcement.
	 * 
	 * @param startingASN 
	 * @param prefixBlockID 
	 * @return AS_PATH of the announcement. The list ends with the origin (index: size - 1) and starts with the given ASN (index: 0)
	*/
	std::vector<ASN> Traceback(const ASN& startingASN, const uint32_t& prefixBlockID);

	/**
	 * Write the local ribs of the provided ASNs to the CSV file at the given location.
	 * If no ASNs are specified, all local ribs are dumped to the file. 
	 *  - If this is the case, prepare your hard drive...
	 * 
	 * @param resultsFilePath 
	 * @param localRibsToDump 
	*/
	void GenerateResultsCSV(const std::string& resultsFilePath, std::vector<ASN> localRibsToDump);

	inline AnnouncementCachedData& GetCachedData(const ASN_ID& asnID, const uint32_t& prefixBlockID) {
		return localRibs.GetAnnouncement(asnID, prefixBlockID);
	}

	inline const AnnouncementStaticData& GetStaticData(const size_t& index) const {
		return announcementStaticData[index];
	}

	inline PropagationPolicy* GetPropagationPolicy(const ASN_ID& asnID) {
		return idToPolicy[asnID];
	}

    inline size_t GetNumASes() { return localRibs.GetNumASes(); }
    inline size_t GetNumPrefixes() { return localRibs.GetNumPrefixes(); }

protected:
	/**
	 * For a given AS_PATH and index to fill static data (corresponding to the static announcement data list of the graph), 
	 *  seed the announcement information along the path with the given behavior configuration
	 * 
	 * @param asPath -> AS_PATH where the origin is at the end of the list (index: size - 1) and the last AS along the path is at index 0
	 * @param staticDataIndex -> Index of the static data structure to fill with data
	 * @param prefix 
	 * @param prefixString 
	 * @param timestamp 
	 * @param config 
	*/
	void SeedPath(const std::vector<ASN>& asPath, size_t staticDataIndex, const Prefix& prefix, const std::string& prefixString, int64_t timestamp, const SeedingConfiguration& config);
};
