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

    AnnouncementCachedData() {
        SetDefaultState();
    }

    inline void SetDefaultState() {
        this->recievedFromASN = 0;
        this->staticDataIndex = 0;
        this->seeded = 0;
        this->pathLength = 0;
        this->relationship = 0;
    }

    inline bool isDefaultState() { return pathLength == 0; }
};
