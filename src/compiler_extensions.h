/*
 * ---------------------------------------------------
 * Use compiler builtins for better compilation
 * results. Taken from the linux kernel source
 * ---------------------------------------------------
 */
#ifndef COMPILER_EXTENSIONS_H
#define COMPILER_EXTENSIONS_H

#if __GNUC__ > 2
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       x
#define unlikely(x)     x
#endif
#ifdef __GNUC__
#define __UNUSED__ __attribute__((unused))
#define __NORETURN__ __attribute__((noreturn))
#define __CONSTRUCTOR__ __attribute((constructor))
#else
#define UNUSED
#define __NORETURN__
#endif
#define clz32	__builtin_clz
#define clz64	__builtin_clzll

#endif
