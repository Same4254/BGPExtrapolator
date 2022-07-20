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
                    currentAnnouncement.SetPathLength(sendingAnnouncement.GetPathLength() + 1);
                    currentAnnouncement.SetRecievedFromID(neighborID);
                    currentAnnouncement.SetRelationship(relationshipPriority);
                    currentAnnouncement.SetStaticDataIndex(sendingAnnouncement.GetStaticDataIndex());
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
        if (sendingAnnouncement.isDefaultState() || currentAnnouncement.isSeeded())
            return false;

        if (currentAnnouncement.isDefaultState())
            return true;

        if (relationshipPriority > currentAnnouncement.GetRelationship()) {
            return true;
        } else if (relationshipPriority == currentAnnouncement.GetRelationship()) {
            if (sendingAnnouncement.GetPathLength() + 1 < currentAnnouncement.GetPathLength()) {
                return true;
            } else if (sendingAnnouncement.GetPathLength() + 1 == currentAnnouncement.GetPathLength()) {
                int64_t sendingTimestamp = graph.GetStaticData_ReadOnly(sendingAnnouncement.GetStaticDataIndex()).timestamp;
                int64_t currentTimestamp = graph.GetStaticData_ReadOnly(currentAnnouncement.GetStaticDataIndex()).timestamp;

                if (sendingTimestamp > currentTimestamp) {
                    return true;
                } else if (sendingTimestamp == currentTimestamp) {
                    ASN asnToCompare;
                    if (currentAnnouncement.GetRecievedFromID() == asnID && currentAnnouncement.GetPathLength() == 2)
                        asnToCompare = graph.GetStaticData_ReadOnly(currentAnnouncement.GetStaticDataIndex()).originASN;
                    else
                        asnToCompare = graph.GetASN(currentAnnouncement.GetRecievedFromID());

                    return neighborASN < asnToCompare;
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
