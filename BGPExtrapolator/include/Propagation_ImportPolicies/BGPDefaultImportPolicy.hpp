#pragma once

#include <array>

#include "PropagationImportPolicy.hpp"

/**
 * The BGP policy is the vanilla behvior of an AS during propagation.
*/
class BGPPolicy final : public PropagationImportPolicy {
public:
protected:
    void ProcessRelationship(Graph& graph, const std::vector<ExportInformation> &exportInfo, const ASN_ASNID_PAIR &neighbor, const uint8_t& relationshipPriority) {
        ASN_ID neighborID = neighbor.id;
        ASN neighborASN = neighbor.asn;

        const auto numPrefixes = graph.GetNumPrefixes();
        for (uint32_t i = 0; i < numPrefixes; i++) {
            const ExportInformation &exp = exportInfo[i];
            if (!exp.IsEnabled())
                continue;

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

public:
    BGPPolicy(const ASN& asn, const ASN_ID& asnID) : PropagationImportPolicy(asn, asnID) {

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

    virtual void ProcessProviderAnnouncements(Graph& graph, const std::vector<ExportInformation> &exportInfo, const ASN_ASNID_PAIR &provider) {
        ProcessRelationship(graph, exportInfo, provider, RELATIONSHIP_PRIORITY_PROVIDER_TO_CUSTOMER);
    }

    virtual void ProcessPeerAnnouncements(Graph& graph, const std::vector<ExportInformation> &exportInfo, const ASN_ASNID_PAIR &peer) {
        ProcessRelationship(graph, exportInfo, peer, RELATIONSHIP_PRIORITY_PEER_TO_PEER);
    }

    virtual void ProcessCustomerAnnouncements(Graph& graph, const std::vector<ExportInformation> &exportInfo, const ASN_ASNID_PAIR &customer) {
        ProcessRelationship(graph, exportInfo, customer, RELATIONSHIP_PRIORITY_CUSTOMER_TO_PROVIDER);
    }
};
