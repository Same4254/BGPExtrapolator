#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>
#include <set>
#include <map>
#include <string>
#include <fstream>

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

//TODO: should these be const varaibles and typedefs? *Technically* a little more type-safe

//Yes, the ASN and ASN_ID are the same type. However, I want to distinguish when either is being used because they are very different in principle
#define ASN uint32_t

//Index of the AS in contiguous memory
#define ASN_ID uint32_t

#define RELATIONSHIP_PRIORITY_ORIGIN 3
#define RELATIONSHIP_PRIORITY_CUSTOMER_TO_PROVIDER 2
#define RELATIONSHIP_PRIORITY_PEER_TO_PEER 1
#define RELATIONSHIP_PRIORITY_PROVIDER_TO_CUSTOMER 0
#define RELATIONSHIP_PRIORITY_BROKEN 0

//This is the value that a path length of 0 will have (see comments on the Priority struct)
#define MIN_PATH_LENGTH 0xff

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
	 * Gloabl_Id and Block_Id
	 *  Every prefix is given a continuous index (global_id). The IDs start at 0 and increment up.
	 *	The setup of the dataset is to separate the data into blocks (such that we do not fill up RAM during propagation).
	 *	Because of this separation of data, each prefix is given an id for the block of prefixes only (block_id). 
	 *  This means that block_id's are continguous throughout that block.
	 *	The whole point of these IDs is so announcement lookup for a prefix in a local rib is an array access, where the index is the block_id
	 *	This is apposed to some kind of hash map or RB tree (announcement lookup in the local rib is in the hottest path in the propagation stage)
	*/
	struct Prefix {
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

		std::string prefix_string;
	};

	/**
	 * Dynamic data of an announcement. This is information about the announcement that can change during the propagation stage.
	 * This also includes a pointer to the static data it is associated with.
	 * 
	 * PERF_TODO: the pointer to static data could be replaced with a 32 bit index rather than a pointer. This may improve cache performance? Announcement size would go down, but it would require a refrence to the static announcement structure...
	*/
	struct AnnouncementDynamicData {
		ASN_ID received_from_id;
		ASN received_from_asn;
		Priority priority;
		AnnouncementStaticData* static_data;//NOTE: this struct does not own the static data pointer and is NOT responsible for cleaning it up

		/**
		* This is the "constructor" of sorts. 
		* Since this tool allocates all of the memory upfront, and never reconstructs the local ribs, this method should be used anytime the entire announcement is set
		*/
		void fill(const ASN_ID& received_from_id, const ASN& received_from_asn, const Priority& priority, AnnouncementStaticData* static_data);

		/**
		 * Resets the announcement data to an initial state. Nothing is deallocated (including the static_data pointer)
		*/
		void reset();
	};

	/**
	 * Information that is necessary for proccessing announcements of an AS.
	 * This means that when we want one AS to pull all of the announcements from a neighbor, all that is needed is the two ASProcessInfo structs to make this decision
	*/
	struct ASProcessInfo {
		// Contiguous index of ASes for vector lookups
		ASN_ID asn_id;
		ASN asn;

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
		//TODO: these vectors could be replaced with a different structure. Something fixed in size, but whose size is determined at runtime (so an array won't work here).
		// This is guarentee that pointers will remain valid and that no memory is allocated/de-allocated during runtime (it should only do this at initilization and when the program terminates)

		//Takes the ASN and gets the corresponding ASN_ID. An ASN_ID is the index of the process info for that AS
		//PERF_TODO: could a linear vector lookup be faster?

		//PERF_TODO: is ASN_ID neccessary? Would it be more plausible to have a biiig vector where the index is the ASN itself? This would remove mapping from ASN to ASN_ID
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

		/**
		 * Creates the extrapolator handle from a CSV of relationship information. The connections made will obey multi-home behavior and will be organized into ranks.
		 * The given data should not contain strongly connected components (see Tarjan Algorithm).
		 * 
		 * Local ribs and static information are allocated here, and only here, and never change thereafter. This is so the process never has to deal with dynamic memory reallocations after initialization.
		 *
		 * @param file_path_relationships -> Filepath to CSV file containing relationship data
		 * @param maximum_prefix_block_id -> maximum number of unique prefixes that will ever be seen in the dataset. Used to allocated local ribs once
		 * @param maximum_number_seeded_announcements -> maximum number of announcements that could possibly be seeded. Essentially, the maximum amount of rows in the announcements CSV. Used to allocate static information
		*/
		Graph(const std::string& file_path_relationships, size_t maximum_prefix_block_id, size_t maximum_number_seeded_announcements);

		/**
		 * Resets the announcements in the graph. All relationships are preserved and no memory is deallocated.
		 * - Priorities are reset to the initial state to signify invalid announcements (all fields set to 0).
		 * - Pointer to the static data is set to nullptr
		 */
		void reset_all_announcements();

		/**
		 * Resets the all of the **non-seeded** announcements in the graph. All relationships are preserved and no memory is deallocated.
		 * - Priorities are reset to the initial state to signify invalid announcements (all fields set to 0).
		 * - Pointer to the static data is set to nullptr
		 * 
		 * This could be used when doing multiple propagation techniques with the same starting seeded conditions. Thus, re-seeding the data would not be neccessary.
		 */
		void reset_all_non_seeded_announcements();

		/**
		* Seeds announcements into the graph for a given dataset, which has a maximum possible number of rows as specified in the constructor
		*
		* If origin_only is set to true, then the announcement will only be seeded at the origin.
		*
		* The difference between a seeded announcement and a propagated announcement is that the seeded announcement is known to be true. Thus in all cases the seeded announcement will have a higher priority than any propagated announcement.
		* This does not mean that a seeded announcement will always be accepted by a neighbor, since the seeded-ness of an announcement is not relevant to the propagation TO the neighbor.
		* Rather, seeded-ness means that this announcement will never be replaced with another announcement for the given prefix during propagation
		* 
		* During this stage, it may be possible to encounter several announcements for a prefix, in which case a tiebraking is performed on the timestamp.
		* How this works is dependent on the configuration of seeding. 
		* 
		* - First, timestamps are compared. Depending on the configuration options, older or newer timestamps may be preffered. 
		* 
		* - Next, the path lengths and relationships are compared (priority). Higher priority wins.
		* 
		* If timestamps are equal, there are then two options: Lowest ASN or Random decision.
		* 
		* - Lowest ASN compares the recieved_from_asn of the the current announcement with the ASN of the sender of the incoming announcement. If the current announcement was recieved from a lower ASN than the sender's ASN, then the incoming announcement is rejected.
		* If the sender has a lower ASN, then the announcement is accepted. This methodology is loosely based on the idea that routers will route to routers with a lower ID. If the current announcement is at its origin, then the ASN of the origin AS is compared to the sender ASN.
		* 
		* Cisco Best Path Selection Algorithm: https://www.cisco.com/c/en/us/support/docs/ip/border-gateway-protocol-bgp/13753-25.html
		* 
		* - A random decision will brake the tie on a seeded 50/50 chance. Used to measure a baseline of how much more accurate the tool is to randomness. 
		* 
		* See the UML document for a visual representation of this decision process
		* 
		* @param file_path_announcements -> file path to the CSV containing all of the announcements
		* @param origin_only -> Only seed the announcement at the origin of the path
		* @param prefer_new_timestamp -> Whether a new timestamp (larger) is preffered over older (smaller) timestamp
		* @param random_tiebraking -> How to handle tiebrakes. Uniform Random tiebraking (true) or lowest ASN (false)
		*/
		void seed_block_from_csv(std::string& file_path_announcements, bool origin_only, bool prefer_new_timestamp, bool random_tiebraking);

		/**
		 * For a specific path, seed the announcement along the path. See notes on "seed_block_from_csv" function for notes on how this works.
		 * 
		 * @param as_path -> The path to seed
		 * @param static_data -> Location of the static data structure to store constant information about the announcements. This pointer is given to all announcements that it is seeded with. Make sure this pointer stays valid!
		 * @param prefix -> Prefix that the announcement is for
		 * @param prefix_string -> The string representing the prefix from the dataset. Only used to output at then end for results.
		 * @param timestamp -> Timestamp of the announcement
		 * @param origin_only -> Whether to only seed the announcement at the origin
		 * @param prefer_new_timestamp -> Whether a new timestamp (larger) is preffered over older (smaller) timestamp
		 * @param random_tiebraking -> How to handle timestamp timebraking. Deterministic randomization based on the ASN (true) or keep the first announcement that was accepted (false)
		*/
		void seed_path(std::vector<ASN>& as_path, AnnouncementStaticData *static_data, Prefix& prefix, const std::string &prefix_string, int64_t timestamp, bool origin_only, bool prefer_new_timestamp, bool random_tiebraking);

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
		 *  - Peer routes: no
		 *  - Customer routes: yes
		 * 
		 * The main difference in seeding vs. propagation is that timestamp comparisons are first during seeding. During propagation, timestamp is a tiebraker.
		 * 
		 * If the timestamps are also equal, then the decision falls onto either a random decision or the lowest ASN. Timestamp comparison is also optional.
		 * 
		 * @param timestamp_tiebrake -> Whether to compare timestamps of the announcements
		 * @param prefer_new_timestamp -> If we are comparing timestamps, this states whether to prefer a new timestamp (larger) is over an older (smaller) timestamp
		 * @param random_tiebraking -> If timestamps were equal, or not compared at all, then either make a random decision (true) or prefer the lower ASN (false)
		*/
		void propagate(bool timestamp_tiebrake, bool prefer_new_timestamp, bool random_tiebraking);

		/**
		 * Generates CSV file of the loc_ribs. 
		 * 
		 * Announcements with no meaningful data (priority of 0) do not get exported to the file.
		 * 
		 * WARNING: The resulting file from a significant input can generate Herculean sized files (it is trivial to fill a hard drive with this output) when writing the local rib of all ASes. 
		 * 
		 * @param results_file_path -> File path for the file to write to. If it does not exist, it will be created. If it does exist, data will be deleted, tread carefully.
		*/
		void generate_results_csv(const std::string &results_file_path, const std::vector<ASN>& local_ribs_to_dump);

		/**
		 * Given an AS and a prefix, this will trace the path back to the origin and generate the AS_PATH. 
		 * 
		 * The origin will be at the end of the list (index = size - 1), starting AS (the given parameter) will be at index 0.
		 * 
		 * PERF_TODO: Would it be faster to simply generate and append to the string, rather than creating this list first?
		 * 
		 * @param process_info -> AS at the end of the AS_PATH
		 * @param prefix_block_id -> prefix to trace
		 * @return AS_PATH starting at the origin (end index) and ending at the given AS (index 0)
		*/
		std::vector<ASN> traceback(ASProcessInfo *process_info, uint32_t prefix_block_id);
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

	/*
	* PERF_TODO: 
	* 
	* The timestamp could go into the priority. This timestamp could be compressed to 32 bits by choosing the minimum timestamp in the data and making all other timestamps the difference from that. 
	* With 32 (unsigned int) bits the range is just over 60 years, well over even the existence of BGP itself.
	*/

	extern void as_process_customer_announcements(ASProcessInfo& reciever, std::vector<ASProcessInfo*>& customers, bool timestamp_tiebrake, bool prefer_new_timestamp, bool random_tiebraking);
	extern void as_process_peer_announcements(ASProcessInfo& reciever, std::vector<ASProcessInfo*>& peers, bool timestamp_tiebrake, bool prefer_new_timestamp, bool random_tiebraking);
	extern void as_process_provider_announcements(ASProcessInfo& reciever, std::vector<ASProcessInfo*>& providers, bool timestamp_tiebrake, bool prefer_new_timestamp, bool random_tiebraking);

	/**
	 * The goal of this function is to perform the comparison of priorities and if the other priotirty is better, then the announcement is accepted and copied in the local rib of the reciever at the given prefix_block_id.
	 * 
	 * The announcement from the other AS is a constant refrence. This is becuase it should be the actual refrence to the announcement in the local rib of the other AS. 
	 * The reason for this is to avoid constructing an entire temporary announcement with the new priority, only to deconstruct it immediately if it is regected.
	 * Rather, the guiding principle is to only copy an announcement when it is neccessary
	 * 
	 * @param reciever -> AS recieving the announcement. If accepted, the announcement will be placed into the local rib of this AS at the specified prefix_block_id index
	 * @param sender -> AS that is sending the announcement. Used to compare ASNs
	 * @param recieved_from_id -> The ASN_ID of the AS sending this announcement. Used for traceback in the results stage
	 * @param prefix_block_id -> index in the local ribs of the announcements in question
	 * @param other_announcement -> Constant refrence to the announcement in the sender's local rib
	 * @param temp_priority -> Priority containing the relationship and path length information if the announcement were to be accepted into the reciever's local rib
	*/
	extern inline void as_process_announcement(ASProcessInfo& reciever, const ASProcessInfo& sender, const ASN_ID& recieved_from_id, const uint32_t& prefix_block_id, const AnnouncementDynamicData& other_announcement, const Priority& temp_priority, bool timestamp_tiebrake, bool prefer_new_timestamp, bool random_tiebraking);
}