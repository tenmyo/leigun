#ifndef XY_HASH_H
#define XY_HASH_H
#define XY_USER_HASHFUNC (0)
#define XY_ONE_WORD_KEYS (1)
#define XY_STRING_KEYS   (2)

typedef unsigned int HashValue;

typedef struct XY_HashEntry {
	const void *key;
	void *value;
	struct XY_HashEntry *next;
	struct XY_HashEntry *prev;
} XY_HashEntry;

typedef unsigned int XY_HashFunc(const void *data);
typedef int XY_IsEqualFunc(const void *data1, const void *data2);

typedef struct XY_HashTable {
	XY_HashFunc *hashfunc;
	XY_IsEqualFunc *isequal;
	XY_HashEntry **table;
	int nr_hashes;
} XY_HashTable;

typedef struct XY_HashSearch {
	int nr_hash;
	XY_HashEntry *cursor;
	XY_HashTable *hash;
} XY_HashSearch;

XY_HashEntry *XY_FindHashEntry(XY_HashTable * hash, const void *key);
XY_HashEntry *XY_FirstHashEntry(XY_HashTable * hash, XY_HashSearch * search);
XY_HashEntry *XY_NextHashEntry(XY_HashSearch * search);

int XY_InitHashTable(XY_HashTable * hash, int keytype, int size);
void XY_ClearHashTable(XY_HashTable * hash);

XY_HashEntry *XY_CreateHashEntry(XY_HashTable * hash, void *key, int *newptr);
XY_HashEntry *XY_AddHashEntry(XY_HashTable * hash, XY_HashEntry * newentry, const void *key);
void XY_RemoveHashEntry(XY_HashTable * hash, XY_HashEntry * entry);
void XY_DeleteHashEntry(XY_HashTable * hash, XY_HashEntry * entry);

static inline void
XY_SetHashValue(XY_HashEntry * entryPtr, void *value)
{
	entryPtr->value = value;
}

static inline void *
XY_GetHashValue(XY_HashEntry * entryPtr)
{
	return entryPtr->value;
}

static inline void
XY_SetHashFunc(XY_HashTable * table, XY_HashFunc * func)
{
	table->hashfunc = func;
}

static inline void
XY_SetIsEqualFunc(XY_HashTable * table, XY_IsEqualFunc * func)
{
	table->isequal = func;
}

#ifdef XY_HASH_STAT
void XY_HashStat(XY_HashTable * hash);
#endif

#endif
