#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <memory>

#include <limits>
#include <rapidcsv.h>
#include <nlohmann/json.hpp>

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

/**
 * Describes the desired method of seeding
 */
struct SeedingConfiguration {
	bool originOnly;
	TIMESTAMP_COMPARISON timestampComparison;
	TIEBRAKING_METHOD tiebrakingMethod;
};

/**
 * A small pair to cache ASN and ID in the same place in memory
 */
struct ASN_ASNID_PAIR {
	ASN asn;
	ASN_ID id;
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
     *
     * NOTE: Even if stub removal is enabled, the relationship priority between the provider and stub will still be listed here
	*/
	std::map<std::pair<ASN, ASN>, uint8_t> relationshipPriority;

    // ASes are not stored individually. An "AS" is just an index in these structures
    // If stubs are excluded, then they will not have an ID or any memory allocated to them
	std::vector<std::unique_ptr<PropagationPolicy>> idToPolicy;

    // Each rank contains the IDs of the ASes in that rank (rank 0 (index 0) is the lowest propagation rank)
	std::vector<std::vector<ASN_ID>> rankToIDs;
	std::vector<std::vector<ASN_ASNID_PAIR>> asIDToProviderIDs;
	std::vector<std::vector<ASN_ASNID_PAIR>> asIDToPeerIDs;
	std::vector<std::vector<ASN_ASNID_PAIR>> asIDToCustomerIDs;

    bool stubRemoval;
    // This structure will be populated regardless of the stubRemoval flag
    std::unordered_map<ASN, ASN_ID> stubASNToProviderID;

	std::vector<AnnouncementStaticData> announcementStaticData;

    LocalRibs localRibs;

public:
	/**
	 * Constructs a graph from the given CAIDA relationship dataset.
     * Relationships between ASes will be constructed and they will be organized by their propagation_rank
     * This will *not* allocate local ribs, since the announcements are not given here
     * 
     * "Stub removal" is an optimization technique. ASes that have only a single provider (and no other relationships) are stubs.
     * The local rib of a stub is merely the same as its provider (since it has no other choices)
     * Thus, rather than allocate space for their local ribs and propagate the announcements to them, we may reconstruct them when writing results
     * Stubs make up about 36% of the graph, thus removing them will save a significant amount of memory
     *
     * NOTE: As of writing, stub removal will not work with origin-only seeding when a stub is also an origin
     * TODO: ^ fix that
	 * 
	 * @param relationshipsCSV -> File path to the CAIDA Relationships tsv
     * @param stubRemoval -> Whether to enable stub removal optimization
	*/
	Graph(const std::string &relationshipsFilePath, const bool stubRemoval);

	/**
	 * Resets all announcements to their default state. No memory is deallocated.
	*/
	void ResetAllAnnouncements();

	/**
	 * Resets all announcements, that are not seeded, to their default state. No memory is deallocated
	*/
	void ResetAllNonSeededAnnouncements();

	/**
	 * Given a dataset of MRT announcements, and the method of seeding, this seeds the real-world data into the graph.
	 * All announcements inserted during this stage will be marked as seeded. And will not be replaced during propagation.
     * 
     * *********
     * NOTE: This will allocate the local ribs. Be aware of how large the dataset is and how much RAM it will use
     * *********
	 * 
	 * Special Cases:
	 *  - If an AS on the path does not exist in the graph (the CAIDA relationships), then that AS is skipped (while preserving path length).
	 *    This missing AS will thus not show up during traceback or the final results. Such improvement may be left in a future paper.
	 *  - Prepending will inflate the path length for the next unique AS on the path. Only the first occurence of the prepended ASN is seeded.
	 *  - The origin will be seeded to show it recieved from itself (marks the end of the path during traceback)
     *  - If stub removal was enabled, the provider to that stub will still show that it recieved from the stub, if such an announcement was in the mrt dataset
     *     (this means during traceback, be aware that the revieved_from_asn may be a stub ASN not allocated in the graph)
	 * 
	 * @param filePathAnnouncements -> File path to the mrt announcements tsv
	 * @param config -> Configuration for how announcements ought to be seeded and tiebroken in the graph
	*/
	void SeedBlock(const std::string& filePathAnnouncements, const SeedingConfiguration& config);

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
     * The returned AS_PATH will have the origin at the end of the list, and the starting ASN will be at the beginning
     *
     * Also note that this is the control-plane traceback. This method will not look at the netmask or follow the most specific prefix in the AS.
     * Rather, it will just dumbly follow where this exact listed prefix goes
	 * 
     * @param as_path -> the list to add to containing the traceback of ASNS (this will not be cleared. Only appended to)
	 * @param startingASN  -> ASN to start at and trace back to the origin
	 * @param prefixBlockID -> ID of the prefix to trace
	*/
	void Traceback(std::vector<ASN> &as_path, const ASN startingASN, const uint32_t prefixBlockID) const;

	/**
     * Write the trace of every prefix in the provided ASes to a CSV file
	 * **WARNING** If no ASNs are specified, all local ribs are dumped to the file. 
	 *  - If this is the case, prepare your hard drive...
     *  - I'm serious. The amount of data this generates is absurd. Be *very* careful about how many RIBs are dumped (and how big they are)
     *  - For reference, about 4000 unique prefixes with 70,000 ASes will generate about 9GB of traces
	 * 
	 * @param resultsFilePath -> Path to the results file
	 * @param localRibsToDump -> ASNs of ASes to trace the route for all prefixes in the local rib
	*/
	void GenerateTracebackResultsCSV(const std::string& resultsFilePath, std::vector<ASN> localRibsToDump);

    // **** Getters **** //

    inline bool IsStub(const ASN asn) const { return stubASNToProviderID.find(asn) != stubASNToProviderID.end(); }
    inline ASN_ID GetProviderIDOfStubASN(const ASN stubASN) const { return stubASNToProviderID.at(stubASN); }

    inline bool ContainsASN(const ASN asn) const { return asnToID.find(asn) != asnToID.end(); }
	inline ASN GetASN(const ASN_ID id) const { return idToASN[id]; }
    inline ASN_ID GetASNID(const ASN asn) const { return asnToID.at(asn); }

	inline AnnouncementCachedData& GetCachedData(const ASN_ID& asnID, const uint32_t& prefixBlockID) {
		return localRibs.GetAnnouncement(asnID, prefixBlockID);
	}

    /**
     * Returns a const reference to the announcement that cannot be modified
     */
    inline const AnnouncementCachedData& GetCachedData_ReadOnly(const ASN_ID& asnID, const uint32_t& prefixBlockID) const {
		return localRibs.GetAnnouncement_ReadOnly(asnID, prefixBlockID);
	}

	inline AnnouncementStaticData& GetStaticData(const size_t& index) {
		return announcementStaticData[index];
	}

    /**
     * Returns a const reference to the static data that cannot be modified
     */
	inline const AnnouncementStaticData& GetStaticData_ReadOnly(const size_t& index) const {
		return announcementStaticData[index];
	}

	inline const PropagationPolicy& GetPropagationPolicy(const ASN_ID& asnID) {
		return *idToPolicy[asnID];
	}

    inline size_t GetNumASes() const { return localRibs.GetNumASes(); }
    inline size_t GetNumPrefixes() const { return localRibs.GetNumPrefixes(); }

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
