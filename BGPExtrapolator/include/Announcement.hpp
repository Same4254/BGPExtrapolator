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
	uint8_t seeded;
	uint8_t pathLength;
	uint8_t relationship;

	ASN recievedFromASN;
	AnnouncementStaticData* staticData;
};