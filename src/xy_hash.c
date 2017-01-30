/*
 *************************************************************************************************
 *
 * Hash-Tables taken from the XY-Library	 
 * 
 * Copyright 2002 2003 Jochen Karrer. All rights reserved.
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
#include <xy_hash.h>
#include "sgstring.h"

static unsigned int
hashkey_one_word32(const void *data)
{
	uint32_t w = (unsigned long)data;
	return (w + (w >> 5) + (w >> 10) + (w >> 15) + (w >> 20) + (w >> 25) + (w >> 30)) & 31;
}

static unsigned int
hashkey_one_word256(const void *data)
{
	uint32_t w = (unsigned long)data;
	return (w + (w >> 24) + (w >> 16) + (w >> 8)) & 255;
}

static unsigned int
hashkey_one_word1024(const void *data)
{
	uint32_t w = (unsigned long)data;
	return (w + (w >> 10) + (w >> 20) + (w >> 30)) & 1023;
}

static unsigned int
hashkey_one_word8192(const void *data)
{
	uint32_t w = (unsigned long)data;
	return (w + (w >> 13) + (w >> 26)) & 8191;
}

static int
one_word_isequal(const void *data1, const void *data2)
{
	return ((unsigned long)data1 == (unsigned long)data2);
}

static int
string_isequal(const void *d1, const void *d2)
{
	char *str1 = (char *)d1;
	char *str2 = (char *)d2;
	return (strcmp(str1, str2) == 0);
}

static int
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
hashkey_string32(const void *data)
{
	char *str = (char *)data;
	unsigned long w;
	w = hash_string2(str);
	return (w + (w >> 5) + (w >> 10) + (w >> 15) + (w >> 20) + (w >> 25) + (w >> 30)) & 31;
}

static unsigned int
hashkey_string256(const void *data)
{
	char *str = (char *)data;
	unsigned long w;
	w = hash_string2(str);
	return (w + (w >> 24) + (w >> 16) + (w >> 8)) & 255;
}

static unsigned int
hashkey_string1024(const void *data)
{
	char *str = (char *)data;
	unsigned long w;
	w = hash_string2(str);
	return (w + (w >> 10) + (w >> 20) + (w >> 30)) & 1023;
}

static unsigned int
hashkey_string8192(const void *data)
{
	char *str = (char *)data;
	unsigned long w;
	w = hash_string2(str);
	return (w + (w >> 13) + (w >> 26)) & 8191;
}

static unsigned int
hashkey_string65536(const void *data)
{
	char *str = (char *)data;
	unsigned long w;
	w = hash_string2(str);
	return (w + (w >> 16) + (w >> 12) - (w >> 3)) & 65535;
}

XY_HashEntry *
XY_FindHashEntry(XY_HashTable * hash, const void *key)
{
	int h = hash->hashfunc(key);
	XY_HashEntry **first = &hash->table[h];
	XY_HashEntry *cursor;
	//printf("searche for key %s\n",key);
	for (cursor = *first; cursor; cursor = cursor->next) {
		if (hash->isequal(cursor->key, key)) {
			break;
		}
	}
	//if(cursor)printf("%s isequal %s\n",cursor->key,key);
	return cursor;
}

XY_HashEntry *
XY_NextHashEntry(XY_HashSearch * search)
{
	XY_HashEntry *entry = search->cursor;
	XY_HashTable *hash = search->hash;
	if (entry->next) {
		search->cursor = entry->next;
		return search->cursor;
	}
	while (search->nr_hash < hash->nr_hashes) {
		XY_HashEntry **first = &hash->table[search->nr_hash];
		search->nr_hash++;
		if (*first) {
			search->cursor = *first;
			return *first;
		}
	}
	return NULL;
}

XY_HashEntry *
XY_FirstHashEntry(XY_HashTable * hash, XY_HashSearch * search)
{
	search->hash = hash;
	for (search->nr_hash = 0; search->nr_hash < hash->nr_hashes;) {
		XY_HashEntry **first = &hash->table[search->nr_hash];
		search->nr_hash++;
		if (*first) {
			search->cursor = *first;
			return *first;
		}
	}
	return NULL;
}

void
XY_DeleteHashEntry(XY_HashTable * hash, XY_HashEntry * entry)
{
	XY_HashEntry *prev = entry->prev;
	XY_HashEntry *next = entry->next;
	if (prev) {
		prev->next = next;
	} else {
		int h = hash->hashfunc(entry->key);
		XY_HashEntry **first = &hash->table[h];
		*first = next;
	}
	if (next)
		next->prev = prev;
	free(entry);
}

void
XY_ClearHashTable(XY_HashTable * hash)
{
	XY_HashEntry *cursor;
	int i;
	for (i = 0; i < hash->nr_hashes; i++) {
		XY_HashEntry **first = &hash->table[i];
		XY_HashEntry *next;
		for (cursor = *first; cursor; cursor = next) {
			next = cursor->next;
			free(cursor);
		}
		*first = NULL;
	}
	free(hash->table);
}

XY_HashEntry *
XY_AddHashEntry(XY_HashTable * hash, XY_HashEntry * newentry, const void *key)
{
	int h = hash->hashfunc(key);
	XY_HashEntry **first = &hash->table[h];
	XY_HashEntry *cursor;

	for (cursor = *first; cursor; cursor = cursor->next) {
		if (hash->isequal(cursor->key, key)) {
			break;
		}
	}
	if (cursor) {
		return NULL;
	}
	newentry->next = *first;
	newentry->prev = NULL;
	if (*first)
		(*first)->prev = newentry;
	*first = newentry;
	newentry->key = key;
	return newentry;
}

void
XY_RemoveHashEntry(XY_HashTable * hash, XY_HashEntry * entry)
{
	XY_HashEntry *prev = entry->prev;
	XY_HashEntry *next = entry->next;
	if (prev) {
		prev->next = next;
	} else {
		int h = hash->hashfunc(entry->key);
		XY_HashEntry **first = &hash->table[h];
		*first = next;
	}
	if (next)
		next->prev = prev;
}

XY_HashEntry *
XY_CreateHashEntry(XY_HashTable * hash, void *key, int *newptr)
{
	int h = hash->hashfunc(key);
	XY_HashEntry **first = &hash->table[h];
	XY_HashEntry *cursor;
	XY_HashEntry *newentry;
#if 0
	printf("h ist %d\n", h);
	printf("first ist %p\n", first);
	printf("*first ist %p\n", *first);
#endif
	for (cursor = *first; cursor; cursor = cursor->next) {
		if (hash->isequal(cursor->key, key)) {
			break;
		}
	}
	if (cursor) {
//              printf("already da %s\n",cursor->key);
		*newptr = 0;
		return cursor;
	}
	*newptr = 1;
	newentry = (XY_HashEntry *) malloc(sizeof(XY_HashEntry));
	if (!newentry) {
		exit(342);
	}
	newentry->next = *first;
	newentry->prev = NULL;
	if (*first)
		(*first)->prev = newentry;
	*first = newentry;
	newentry->key = key;
	//printf("key ist %s\n",key);
	return newentry;
}

int
XY_InitHashTable(XY_HashTable * hash, int keytype, int size)
{
	int n_bytes = size * sizeof(void *);
	hash->table = (XY_HashEntry **) sg_calloc(n_bytes);
#if 0
	printf("Table at %p, len %d\n", hash->table, n_bytes);
#endif
	switch (keytype) {
	    case XY_USER_HASHFUNC:
		    break;
	    case XY_ONE_WORD_KEYS:
		    hash->isequal = one_word_isequal;
		    switch (size) {
			case 32:
				hash->hashfunc = hashkey_one_word32;
				break;
			case 256:
				hash->hashfunc = hashkey_one_word256;
				break;
			case 1024:
				hash->hashfunc = hashkey_one_word1024;
				break;
			case 8192:
				hash->hashfunc = hashkey_one_word8192;
				break;
			default:
				sg_free(hash->table);
				return -1;
		    }
		    break;
	    case XY_STRING_KEYS:
		    hash->isequal = string_isequal;
		    switch (size) {
			case 32:
				hash->hashfunc = hashkey_string32;
				break;
			case 256:
				hash->hashfunc = hashkey_string256;
				break;
			case 1024:
				hash->hashfunc = hashkey_string1024;
				break;
			case 8192:
				hash->hashfunc = hashkey_string8192;
				break;
			case 65536:
				hash->hashfunc = hashkey_string65536;
				break;
			default:
				sg_free(hash->table);
				return -1;

		    }
		    break;
	    default:
		    sg_free(hash->table);
		    return -1;
		    break;
	}
	hash->nr_hashes = size;
	return 0;
}

#ifdef XY_HASH_STAT
void
XY_HashStat(XY_HashTable * hash)
{
	int *stat = NULL;
	int max = 0;
	int count;
	int sum = 0;
	int sumquadrat = 0;
	int i, j;
	for (i = 0; i < hash->nr_hashes; i++) {
		XY_HashEntry **first = &hash->table[i];
		XY_HashEntry *cursor;
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
	       (float)sum / hash->nr_hashes);
}
#endif
