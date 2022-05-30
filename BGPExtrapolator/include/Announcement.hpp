#pragma once

#include <string>

#include "Defines.h"

struct Prefix {
    uint32_t global_id, block_id;
};

struct AnnouncementStaticData {
    ASN_ID origin;
    Prefix prefix;
    int64_t timestamp;

    std::string prefixString;
};

struct AnnouncementCachedData {
    ASN recievedFromASN;
    //AnnouncementStaticData* staticData;
    uint32_t staticDataIndex;

    uint8_t seeded;
    uint8_t pathLength;
    uint8_t relationship;
};
