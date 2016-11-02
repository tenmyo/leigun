/*
 *************************************************************************************************
 *
 * String Hash-Tables 
 * 
 * Copyright 2002 2003 2009 Jochen Karrer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Jochen Karrer ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Jochen Karrer.
 *
 *************************************************************************************************
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <strhash.h>
#include "sgstring.h"

static inline int
hash_string2(const char *s)
{
	unsigned int hash = 0;
	while (*s) {
		hash = *s + (hash << 6) + (hash << 16) - hash;
		s++;
	}
	return hash;
}

static unsigned int
hashkey_string(const void *data)
{
	char *str = (char *)data;
	unsigned long w;
	w = hash_string2(str);
	return (w + (w >> 10) + (w >> 20) + (w >> 30)) & 1023;
}

SHashEntry *
SHash_FindEntry(SHashTable * hash, const char *key)
{
	int h = hashkey_string(key);
	SHashEntry **first = &hash->table[h];
	SHashEntry *cursor;
	for (cursor = *first; cursor; cursor = cursor->next) {
		if (!strcmp(cursor->key, key)) {
			break;
		}
	}
	//if(cursor)printf("%s isequal %s\n",cursor->key,key);
	return cursor;
}

SHashEntry *
SHash_NextEntry(SHashSearch * search)
{
	SHashEntry *entry = search->cursor;
	SHashTable *hash = search->hash;
	if (entry->next) {
		search->cursor = entry->next;
		return search->cursor;
	}
	while (search->nr_hash < hash->nr_buckets) {
		SHashEntry **first = &hash->table[search->nr_hash];
		search->nr_hash++;
		if (*first) {
			search->cursor = *first;
			return *first;
		}
	}
	return NULL;
}

SHashEntry *
SHash_FirstEntry(SHashTable * hash, SHashSearch * search)
{
	search->hash = hash;
	for (search->nr_hash = 0; search->nr_hash < hash->nr_buckets;) {
		SHashEntry **first = &hash->table[search->nr_hash];
		search->nr_hash++;
		if (*first) {
			search->cursor = *first;
			return *first;
		}
	}
	return NULL;
}

void
SHash_DeleteEntry(SHashTable * hash, SHashEntry * entry)
{
	SHashEntry *prev = entry->prev;
	SHashEntry *next = entry->next;
	if (prev) {
		prev->next = next;
	} else {
		int h = hashkey_string(entry->key);
		SHashEntry **first = &hash->table[h];
		*first = next;
	}
	if (next)
		next->prev = prev;
	sg_free(entry->key);
	sg_free(entry);
}

void
SHash_ClearTable(SHashTable * hash)
{
	SHashEntry *cursor;
	int i;
	for (i = 0; i < hash->nr_buckets; i++) {
		SHashEntry **first = &hash->table[i];
		SHashEntry *next;
		for (cursor = *first; cursor; cursor = next) {
			next = cursor->next;
			sg_free(cursor->key);
			sg_free(cursor);
		}
		*first = NULL;
	}
	free(hash->table);
}

SHashEntry *
SHash_CreateEntry(SHashTable * hash, const char *key)
{
	int h = hashkey_string(key);
	SHashEntry **first = &hash->table[h];
	SHashEntry *cursor;
	SHashEntry *newentry;
	for (cursor = *first; cursor; cursor = cursor->next) {
		if (!strcmp(cursor->key, key)) {
			break;
		}
	}
	if (cursor) {
		return NULL;
	}
	newentry = sg_new(SHashEntry);
	newentry->next = *first;
	newentry->prev = NULL;
	if (*first)
		(*first)->prev = newentry;
	*first = newentry;
	newentry->key = sg_strdup(key);
	return newentry;
}

void
SHash_InitTable(SHashTable * hash)
{
	int size = 1024;
	int n_bytes = size * sizeof(void *);
	hash->table = (SHashEntry **) sg_calloc(n_bytes);
	hash->nr_buckets = size;
}

#ifdef SHASH_STAT
void
SHashStat(SHashTable * hash)
{
	int *stat = NULL;
	int max = 0;
	int count;
	int sum = 0;
	int sumquadrat = 0;
	int i, j;
	for (i = 0; i < hash->nr_buckets; i++) {
		SHashEntry **first = &hash->table[i];
		_SHashEntry *cursor;
		count = 0;
		for (cursor = *first; cursor; cursor = cursor->next) {
			count++;
		}
		if (count >= max) {
			stat = realloc(stat, sizeof(int) * (count + 1));
			for (j = max; j < count + 1; j++)
				stat[j] = 0;
			max = count + 1;
		}
		stat[count]++;
	}
	for (i = 0; i < max; i++) {
		if (stat[i]) {
			printf("%d times %d\n", stat[i], i);
		}
		sumquadrat += i * i * stat[i];
		sum += i * stat[i];
	}
	printf("sum %d,quadrat %f, optimum %f\n", sum, (float)sumquadrat / sum,
	       (float)sum / hash->nr_buckets);
}
#endif
