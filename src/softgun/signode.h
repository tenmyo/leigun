/*
 **********************************************************************************
 * signode.h 
 *      Emulation of Signal nodes 
 *
 * (C) 2004  Lightmaze Solutions AG
 *   Author: Jochen Karrer
 *
 **********************************************************************************
 */
#ifndef SIGNODE_H
#define SIGNODE_H
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "strhash.h"
#include "compiler_extensions.h"

/*
 * ----------------------------------------------------------------------
 * Possible values for signal nodes
 * A meassured value can only be SIG_HIGH, SIG_LOW, or (currently) OPEN
 * ----------------------------------------------------------------------
 */
#define SIG_LOW				(0)
#define SIG_HIGH			(1)
#define SIG_OPEN			(2)
#define SIG_PULLDOWN		(3)
#define SIG_PULLUP			(4)
#define SIG_WEAK_PULLDOWN	(5)
#define SIG_WEAK_PULLUP		(6)
#define SIG_FORCE_LOW		(7)
#define SIG_FORCE_HIGH		(8)
#define SIG_ILLEGAL   (0x100)

typedef uint64_t SigStamp;

struct SigTrace;
struct SigLink;
typedef struct SigNode {
	uint32_t magic;
	SHashEntry *hash_entry;
	int selfval;
	int propval;
	SigStamp stamp;
	struct SigLink *linkList;
	struct SigTrace *sigTraceList;
} SigNode;

static inline const char *
SigName(SigNode * node)
{
	return SHash_GetKey(node->hash_entry);
}

typedef struct SigLink {
	struct SigLink *next;
	SigNode *partner;
} SigLink;

typedef void SigTraceProc(struct SigNode *, int value, void *clientData);
typedef void SigConflictProc(const char *msg);

typedef struct SigTrace {
	SigTraceProc *proc;
	void *clientData;
	struct SigTrace *next;
	int isactive;
} SigTrace;

void SignodesInit();
SigNode *SigNode_New(const char *format, ...) __attribute__ ((format(printf, 1, 2)));
void SigNode_Delete(SigNode * signode);
int SigNode_Set(SigNode * signode, int sigval);
SigTrace *SigNode_Trace(SigNode * node, SigTraceProc * proc, void *clientData);
int SigNode_Untrace(SigNode * node, SigTrace * trace);
int SigName_Link(const char *name1, const char *name2);
int SigNode_Link(SigNode * sig1, SigNode * sig2);
int SigNode_Linked(SigNode * sig1, SigNode * sig2);
int SigName_RemoveLink(const char *name1, const char *name2);
int SigNode_RemoveLink(SigNode * n1, SigNode * n2);
bool SigNode_IsTraced(SigNode * sig);
static inline int
SigNode_Val(SigNode * signode)
{
	return signode->propval;
}

static inline int
SigNode_State(SigNode * signode)
{
	return signode->selfval;
}

SigNode *SigNode_FindDominant(SigNode * sig);
void SigNode_Dump(SigNode * sig);
SigNode *SigNode_Find(const char *format, ...) __attribute__ ((format(printf, 1, 2)));
int SigEdge(SigNode * node);
/*
 **************************************************************************
 * Procedure to print an error message if a signal level conflict occurs
 **************************************************************************
 */
void Signodes_SetConflictProc(SigConflictProc * proc);

#endif
