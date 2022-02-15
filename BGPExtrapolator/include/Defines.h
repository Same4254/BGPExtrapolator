#pragma once

#include <cstdint>

//Yes, the ASN and ASN_ID are the same type. However, I want to distinguish when either is being used because they are very different in principle
#define ASN uint32_t

//Index of the AS in contiguous memory
#define ASN_ID uint32_t

#define RELATIONSHIP_PRIORITY_ORIGIN 3
#define RELATIONSHIP_PRIORITY_CUSTOMER_TO_PROVIDER 2
#define RELATIONSHIP_PRIORITY_PEER_TO_PEER 1
#define RELATIONSHIP_PRIORITY_PROVIDER_TO_CUSTOMER 0
#define RELATIONSHIP_PRIORITY_BROKEN 0

#define MIN_PATH_LENGTH 0xff

#define SEPARATED_VALUES_DELIMETER '\t'