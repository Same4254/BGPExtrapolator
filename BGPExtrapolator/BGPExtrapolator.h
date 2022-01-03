#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>
#include <set>
#include <map>
#include <string>

#include <rapidcsv.h>

/* ----General Notes----
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

	Overall minimize (unpredictable) branching. We want this thing to be a straight forward process as much as possible
*/

#ifdef IPv6
#define IPType __int128
#else
#define IPType uint32_t
#endif

//Yes, the ASN and ASN_ID are the same type. However, I want to distinguish when either is being used because they are very different in principle
#define ASN uint32_t

//Index of the AS in contiguous memory
#define ASN_ID uint32_t

#define RELATIONSHIP_PRIORITY_ORIGIN 3
#define RELATIONSHIP_PRIORITY_CUSTOMER 2
#define RELATIONSHIP_PRIORITY_PEER 1
#define RELATIONSHIP_PRIORITY_PROVIDER 0

//This is the value that a path length of 0 will have (see comments on the Prefix struct)
#define MIN_PATH_LENGTH 0xff

#define GALOIS_HASH_KEY 3

// Delimeter in the input CSV files
#define SEPARATED_VALUES_DELIMETER '\t'

//NOTE. "TODO" marks code improvements. "PERF_TODO" marks a *performance* suggestion that needs to be tested

namespace BGPExtrapolator {
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
	 * A path length of 0 means that the announcement is invalid.
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

	/**
	 * Address and Netmask: this is the actual network information. 
	 *	These values are the decimal conversion of their original form. 
	 *  For example an address of 192.168.1.1 is converted into bits such that the binary representation of 192 is the most significant bits
	 *	The netmask from the dataset states how many 1 bits there are. So the netmask representation here is an integer with that many bits set to 1, start from the least significant bits
	 * 
	 * Gloabl_Id and Block_Id
	 *  Every prefix is given a continuous index (global_id). The IDs start at 0 and increment up.
	 *	The setup of the dataset is to separate the data into blocks (such that we do not fill up RAM during propagation).
	 *	Because of this separation of data, each prefix is given an id for the block of prefixes only (block_id). 
	 *  This means that block_id's are continguous throughout that block.
	 *	The whole point of these IDs is so announcement lookup for a prefix in a local rib is an array access, where the index is the block_id
	 *	This is apposed to some kind of hash map or RB tree (announcement lookup in the local rib is in the hottest path in the propagation stage)
	*/
	struct Prefix {
		IPType address, netmask;
		uint32_t global_id, block_id;
	};

	/**
	 * Static data of an announcement. This is information about the announcement that does not change through propagation
	 * Splitting the static data out of the Announcement allows for more dynamic announcements to fit into the cache, since they will be smaller.
	 * This is because all of this redundant information is no longer part of the announcement information that gets copied around.
	 * This is important for performance because during propagation we want as many announcements in the cache as physically possible.
	*/
	struct AnnouncementStaticData {
		ASN_ID origin;
		Prefix prefix;
		int64_t timestamp;
	};

	/**
	 * Dynamic data of an announcement. This is information about the announcement that can change during the propagation stage.
	 * This also includes a pointer to the static data it is associated with.
	 * 
	 * PERF_TODO: the pointer to static data could be replaced with a 32 bit index rather than a 64 bit pointer. This may improve cache performance
	*/
	struct AnnouncementDynamicData {
		ASN_ID received_from_id;
		Priority priority;
		AnnouncementStaticData* static_data;
	};

	/**
	 * Information that is necessary for proccessing announcements of an AS.
	 * This means that when we want one AS to pull all of the announcements from a neighbor, all that is needed is the two ASProcessInfo structs to make this decision
	*/
	struct ASProcessInfo {
		ASN_ID asn_id;// Contiguous index of ASes for vector lookups
		ASN asn;// Actual AS number for the AS

		//Precomputed random boolean for tiebraking. This randomization is determined by the ASN to be deterministic. See tiny_hash for implementation details
		bool rand_tiebrake_value;

