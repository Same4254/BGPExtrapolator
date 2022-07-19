#pragma once

#include <array>

#include "PropagationPolicy.hpp"

/**
 * The BGP policy is the vanilla behvior of an AS during propagation.
*/
class BGPPolicy final : public PropagationPolicy {
public:
protected:
    virtual void ProcessRelationship(Graph& graph, const std::vector<ASN_ASNID_PAIR>& neighbors, const uint8_t& relationshipPriority) {
        for (ASN_ASNID_PAIR neighbor : neighbors) {
            ASN_ID neighborID = neighbor.id;
            ASN neighborASN = neighbor.asn;

            const auto numPrefixes = graph.GetNumPrefixes();
            for (uint32_t i = 0; i < numPrefixes; i++) {
                AnnouncementCachedData& currentAnnouncement = graph.GetCachedData(asnID, i);
                AnnouncementCachedData& sendingAnnouncement = graph.GetCachedData(neighborID, i);

                if (CompareAnnouncements(graph, currentAnnouncement, neighborASN, sendingAnnouncement, relationshipPriority)) {
                    currentAnnouncement.pathLength = sendingAnnouncement.pathLength + 1;
                    currentAnnouncement.recievedFromASN = neighborASN;
                    currentAnnouncement.relationship = relationshipPriority;
                    currentAnnouncement.staticDataIndex = sendingAnnouncement.staticDataIndex;
                }
            }
        }
    }

public:
    BGPPolicy(const ASN& asn, const ASN_ID& asnID) : PropagationPolicy(asn, asnID) {

    }

    /**
     * Compares two announcements and returns whether the sender announcement should replace the reciever announcement in the reciever's local rib.
     * 
     * @param graph 
     * @param recieverAnnouncement 
     * @param sender 
     * @param senderAnnouncement 
     * @param relationshipPriority 
     * @return (true) if the sending announcement should replace the current announcement. False if it should not.
    */
    inline bool CompareAnnouncements(const Graph& graph, const AnnouncementCachedData& currentAnnouncement, const ASN neighborASN, const AnnouncementCachedData& sendingAnnouncement, const uint8_t& relationshipPriority) {
        if (sendingAnnouncement.isDefaultState() || currentAnnouncement.seeded)
            return false;

        if (currentAnnouncement.isDefaultState())
            return true;

        if (relationshipPriority > currentAnnouncement.relationship) {
            return true;
        } else if (relationshipPriority == currentAnnouncement.relationship) {
            if (sendingAnnouncement.pathLength + 1 < currentAnnouncement.pathLength) {
                return true;
            } else if (sendingAnnouncement.pathLength + 1 == currentAnnouncement.pathLength) {
                int64_t sendingTimestamp = graph.GetStaticData_ReadOnly(sendingAnnouncement.staticDataIndex).timestamp;
                int64_t currentTimestamp = graph.GetStaticData_ReadOnly(currentAnnouncement.staticDataIndex).timestamp;

                if (sendingTimestamp > currentTimestamp) {
                    return true;
                } else if (sendingTimestamp == currentTimestamp) {
                    return neighborASN < currentAnnouncement.recievedFromASN;
                }
            }
        }

        return false;
    }

    virtual void ProcessProviderAnnouncements(Graph& graph, const std::vector<ASN_ASNID_PAIR>& providerIDs) {
        ProcessRelationship(graph, providerIDs, RELATIONSHIP_PRIORITY_PROVIDER_TO_CUSTOMER);
    }

    virtual void ProcessPeerAnnouncements(Graph& graph, const std::vector<ASN_ASNID_PAIR>& peerIDs) {
        ProcessRelationship(graph, peerIDs, RELATIONSHIP_PRIORITY_PEER_TO_PEER);
    }

    virtual void ProcessCustomerAnnouncements(Graph& graph, const std::vector<ASN_ASNID_PAIR>& customerIDs) {
        ProcessRelationship(graph, customerIDs, RELATIONSHIP_PRIORITY_CUSTOMER_TO_PROVIDER);
    }
};
