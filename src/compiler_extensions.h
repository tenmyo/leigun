/*
 * ---------------------------------------------------
 * Use compiler builtins for better compilation
 * results. Taken from the linux kernel source
 * ---------------------------------------------------
 */
#pragma once
#ifndef COMPILER_EXTENSIONS_H
#define COMPILER_EXTENSIONS_H

#if __GNUC__ > 2
#  define likely(x)       __builtin_expect(!!(x), 1)
#  define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#  define likely(x)       x
#  define unlikely(x)     x
#endif

#ifdef __GNUC__
#  define __UNUSED__ __attribute__((unused))
#  define __NORETURN__ __attribute__((noreturn))
#  define __CONSTRUCTOR__ __attribute((constructor))
#else
#  define __UNUSED__
#  define __NORETURN__
#  define __attribute__(...)
#endif
#define clz32	__builtin_clz
#define clz64	__builtin_clzll

#if defined(_MSC_VER)
#  define typeof(x) void
#  include <BaseTsd.h>
   typedef SSIZE_T ssize_t;
#  define alloca(x) _alloca(x)
#endif

#ifdef __unix__
#  define _FILE_OFFSET_BITS 64
#  include <unistd.h>
#else
   typedef long off_t;
#  define fseeko fseek
#endif

#if defined(_MSC_VER)
// softgun
#define NO_DEBUGGER
#define NO_SHLIB
#define NO_LOAD_BIN

#define NO_ALSA

// rfbserver
#define NO_ZLIB
#define NO_STARTCMD
//#define NO_KEYBOARD
//#define NO_MOUSE

// uzebox
#define NO_MMC
#define NO_USART
#define NO_EEPROM

// fio
#define NO_ARM9
#define NO_SIGNAL
#endif
#endif
