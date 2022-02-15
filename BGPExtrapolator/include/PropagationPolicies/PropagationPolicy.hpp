#pragma once

#include "Graphs/Graph.hpp"

class PropagationPolicy {
public:
	const ASN asn;
	const ASN_ID asnID;

	PropagationPolicy(const ASN &asn, const ASN_ID &asnID) : asn(asn), asnID(asnID) {

	}

	virtual void ProcessProviderAnnouncements(Graph &graph, const std::vector<PropagationPolicy*> &providers) = 0;
	virtual void ProcessPeerAnnouncements(Graph& graph, const std::vector<PropagationPolicy*>& peers) = 0;
	virtual void ProcessCustomerAnnouncements(Graph& graph, const std::vector<PropagationPolicy*>& customers) = 0;
};
