#ifndef _SGTYPES_H
#define _SGTYPES_H
#include <stdint.h>

typedef struct FractionU64 {
	uint64_t nom;
	uint64_t denom;
} FractionU64_t;

typedef struct FractionS64 {
	int64_t nom;
	int64_t denom;
} FractionS64_t;

typedef struct FractionU32 {
	uint32_t nom;
	uint32_t denom;
} FractionU32_t;

typedef struct FractionS32 {
	int32_t nom;
	int32_t denom;
} FractionS32_t;

#define INLINE static inline

#endif
