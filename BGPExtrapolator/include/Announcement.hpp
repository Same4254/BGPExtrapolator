#pragma once

#include <string>

#include "Defines.h"

struct Prefix {
    uint32_t global_id, block_id;
};

struct AnnouncementStaticData {
    ASN originASN;
    Prefix prefix;
    int64_t timestamp;

    std::string prefixString;
};

struct AnnouncementCachedData {
private:
    ASN_ID recievedFromID;
    uint32_t staticDataIndex;

    uint8_t seeded;
    uint8_t pathLength;
    uint8_t relationship;

public:
    AnnouncementCachedData() {
        SetDefaultState();
    }

    inline void SetDefaultState() {
        this->recievedFromID = 0;
        this->staticDataIndex = 0;
        this->seeded = 0;
        this->pathLength = 0;
        this->relationship = 0;
    }

    inline bool isDefaultState() const { return pathLength == 0; }

    inline bool isSeeded() const { return seeded; }
    inline uint8_t GetPathLength() const { return pathLength; }
    inline uint8_t GetRelationship() const { return relationship; }
    inline ASN_ID GetRecievedFromID() const { return recievedFromID; }
    inline uint32_t GetStaticDataIndex() const { return staticDataIndex; } 

    inline void SetSeeded(const bool seeded) { this->seeded = seeded; }
    inline void SetRecievedFromID(const ASN_ID recievedFromID) { this->recievedFromID = recievedFromID; }
    inline void SetStaticDataIndex(const uint32_t staticDataIndex) { this->staticDataIndex = staticDataIndex; }
    inline void SetPathLength(const uint8_t pathLength) { this->pathLength = pathLength; }
    inline void SetRelationship(const uint8_t relationship) { this->relationship = relationship; }
};
