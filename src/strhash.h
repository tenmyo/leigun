#ifndef _SHASH_H
#define _SHASH_H

typedef struct SHashEntry {
	char *key;
	void *value;
	struct SHashEntry *next;
	struct SHashEntry *prev;
} SHashEntry;

typedef struct SHashTable {
	SHashEntry **table;
	int nr_buckets;
} SHashTable;

typedef struct SHashSearch {
	int nr_hash;
	SHashEntry *cursor;
	SHashTable *hash;
} SHashSearch;

SHashEntry *SHash_FindEntry(SHashTable * hash, const char *key);
SHashEntry *SHash_FirstEntry(SHashTable * hash, SHashSearch * search);
SHashEntry *SHash_NextEntry(SHashSearch * search);

void SHash_InitTable(SHashTable * hash);
void SHash_ClearTable(SHashTable * hash);

SHashEntry *SHash_CreateEntry(SHashTable * hash, const char *key);
void SHash_DeleteEntry(SHashTable * hash, SHashEntry * entry);

static inline void
SHash_SetValue(SHashEntry * entryPtr, void *value)
{
	entryPtr->value = value;
}

static inline void *
SHash_GetValue(SHashEntry * entryPtr)
{
	return entryPtr->value;
}

static inline const char *
SHash_GetKey(SHashEntry * entryPtr)
{
	return entryPtr->key;
}
#endif
