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

/**
 * Some serious data packing. Performance is directly proportional to the size
 * of this structure
 */
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

// For bit engineered smaller announcements, this is a possibility, but it does not work entirely correctly as of writing
// Something to look into in the future if RAM and performance is really still a problem

//#define BITS_RECIEVED_FROM_ID 0x1FFFF
//#define BITS_ROA_VALID 0x20000
//#define BITS_RELATIONSHIP 0xC0000
//#define BITS_PATH_LENGTH 0x7F000000
//#define BITS_SEEDED 0x80000000
//#define BITS_SEEDED_AND_PATH_LENGTH BITS_SEEDED | BITS_PATH_LENGTH
//
///**
// * Structure of the fields in bits (inclusive):
// *
// * (0 - 16): recievedFromID
// * (17): roa valid
// * (18 - 19): relationship
// *
// * (23 - 31) last 8 bits: seeded (highest bit), 7 bits for pathlength
// *
// */
//struct AnnouncementCachedData {
//private:
//    uint32_t staticDataIndex;
//    uint32_t fields;
//
//public:
//    AnnouncementCachedData() {
//        SetDefaultState();
//    }
//
//    inline void SetDefaultState() {
//        this->staticDataIndex = 0;
//        this->fields = 0;
//    }
//
//    inline bool isDefaultState() const { return GetPathLength() == 0; }
//
//    inline bool isSeeded() const { return (fields & BITS_SEEDED) != 0; }
//    inline uint8_t GetPathLength() const { return (fields & BITS_PATH_LENGTH) >> 23; }
//    inline uint8_t GetRelationship() const { return (fields & BITS_RELATIONSHIP) >> 18; }
//    inline ASN_ID GetRecievedFromID() const { return (fields & BITS_RECIEVED_FROM_ID); }
//    inline uint32_t GetStaticDataIndex() const { return staticDataIndex; } 
//
//    //number = (number & ~(1UL << n)) | (x << n);
//    inline void SetSeeded(const bool seeded) { fields = (fields & ~(1UL << 31)) | (((uint32_t) seeded) << 31); }
//    inline void SetRecievedFromID(const ASN_ID recievedFromID) { fields = (fields & ~(BITS_RECIEVED_FROM_ID)) | recievedFromID; }
//    inline void SetStaticDataIndex(const uint32_t staticDataIndex) { this->staticDataIndex = staticDataIndex; }
//    inline void SetPathLength(const uint8_t pathLength) { fields = (fields & ~(BITS_PATH_LENGTH)) | (((uint32_t) pathLength) << 23); }
//    inline void SetRelationship(const uint8_t relationship) { fields = (fields & ~(BITS_RELATIONSHIP)) | (((uint32_t) relationship) << 18); }
//};
