#pragma once

#include "Graphs/Graph.hpp"

class PropagationImportPolicy {
public:
    const ASN asn;
    const ASN_ID asnID;

    PropagationImportPolicy(const ASN &asn, const ASN_ID &asnID) : asn(asn), asnID(asnID) {

    }

    /**
     * Compares the local rib of this AS with its providers and copies any announcements that are "better"
     * Path length priority should be adjusted to represent the hop from one AS to another.
     * 
     * @param graph 
     * @param providers 
    */
    virtual void ProcessProviderAnnouncements(Graph &graph, const std::vector<ExportInformation> &exportInfo, const ASN_ASNID_PAIR &provider) = 0;
    
    /**
     * Compares the local rib of this AS with its peers and copies any announcements that are "better"
     * Path length priority should be adjusted to represent the hop from one AS to another.
     *
     * @param graph
     * @param peers
    */
    virtual void ProcessPeerAnnouncements(Graph& graph, const std::vector<ExportInformation> &exportInfo, const ASN_ASNID_PAIR &peer) = 0;
    
    /**
     * Compares the local rib of this AS with its customers and copies any announcements that are "better"
     * Path length priority should be adjusted to represent the hop from one AS to another.
     *
     * @param graph
     * @param customers
    */
    virtual void ProcessCustomerAnnouncements(Graph& graph, const std::vector<ExportInformation> &exportInfo, const ASN_ASNID_PAIR &customer) = 0;
};
