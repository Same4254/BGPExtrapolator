#pragma once

#include <array>

#include "PropagationPolicy.hpp"

enum ComparisonResponse {
	ACCEPT, FALL_THROUGH, REJECT
};

typedef ComparisonResponse (*BGPComparisonFunction)(const Graph& graph, const PropagationPolicy* reciever, const AnnouncementCachedData& recieverAnnouncement, const PropagationPolicy* sender, const AnnouncementCachedData& senderAnnouncement, const uint8_t& relationshipPriority);

template<BGPComparisonFunction... T>
class BGPPolicy : public PropagationPolicy {
protected:
	std::array<BGPComparisonFunction, sizeof...(T)> comparisons;

	inline bool CompareAnnouncements(const Graph& graph, const AnnouncementCachedData& recieverAnnouncement, const PropagationPolicy* sender, const AnnouncementCachedData& senderAnnouncement, const uint8_t& relationshipPriority) {
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

				if (sendingAnnouncement.staticData == nullptr || currentAnnouncement.seeded)
					continue;

				if (CompareAnnouncements(graph, currentAnnouncement, neighbor, sendingAnnouncement, relationshipPriority)) {
					currentAnnouncement.pathLength = sendingAnnouncement.pathLength - 1;
					currentAnnouncement.recievedFromASN = neighbor->asn;
					currentAnnouncement.relationship = relationshipPriority;
					currentAnnouncement.staticData = sendingAnnouncement.staticData;
				}
			}
		}
	}

public:
	BGPPolicy(const ASN& asn, const ASN_ID& asnID) : PropagationPolicy(asn, asnID), comparisons(T...) {

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