		//All of the locally accepted announcements. Indexed by the prefix block_id. 
		//Note, that this is pre-allocated to fit all prefixes. There may be unused bytes (represented by a priority of 0)
		std::vector<AnnouncementDynamicData> loc_rib;
	};

	/**
	 * For a given As, this is the relationship information in the topology. 
	 * Used during the seeding process to determine priority, and in the initilization stage to keep track of what AS is in what rank
	 * 
	 * PERF_TODO: check and test if a set is what we should be using here. Does this have a strong impact on performance? (doubtful. This is not on the hot path). Could a linear vector lookup be faster?
	*/
	struct ASRelationshipInfo {
		ASN_ID asn_id;
		ASN asn;

		int rank;
		std::set<ASN> peers, customers, providers;
	};

	/**
	 * The Graph is capable of taking in a CSV file that describes its topology, taking in another csv file for seeding the announcements, propagating those announcements, and outputting the results
	 * Vectors are indexed by the ASN_ID
	*/
	class Graph {
	public:
		//Takes the ASN and gets the corresponding ASN_ID. An ASN_ID is the index of the process info for that AS
		//PERF_TODO: could a linear vector lookup be faster?
		std::unordered_map<ASN, ASN_ID> asn_to_asn_id;
		std::vector<ASRelationshipInfo> as_id_to_relationship_info;

		//Do not move or append to these lists after initialization!!!
		//TODO: could replace with a custom allocator and specific data structure for rank iteration and random access
		std::vector<ASProcessInfo> as_process_info;
		std::vector<std::vector<ASProcessInfo*>> as_process_info_by_rank;
		std::vector<std::vector<ASProcessInfo*>> as_id_to_providers;
		std::vector<std::vector<ASProcessInfo*>> as_id_to_peers;
		std::vector<std::vector<ASProcessInfo*>> as_id_to_customers;

		std::vector<AnnouncementStaticData> announcement_static_data;

		rapidcsv::Document relationships_csv, announcements_csv;

		/**
		 * Creates the extrapolator handle from a CSV of relationship information. The connections made will obey multi-home behavior and will be organized into ranks.
		 * The given data should not contain strongly connected components (see Tarjan Algorithm).
		 *
		 * @param file_path_relationships -> Filepath to CSV file containing relationship data
		 * @param fuke_path_announcements -> Filepath to CSV file containing announcements separated into blocks that will be seeded into the graph
		*/
		Graph(const std::string& file_path_relationships, const std::string &file_path_announcements);

		/**
		 * Resets the announcements in the graph. All relationships are preserved and no memory is deallocated.
		 * - Priorities are reset to the initial state to signify invalid announcements (all fields set to 0).
		 * - Pointer to the static data is set to nullptr
		 *
		 * **All other fields of the announcements are left untouched**
		 */
		void reset_announcements();

		/**
		* Seeds announcements into the graph for a given block. Data is retrieved from announcements file given in the constructor.
		*
		* If origin_only is set to true, then the announcement will only be seeded at the origin.
		*
		* The difference between a seeded announcement and a propagated announcement is that the seeded announcement is known to be true. Thus in all cases the seeded announcement will have a higher priority than any propagated announcement.
		* This does not mean that a seeded announcement will always be accepted by a neighbor, since the seeded-ness of an announcement is not relevant to the propagation TO the neighbor.
		* Rather, seeded-ness means that this announcement will never be replaced with another announcement for the given prefix during propagation
		* 
		* During this stage, it may be possible to encounter several announcements for a prefix, in which case a tie_braking is performed on the timestamp (a bigger/newer timestamp is prefered).
		* If timestamps are equal, then it could be determined by randomization (hash of the ASN) when random_tiebraking is true or the first announcement that arrived is kept when random_tiebraking is false
		*
		* @param block -> Which block of the data to seed into the graph (a csv file may contiain several blocks of announcements, we want to handle them one at a time)
		* @param origin_only -> Only seed the announcement at the origin of the path
		* @param random_tiebraking -> How to handle timestamp timebraking. Deterministic randomization based on the ASN (true) or keep the first announcement that was accepted (false)
		*/
		void seed_block_from_csv(size_t block, bool origin_only, bool random_tiebraking);

