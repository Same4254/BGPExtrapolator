#pragma once

#include <array>

#include "PropagationPolicy.hpp"

enum ComparisonResponse {
	ACCEPT, FALL_THROUGH, REJECT
};

//Function that compares a component of two announcements and returns the proper response due to the result of that comparison
typedef ComparisonResponse (*BGPComparisonFunction)(const Graph& graph, const PropagationPolicy* reciever, const AnnouncementCachedData& recieverAnnouncement, const PropagationPolicy* sender, const AnnouncementCachedData& senderAnnouncement, const uint8_t& relationshipPriority);

//***** COMPARISON FUNCTIONS ******/

static inline ComparisonResponse CompareRelationships(const Graph& graph, const PropagationPolicy* reciever, const AnnouncementCachedData& recieverAnnouncement, const PropagationPolicy* sender, const AnnouncementCachedData& senderAnnouncement, const uint8_t& relationshipPriority) {
	if (recieverAnnouncement.relationship > senderAnnouncement.relationship)
		return REJECT;
	else if (recieverAnnouncement.relationship < senderAnnouncement.relationship)
		return ACCEPT;
	return FALL_THROUGH;
}

static inline ComparisonResponse ComparePathLengths(const Graph& graph, const PropagationPolicy* reciever, const AnnouncementCachedData& recieverAnnouncement, const PropagationPolicy* sender, const AnnouncementCachedData& senderAnnouncement, const uint8_t& relationshipPriority) {
	if (recieverAnnouncement.pathLength > senderAnnouncement.pathLength - 1)
		return REJECT;
	else if (recieverAnnouncement.pathLength < senderAnnouncement.pathLength - 1)
		return ACCEPT;
	return FALL_THROUGH;
}

static inline ComparisonResponse CompareTimestampsPreferOld(const Graph& graph, const PropagationPolicy* reciever, const AnnouncementCachedData& recieverAnnouncement, const PropagationPolicy* sender, const AnnouncementCachedData& senderAnnouncement, const uint8_t& relationshipPriority) {
	if (recieverAnnouncement.staticData->timestamp < senderAnnouncement.staticData->timestamp)
		return REJECT;
	else if (recieverAnnouncement.staticData->timestamp > senderAnnouncement.staticData->timestamp)
		return ACCEPT;
	return FALL_THROUGH;
}

static inline ComparisonResponse CompareTimestampsPreferNew(const Graph& graph, const PropagationPolicy* reciever, const AnnouncementCachedData& recieverAnnouncement, const PropagationPolicy* sender, const AnnouncementCachedData& senderAnnouncement, const uint8_t& relationshipPriority) {
	if (recieverAnnouncement.staticData->timestamp > senderAnnouncement.staticData->timestamp)
		return REJECT;
	else if (recieverAnnouncement.staticData->timestamp < senderAnnouncement.staticData->timestamp)
		return ACCEPT;
	return FALL_THROUGH;
}

static inline ComparisonResponse CompareASNsPreferSmaller(const Graph& graph, const PropagationPolicy* reciever, const AnnouncementCachedData& recieverAnnouncement, const PropagationPolicy* sender, const AnnouncementCachedData& senderAnnouncement, const uint8_t& relationshipPriority) {
	if (reciever->asn < sender->asn)
		return REJECT;
	return ACCEPT;
}

static inline ComparisonResponse CompareRandom(const Graph& graph, const PropagationPolicy* reciever, const AnnouncementCachedData& recieverAnnouncement, const PropagationPolicy* sender, const AnnouncementCachedData& senderAnnouncement, const uint8_t& relationshipPriority) {
	if (rand() % 2 == 0)
		return ACCEPT;
	return REJECT;
}

/**
 * The BGP policy is the vanilla behvior of an AS during propagation.
 * 
 * This Policy should allow a variety of tiebraking methods, examples being timestamp comparison, lowest ASN, or complete randomization
 * The templating of this class allows the comparison operations of the policy to be unrolled at compile-time by the compiler and allow it to inline and optimize the comparison operations
 * Order of these comparison operations should be preserved with how they are present in the template parameters
*/
template<BGPComparisonFunction... T>
class BGPPolicy : public PropagationPolicy {
protected:
	//Write the functions to an array so it is iterable (with preserved order)
	std::array<BGPComparisonFunction, sizeof...(T)> comparisons = { T... };

	/**
	 * Compares two announcements and returns whether the sender announcement should replace the reciever announcement in the reciever's local rib.
	 * 
	 * @param graph 
	 * @param recieverAnnouncement 
	 * @param sender 
	 * @param senderAnnouncement 
	 * @param relationshipPriority 
	 * @return 
	*/
	inline bool CompareAnnouncements(const Graph& graph, const AnnouncementCachedData& recieverAnnouncement, const PropagationPolicy* sender, const AnnouncementCachedData& senderAnnouncement, const uint8_t& relationshipPriority) {
		//Since the comparison operations are known at compile time, this loop can be unrolled by the compiler and optimized. Minimizing the perfromance cost of allowing different comarison configurations easily.
		for (auto& func : comparisons) {
			ComparisonResponse response = func(graph, this, recieverAnnouncement, sender, senderAnnouncement, relationshipPriority);
			if (response == ACCEPT)
				return true;
			else if (response == REJECT)
				return false;
		}

		return false;
	}

	virtual void ProcessRelationship(Graph& graph, const std::vector<PropagationPolicy*>& neighbors, const uint8_t& relationshipPriority) {
		for (PropagationPolicy* neighbor : neighbors) {
			for (uint32_t i = 0; i < graph.GetNumPrefixes(); i++) {
				AnnouncementCachedData& currentAnnouncement = graph.GetCachedData(asnID, i);
				AnnouncementCachedData& sendingAnnouncement = graph.GetCachedData(neighbor->asnID, i);

				//Null static data means that the announcement does not exist (just allocated bytes of memory)
				//Don't replace a seeded announcement
				if (sendingAnnouncement.staticData == nullptr || currentAnnouncement.seeded)
					continue;

				if (CompareAnnouncements(graph, currentAnnouncement, neighbor, sendingAnnouncement, relationshipPriority)) {
					//Accept the incoming announcement to replace what is currently in the local rib of this AS
					currentAnnouncement.pathLength = sendingAnnouncement.pathLength - 1;
					currentAnnouncement.recievedFromASN = neighbor->asn;
					currentAnnouncement.relationship = relationshipPriority;
					currentAnnouncement.staticData = sendingAnnouncement.staticData;
				}
			}
		}
	}

public:
	BGPPolicy(const ASN& asn, const ASN_ID& asnID) : PropagationPolicy(asn, asnID) {

	}

	virtual void ProcessProviderAnnouncements(Graph& graph, const std::vector<PropagationPolicy*>& providers) {
		ProcessRelationship(graph, providers, RELATIONSHIP_PRIORITY_PROVIDER_TO_CUSTOMER);
	}

	virtual void ProcessPeerAnnouncements(Graph& graph, const std::vector<PropagationPolicy*>& peers) {
		ProcessRelationship(graph, peers, RELATIONSHIP_PRIORITY_PEER_TO_PEER);
	}

	virtual void ProcessCustomerAnnouncements(Graph& graph, const std::vector<PropagationPolicy*>& customers) {
		ProcessRelationship(graph, customers, RELATIONSHIP_PRIORITY_CUSTOMER_TO_PROVIDER);
	}
};
