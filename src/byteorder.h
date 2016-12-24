/*
 **********************************************************************************
 *
 * Byteorder Conversion Macros
 *
 * (C) 2004  Lightmaze Solutions AG
 *   Author: Jochen Karrer
 *
 **********************************************************************************
 */

#ifndef BYTEORDER_H
#define BYTEORDER_H

/* These defines were found with cpp -dM,
 * see echo | cpp -dM for example */
#if defined(__FreeBSD__)

#include <sys/endian.h>
#define swap64(x)		bswap64(x)
#define swap32(x)		bswap32(x)
#define swap16(x)		bswap16(x)

#elif defined(linux)

#include <byteswap.h>
#include <endian.h>
#define swap64(x)		__bswap_64(x)
#define swap32(x)		__bswap_32(x)
#define swap16(x)		__bswap_16(x)

#elif defined(__CYGWIN__)

#include <byteswap.h>
#include <endian.h>
#define swap64(x)		bswap_64(x)
#define swap32(x)		bswap_32(x)
#define swap16(x)		bswap_16(x)

#elif defined(_MSC_VER)

#define __LITTLE_ENDIAN 1234
#define __BYTE_ORDER __LITTLE_ENDIAN
#include <stdlib.h>
#define swap64(x)		_byteswap_uint64(x)
#define swap32(x)		_byteswap_ulong(x)
#define swap16(x)		_byteswap_ushort(x)

#else

#warning Unknown architecture
#include <byteswap.h>
#include <endian.h>
#define swap64(x)		bswap_64(x)
#define swap32(x)		bswap_32(x)
#define swap16(x)		bswap_16(x)

#endif

enum en_endianness {
    en_LITTLE_ENDIAN = 0,
    en_BIG_ENDIAN = 1,
};

#ifndef __BYTE_ORDER
#error __BYTE_ORDER undefined
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
#define HOST_BYTEORDER en_BIG_ENDIAN

#define host64_to_be(x)		(x)
#define host32_to_be(x)		(x)
#define host16_to_be(x)		(x)

#define host64_to_be(x)		swap64(x)
#define host32_to_le(x)		swap32(x)
#define host16_to_le(x)		swap16(x)

#define be64_to_host(x)		(x)
#define be32_to_host(x)		(x)
#define be16_to_host(x)		(x)

#define le64_to_host(x)     swap64(x)
#define le32_to_host(x)		swap32(x)
#define	le16_to_host(x)		swap16(x)

/*
 * ------------------------------------------
 * Big Endian Host with big Endian Target
 * ------------------------------------------
 */
#if TARGET_BIG_ENDIAN
#define TARGET_BYTEORDER en_BIG_ENDIAN
#define NON_TARGET_BYTEORDER en_LITTLE_ENDIAN
#define	target64_to_host(x) 	(x)
#define	target64_to_le(x)   	swap64(x)
#define	le64_to_target(x)   	swap64(x)
#define host64_to_target(x)	(x)

#define	target32_to_host(x) 	(x)
#define	target32_to_le(x)   	swap32(x)
#define	le32_to_target(x)   	swap32(x)
#define host32_to_target(x)	(x)

#define	target16_to_host(x) 	(x)
#define	target16_to_le(x)   	swap16(x)
#define	le16_to_target(x)   	swap16(x)
#define host16_to_target(x)	(x)

/*
 * ----------------------------------------------
 * Big Endian Host with little Endian Target
 * ----------------------------------------------
 */
#else
#define TARGET_BYTEORDER en_LITTLE_ENDIAN
#define NON_TARGET_BYTEORDER en_BIG_ENDIAN
#define		target64_to_host(x) swap64(x)
#define		host64_to_target(x) swap64(x)

#define 	target32_to_host(x) swap32(x)
#define 	target32_to_le(x)   (x)
#define		le32_to_target(x)   (x)
#define		host32_to_target(x) swap32(x)

#define 	target16_to_host(x) swap16(x)
#define 	target16_to_le(x)   (x)
#define		le16_to_target(x)   (x)
#define		host16_to_target(x) swap16(x)

#endif                          /* TARGET_BIG_ENDIAN */

#else
#define HOST_BYTEORDER en_LITTLE_ENDIAN
/*
 * ----------------------------------
 * Little Endian Host
 * ----------------------------------
 */

#define host64_to_le(x)     (x)
#define host32_to_le(x)		(x)
#define	host16_to_le(x)		(x)

#define host64_to_be(x)     swap64(x)
#define host32_to_be(x)		swap32(x)
#define host16_to_be(x)		swap16(x)

#define le64_to_host(x)     (x)
#define le32_to_host(x)		(x)
#define le16_to_host(x)		(x)

#define be64_to_host(x)     swap64(x)
#define be32_to_host(x)		swap32(x)
#define be16_to_host(x)		swap16(x)

/*
 * ------------------------------------------
 * Little Endian Host with big Endian Target
 * ------------------------------------------
 */
#if TARGET_BIG_ENDIAN
#define TARGET_BYTEORDER en_BIG_ENDIAN
#define NON_TARGET_BYTEORDER en_LITTLE_ENDIAN
#define	target64_to_host(x) 	swap64(x)
#define	target64_to_le(x)   	swap64(x)
#define	le64_to_target(x)   	swap64(x)
#define host64_to_target(x)	swap64(x)

#define	target32_to_host(x) 	swap32(x)
#define	target32_to_le(x)   	swap32(x)
#define	le32_to_target(x)   	swap32(x)
#define host32_to_target(x)	swap32(x)

#define	target16_to_host(x) 	swap16(x)
#define	target16_to_le(x)   	swap16(x)
#define	le16_to_target(x)   	swap16(x)
#define host16_to_target(x)	swap16(x)

/*
 * ----------------------------------------------
 * Little Endian Host with little Endian Target
 * ----------------------------------------------
 */
#else
#define TARGET_BYTEORDER en_LITTLE_ENDIAN
#define NON_TARGET_BYTEORDER en_BIG_ENDIAN
#define		target64_to_host(x) (x)
#define		host64_to_target(x) (x)

#define 	target32_to_host(x) (x)
#define 	target32_to_le(x)   (x)
#define 	host32_to_le(x)     (x)
#define		host32_to_target(x) (x)

#define 	target16_to_host(x) (x)
#define 	target16_to_le(x)   (x)
#define		host16_to_target(x) (x)
#define		le16_to_target(x)   (x)
#define		be16_to_target(x)   	swap16(x)

#define		le32_to_target(x)   (x)
#define		be32_to_target(x)   	swap32(x)
#define 	le32_to_host(x)     (x)
#endif

#endif
#endif                          /* BYTEORDER_H */
