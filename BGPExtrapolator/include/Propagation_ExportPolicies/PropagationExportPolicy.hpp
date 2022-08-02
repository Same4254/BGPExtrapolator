#pragma once

#include <vector>

#include "Announcement.hpp"

class PropagationExportPolicy {
public: 
    const ASN asn;
    const ASN_ID asnID;

    PropagationExportPolicy(const ASN asn, const ASN_ID asnID) : asn(asn), asnID(asnID) {

    }

    virtual void FillExportInformation(std::vector<ExportInformation> &exportInfo, const ASN neighborASN, const ASN_ID neighborID) = 0;
};
