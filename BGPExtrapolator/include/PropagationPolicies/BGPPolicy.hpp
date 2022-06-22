#pragma once

#include <array>

#include "PropagationPolicy.hpp"

/**
 * The BGP policy is the vanilla behvior of an AS during propagation.
 * 
 * This Policy should allow a variety of tiebraking methods, examples being timestamp comparison, lowest ASN, or complete randomization
 * The templating of this class allows the comparison operations of the policy to be unrolled at compile-time by the compiler and allow it to inline and optimize the comparison operations
 * Order of these comparison operations should be preserved with how they are present in the template parameters
*/
class BGPPolicy final : public PropagationPolicy {
public:
protected:
    virtual void ProcessRelationship(Graph& graph, const std::vector<ASN_ASNID_PAIR>& neighbors, const uint8_t& relationshipPriority) {
        for (ASN_ASNID_PAIR neighbor : neighbors) {
            ASN_ID neighborID = neighbor.id;
            ASN neighborASN = neighbor.asn;

            for (uint32_t i = 0; i < graph.GetNumPrefixes(); i++) {
                AnnouncementCachedData& currentAnnouncement = graph.GetCachedData(asnID, i);
                AnnouncementCachedData& sendingAnnouncement = graph.GetCachedData(neighborID, i);

                if (CompareAnnouncements(graph, currentAnnouncement, neighborASN, sendingAnnouncement, relationshipPriority)) {
                    AcceptAnnouncement(currentAnnouncement, sendingAnnouncement, neighborASN, relationshipPriority);
                }

                ////Null static data means that the announcement does not exist (just allocated bytes of memory)
                ////Don't replace a seeded announcement
                //if (sendingAnnouncement.isDefaultState() || currentAnnouncement.seeded)
                //    continue;

                //if (currentAnnouncement.isDefaultState()) {
                //    AcceptAnnouncement(currentAnnouncement, sendingAnnouncement, neighborASN, relationshipPriority);
                //}
                //
                //bool accept = false;
                //if (relationshipPriority > currentAnnouncement.relationship) {
                //    accept = true;
                //}
                //else if (relationshipPriority == currentAnnouncement.relationship) {
                //    if (sendingAnnouncement.pathLength + 1 < currentAnnouncement.pathLength) {
                //        accept = true;
                //    }
                //    else if (sendingAnnouncement.pathLength + 1 == currentAnnouncement.pathLength) {
                //        int64_t sendingTimestamp = graph.GetStaticData(sendingAnnouncement.staticDataIndex).timestamp;
                //        int64_t currentTimestamp = graph.GetStaticData(currentAnnouncement.staticDataIndex).timestamp;
                //
                //        if (sendingTimestamp > currentTimestamp) {
                //            accept = true;
                //        }
                //        else if (sendingTimestamp == currentTimestamp) {
                //            accept = neighborASN < currentAnnouncement.recievedFromASN;
                //        }
                //    }
                //}
                //
                //if (accept) {
                //    AcceptAnnouncement(currentAnnouncement, sendingAnnouncement, neighborASN, relationshipPriority);
                //}
            }
        }
    }

    inline void AcceptAnnouncement(AnnouncementCachedData &currentAnnouncement, const AnnouncementCachedData &sendingAnnouncement, const ASN neighborASN, const uint8_t relationshipPriority) {
        currentAnnouncement.pathLength = sendingAnnouncement.pathLength + 1;
        currentAnnouncement.recievedFromASN = neighborASN;
        currentAnnouncement.relationship = relationshipPriority;
        currentAnnouncement.staticDataIndex = sendingAnnouncement.staticDataIndex;
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
     * @return 
    */
    inline bool CompareAnnouncements(const Graph& graph, const AnnouncementCachedData& currentAnnouncement, const ASN neighborASN, const AnnouncementCachedData& sendingAnnouncement, const uint8_t& relationshipPriority) {
        //Null static data means that the announcement does not exist (just allocated bytes of memory)
                //Don't replace a seeded announcement
        if (sendingAnnouncement.isDefaultState() || currentAnnouncement.seeded)
            return false;

        if (currentAnnouncement.isDefaultState()) {
            return true;
        }

        if (relationshipPriority > currentAnnouncement.relationship) {
            return true;
        }
        else if (relationshipPriority == currentAnnouncement.relationship) {
            if (sendingAnnouncement.pathLength + 1 < currentAnnouncement.pathLength) {
                return true;
            }
            else if (sendingAnnouncement.pathLength + 1 == currentAnnouncement.pathLength) {
                int64_t sendingTimestamp = graph.GetStaticData_ReadOnly(sendingAnnouncement.staticDataIndex).timestamp;
                int64_t currentTimestamp = graph.GetStaticData_ReadOnly(currentAnnouncement.staticDataIndex).timestamp;

                if (sendingTimestamp > currentTimestamp) {
                    return true;
                }
                else if (sendingTimestamp == currentTimestamp) {
                    return neighborASN < currentAnnouncement.recievedFromASN;
                }
            }
        }

        return false;
    }

    /*inline bool CompareAnnouncements(const Graph& graph, const AnnouncementCachedData& currentAnnouncement, const ASN neighborASN, const AnnouncementCachedData& sendingAnnouncement, const uint8_t& relationshipPriority) {
    
        return false;
    }*/

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
