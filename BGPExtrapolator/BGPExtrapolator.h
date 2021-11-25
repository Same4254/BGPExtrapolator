#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>
#include <set>
#include <map>
#include <string>

#include <rapidcsv.h>

/*
Gao Rexford:
	Exporting to a provider: AS cannot export routes learned from other providers or peers
	Exporting to a peer: Can export routes from customers. Does NOT export routes from peers or providers.

Note:
	A seeded announcement does not get replaced during propagation.

Key changes:
	You don't *send* announcements to a queue.
		- A customer sending to a provider is *really* a provider going through its customers and seeing if they have anything of interest
		- Prevents excessive copying, which was a dynamic vector in the previous version of the extrapolator
		- Only copy an announcement from another AS into this AS when that announcement is better than the current accepted one

	Propagation of a relationship each is a specific method:
		- Prevents excessive branching in the core loop

	MH/tiebraking/randomization mode is a global feature known at the extrapolator-scope. 
		Do not decide this at the AS level, which must figure out the MH mode at the most iterated point in the code every single time

	Precompute the randomization hashes

	Overall minimize branching. We want this thing to be a straight forward process as much as possible
*/

#ifdef IPv6
#define IPType __int128
#else
#define IPType uint32_t
#endif

#define ASN uint32_t

//Index of the AS in contiguous memory
#define ASN_ID uint32_t

#define CUSTOMER_RELATIONSHIP_PRIORITY 2
#define PEER_RELATIONSHIP_PRIORITY 1

/**
 * The priority is how two announcements are compared. An announcement with a larger pariority should be chosen over a lesser priority
 * 
 * The union is used such that we may edit the individual components of the priority (without bit shifting)
 *	And also perform the simple integer comparison of two priorities
 * 
 * The order matters here. For example, a seeded announcement is better than any non-seeded announcement. Thus it takes up the highest bits. 
 * That way in an integer comparison the seeded bits make the seeded priority larger than any other priority.
 * 
 * The logic continues, where seeded is more important than the relationship. Relationship is more important than the path length.
 * 
 * The path length property is non-intuitive. Here, a smaller path length is preffered. 
 * However, to keep the simple integer comparison, the smaller path length must result in a larger integer in the field in the priority. 
 * Thus, we subtract from the maximum integer as we propagate rather than increase from 0. That way, a smaller path results in a larger priority.
 * 
 * The reservedField does nothing as of writing
*/
union Priority {
	struct {
		uint8_t reservedField;
		uint8_t pathLength;
		uint8_t relationship;
		uint8_t seeded;
	};

	uint32_t allFields;
};

struct Prefix {
	IPType address, netmask;
	uint32_t id, block_id;
};

/**
 * Static data of an announcement. This is information about the announcement that does not change through propagation
 * Splitting the static data out of the Announcement allows for more dynamic announcements to fit into the cache, since they will be smaller.
 * This is because all of this redundant information is no longer part of the announcement information that gets copied around.
*/
struct AnnouncementStaticData {
	ASN_ID origin;
	Prefix prefix;
	int64_t timestamp;
};

/**
 * Dynamic data of an announcement. This is information about the announcement that can change during the propagation stage.
 * This also includes a pointer to the static data it is associated with.
*/
struct AnnouncementDynamicData {
	ASN_ID received_from_asn;
	Priority priority;
	AnnouncementStaticData *static_data;
};

/**
 * Information that is stricly only necessary for proccessing announcements of an AS.
*/
struct ASProcessInfo {
	ASN_ID asn_id;
	ASN asn;

	//TODO: replace with prefix announcement map
	std::vector<AnnouncementDynamicData> loc_rib;
};

/**
 * All of the information about the graph, parsed from the csv files
*/
struct GraphHandle {
	//Takes the ASN and gets the corresponding ASN_ID. An ASN_ID is the index of the process info for that AS
	std::unordered_map<ASN, ASN_ID> asn_to_asn_id;

	//Do not move or append to these lists after initialization!!!
	//TODO: replace with a custom allocator and specific data structure for rank iteration and random access
	
	std::vector<ASProcessInfo> as_process_info;
	std::vector<std::vector<ASProcessInfo*>> as_process_info_by_rank;
	std::vector<std::vector<ASProcessInfo*>> as_id_to_providers;
	std::vector<std::vector<ASProcessInfo*>> as_id_to_peers;
	std::vector<std::vector<ASProcessInfo*>> as_id_to_customers;
};

/***************************** FILE I/O ******************************/

/**
 * Creates the extrapolator handle from a CSV of relationship information. The connections made will obey multi-home behavior and will be organized into ranks.
 * The given data should not contain strongly connected components (see Tarjan Algorithm).
 * 
 * @param file_path_relationships -> Filepath to CSV file containing relationship data
 * @return Handle to the created graph
*/
extern GraphHandle extrapolator_graph_from_relationship_csv(std::string file_path_relationships);

/**
 * Given the graph to seed and the file of announcements, seed the announcements in the graph.
 * 
 * If origin_only is set to true, then the announcement will only be seeded at the origin.
 * 
 * The difference between a seeded announcement and a propagated announcement is that the seeded announcement is known to be true. Thus in all cases the seeded announcement will have a higher priority than any propagated announcement.
 * This does not mean that a seeded announcement will always be accepted by a neighbor, since the seeded-ness of an announcement is not relevant to the propagation TO the neighbor. 
 * Rather, seeded-ness means that this announcement will never be replaced with another announcement for the given prefix
 * 
 * @param gHandle 
 * @param file_path_announcements 
 * @param origin_only 
*/
extern void extrapolator_graph_seed_from_csv(GraphHandle& gHandle, std::string file_path_announcements, bool origin_only);

extern void extrapolator_graph_results_csv_from_handle(GraphHandle& gHandle, std::string results_file_path);

/***************************** INITIALIZATION ******************************/

/**
 * Clears the announcements in the graph. All relationships are preserved and no memory is deallocated. 
 * Priorities are reset to the initial state to signify invalid announcements. 
 * 
 * **All other fields of the announcements are left untouched**
 * 
 * @param gHandle -> The Graph to clear
 */
extern void extrapolator_graph_clear_announcements(GraphHandle& gHandle);

/***************************** PROPAGATION ******************************/

/**
 * Checks if the given AS_PATH contains a loop.
 * We may NOT define a loop as: "if it contains any duplicate ASNs" because of prepending
 *
 * We must see if an ASN is withing the path more than once when its instances are separated.
 *
 * Example of a loop: { 1, 2, 2, 2, 3, 4, 2 }
 * Example of NOT a loop: { 1, 2, 2, 2, 3, 4 }
 * 
 * Should only be used in debug mode, as the idea data will not contain loops, making this uneccessary
 *
 * @param path -> Path to check if contains a loop
 * @return Whether there is a loop in the path
*/
extern bool extrapolator_path_contains_loop(const std::vector<ASN>& as_path);

extern void extrapolator_propagate(GraphHandle& gHandle);

extern void extrapolator_as_process_customer_announcements(ASProcessInfo& reciever, std::vector<ASProcessInfo*>& customers);
extern void extrapolator_as_process_peer_announcements(ASProcessInfo& reciever, std::vector<ASProcessInfo*>& peers);
extern void extrapolator_as_process_provider_announcements(ASProcessInfo& reciever, std::vector<ASProcessInfo*>& providers);
