#ifndef _SGSTRING_H
#define _SGSTRING_H
#include <string.h>
#include <stdlib.h>
#include <compiler_extensions.h>

void sg_oom(const char *file, int line);

#define sg_new(type)	(typeof(type)*)(sgcalloc(sizeof(type),__FILE__,__LINE__))
#define sg_strdup(string) sgstrdup((string),__FILE__,__LINE__)
#define sg_free(mem) free(mem)
#define sg_calloc(size)	sgcalloc((size),__FILE__,__LINE__);
#define sg_realloc(ptr,size)	sgrealloc((ptr),(size),__FILE__,__LINE__);

#define array_size(x) (sizeof(x) / sizeof((x)[0]))

/*
 * ---------------------------------------------------------------------
 * memcpy with using the smaller of the two length fields
 * ---------------------------------------------------------------------
 */
static inline size_t
sg_mincpy(void *dst, const void *src, size_t len1, size_t len2)
{
	size_t len = len1 < len2 ? len1 : len2;
	memcpy(dst, src, len);
	return len;
}

/*
 * ---------------------------------------------------------------------
 * Internal only
 * ---------------------------------------------------------------------
 */
static inline void *
sgcalloc(size_t size, const char *file, int line)
{
	void *mem = malloc(size);
	if (unlikely(!mem)) {
		sg_oom(file, line);
		return 0;
	} else {
		memset(mem, 0, size);
		return mem;
	}
}

static inline void *
sgrealloc(void *ptr, size_t size, const char *file, int line)
{
	void *mem = realloc(ptr, size);
	if (!mem) {
		sg_oom(file, line);
		return 0;
	} else {
		return mem;
	}
}

static inline char *
sgstrdup(const char *string, const char *file, int line)
{
	char *str = strdup(string);
	if (!str) {
		sg_oom(file, line);
		return 0;
	} else {
		return str;
	}
}

#endif
