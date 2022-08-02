#pragma once

#include "Propagation_ExportPolicies/PropagationExportPolicy.hpp"

class ExportAllPolicy : public PropagationExportPolicy {
public:
    ExportAllPolicy(const ASN asn, ASN_ID asnID) : PropagationExportPolicy(asn, asnID) {
        
    }

    void FillExportInformation(std::vector<ExportInformation> &exportInfo, const ASN neighborASN, const ASN_ID neighborID) {
        // Export information defaults to export all...
        return;
    }
};