		/**
		 * For a specific path, seed the announcement along the path. See notes on "seed_block_from_csv" function for notes on how this works.
		 * 
		 * @param as_path -> The path to seed
		 * @param static_data -> Location of the static data structure to store constant information about the announcements. This pointer is given to all announcements that it is seeded with. Make sure this pointer stays valid!
		 * @param prefix -> Prefix that the announcement is for
		 * @param timestamp -> Timestamp of the announcement
		 * @param origin_only -> Whether to only seed the announcement at the origin
		 * @param random_tiebraking -> How to handle timestamp timebraking. Deterministic randomization based on the ASN (true) or keep the first announcement that was accepted (false)
		*/
		void seed_path(std::vector<ASN>& as_path, AnnouncementStaticData *static_data, Prefix& prefix, int64_t timestamp, bool origin_only, bool random_tiebraking);

		/**
		 * Allow the seeded announcements to propagate throughout the graph, obeying Gao Rexford rules. 
		 * 
		 * Original Paper: "Stable Internet Routing Without Global Coordination" by Lixin Gao and Jennifer Rexford.
		 * 
		 * Export to Provider:
		 *  - Provider routes: no
		 *  - Peer routs: no
		 *  - Customer routes: yes
		 * 
		 * Export to Customer:
		 *  - Provider routes: yes
		 *  - Peer routes: yes
		 *  - Customer routes: no
		 * 
		 * Export to Peer:
		 *  - Provider routes: no
		 *  - Peer routs: no
		 *  - Customer routes: yes
		*/
		void propagate();

		/**
		 * Generates CSV file of the loc_ribs. 
		 * 
		 * Announcements with no meaningful data (priority of 0) do not get exported to the file.
		 * 
		 * WARNING: The resulting file from a significant input can generate Herculean sized files (it is trivial to fill a hard drive with this output) when writing the local rib of all ASes. 
		 * 
		 * @param results_file_path -> File path for the file to write to. If it does not exist, it will be created. If it does exist, data will be deleted, tread carefully.
		*/
		void generate_results_csv(const std::string &results_file_path);
	};

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
	extern bool path_contains_loop(const std::vector<ASN>& as_path);

	/**
	 * Converts the string representing a list of ASNs into a vector, with preserved ordering. The origin is placed at the end of the vector.
	 * 
	 * Input Format:
	 *  "{1,2,3}"
	 * 
	 * Where 3 is the origin if the input is an AS_PATH. The returned list will place 3 at the end of the vector (index being size - 1).
	 * 
	 * This may also be used as a general parser for a list.
	 * 
	 * ASSUMPTION: the string does not contain the number 0.
	 * 
	 * @param as_path_string -> String representing the path
	 * @return A vector containing the path, with ordering preserved. Origin is at the end of the vector.
	*/
	extern std::vector<ASN> parse_ASN_list(const std::string &as_path_string);

	extern Prefix cidr_string_to_prefix(const std::string &s);
	extern std::string prefix_to_cidr_string(const Prefix& prefix);

	extern uint8_t galois_hash(const ASN &asn);

	extern void as_process_customer_announcements(ASProcessInfo& reciever, std::vector<ASProcessInfo*>& customers);
	extern void as_process_peer_announcements(ASProcessInfo& reciever, std::vector<ASProcessInfo*>& peers);
	extern void as_process_provider_announcements(ASProcessInfo& reciever, std::vector<ASProcessInfo*>& providers);

	extern inline void as_process_announcement(ASProcessInfo& reciever, const ASN_ID& recieved_from_id, const uint32_t& prefix_block_id, const AnnouncementDynamicData& other_announcement, const Priority& temp_priority);
	extern inline void as_process_announcement_random_tiebrake(ASProcessInfo& reciever, const ASN_ID& recieved_from_id, const uint32_t& prefix_block_id, const AnnouncementDynamicData& other_announcement, const Priority& temp_priority, bool tiebrake_keep_original_ann);
}