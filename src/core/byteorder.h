//===-- core/byteorder.h - Byteorder Conversion Macros --------------*- C
//-*-===//
//
//              The Leigun Embedded System Simulator Platform
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of byteorder conversion macros.
///
//===----------------------------------------------------------------------===//
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
//==============================================================================
//= Dependencies
//==============================================================================
// Local/Private Headers

// External headers

// System headers
#include <stdint.h>


//==============================================================================
//= Constants(also Enumerations)
//==============================================================================


//==============================================================================
//= Types
//==============================================================================


//==============================================================================
//= Variables
//==============================================================================


//==============================================================================
//= Macros
//==============================================================================
#if defined(__linux__) || defined(__CYGWIN__)
// see man bswap(3)
// see man endian(3)
// http://man7.org/linux/man-pages/man3/bswap.3.html
// http://man7.org/linux/man-pages/man3/endian.3.html
#include <byteswap.h>
#include <endian.h>
#define BYTE_Swap16(x) bswap_16(x)
#define BYTE_Swap32(x) bswap_32(x)
#define BYTE_Swap64(x) bswap_64(x)
#define BYTE_HToBe16(x) htobe16(x)
#define BYTE_HToBe32(x) htobe32(x)
#define BYTE_HToBe64(x) htobe64(x)
#define BYTE_HToLe16(x) htole16(x)
#define BYTE_HToLe32(x) htole32(x)
#define BYTE_HToLe64(x) htole64(x)
#define BYTE_BeToH16(x) be16toh(x)
#define BYTE_BeToH32(x) be32toh(x)
#define BYTE_BeToH64(x) be64toh(x)
#define BYTE_LeToH16(x) le16toh(x)
#define BYTE_LeToH32(x) le32toh(x)
#define BYTE_LeToH64(x) le64toh(x)
#define BYTE_ORDER_LITTLE LITTLE_ENDIAN
#define BYTE_ORDER_BIG BIG_ENDIAN
#define BYTE_ORDER_NATIVE BYTE_ORDER
#elif defined(__NetBSD__) || defined(__FreeBSD__) || defined(__DragonFly__)
// see man byteorder(9)
// http://netbsd.gw.com/cgi-bin/man-cgi?byteorder+9.NONE+NetBSD-current
// https://www.freebsd.org/cgi/man.cgi?query=byteorder&sektion=9&manpath=FreeBSD+11.0-RELEASE
// https://leaf.dragonflybsd.org/cgi/web-man?command=byteorder&section=9
#include <sys/endian.h>
#define BYTE_Swap16(x) bswap16(x)
#define BYTE_Swap32(x) bswap32(x)
#define BYTE_Swap64(x) bswap64(x)
#define BYTE_HToBe16(x) htobe16(x)
#define BYTE_HToBe32(x) htobe32(x)
#define BYTE_HToBe64(x) htobe64(x)
#define BYTE_HToLe16(x) htole16(x)
#define BYTE_HToLe32(x) htole32(x)
#define BYTE_HToLe64(x) htole64(x)
#define BYTE_BeToH16(x) be16toh(x)
#define BYTE_BeToH32(x) be32toh(x)
#define BYTE_BeToH64(x) be64toh(x)
#define BYTE_LeToH16(x) le16toh(x)
#define BYTE_LeToH32(x) le32toh(x)
#define BYTE_LeToH64(x) le64toh(x)
#define BYTE_ORDER_LITTLE LITTLE_ENDIAN
#define BYTE_ORDER_BIG BIG_ENDIAN
#define BYTE_ORDER_NATIVE BYTE_ORDER
#elif defined(__OpenBSD__) || defined(__Bitrig__)
// see man byteorder(3)
// http://man.openbsd.org/OpenBSD-current/man3/byteorder.3
#include <endian.h>
#define BYTE_Swap16(x) swap16(x)
#define BYTE_Swap32(x) swap32(x)
#define BYTE_Swap64(x) swap64(x)
#define BYTE_HToBe16(x) htobe16(x)
#define BYTE_HToBe32(x) htobe32(x)
#define BYTE_HToBe64(x) htobe64(x)
#define BYTE_HToLe16(x) htole16(x)
#define BYTE_HToLe32(x) htole32(x)
#define BYTE_HToLe64(x) htole64(x)
#define BYTE_BeToH16(x) be16toh(x)
#define BYTE_BeToH32(x) be32toh(x)
#define BYTE_BeToH64(x) be64toh(x)
#define BYTE_LeToH16(x) le16toh(x)
#define BYTE_LeToH32(x) le32toh(x)
#define BYTE_LeToH64(x) le64toh(x)
#define BYTE_ORDER_LITTLE LITTLE_ENDIAN
#define BYTE_ORDER_BIG BIG_ENDIAN
#define BYTE_ORDER_NATIVE BYTE_ORDER
#elif defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define BYTE_Swap16(x) OSSwapInt16(x)
#define BYTE_Swap32(x) OSSwapInt32(x)
#define BYTE_Swap64(x) OSSwapInt64(x)
#define BYTE_HToBe16(x) OSSwapHostToBigInt16(x)
#define BYTE_HToBe32(x) OSSwapHostToBigInt32(x)
#define BYTE_HToBe64(x) OSSwapHostToBigInt64(x)
#define BYTE_HToLe16(x) OSSwapHostToLittleInt16(x)
#define BYTE_HToLe32(x) OSSwapHostToLittleInt32(x)
#define BYTE_HToLe64(x) OSSwapHostToLittleInt64(x)
#define BYTE_BeToH16(x) OSSwapBigToHostInt16(x)
#define BYTE_BeToH32(x) OSSwapBigToHostInt32(x)
#define BYTE_BeToH64(x) OSSwapBigToHostInt64(x)
#define BYTE_LeToH16(x) OSSwapLittleToHostInt16(x)
#define BYTE_LeToH32(x) OSSwapLittleToHostInt32(x)
#define BYTE_LeToH64(x) OSSwapLittleToHostInt64(x)
#define BYTE_ReadFromBe16(base, byteOffset) OSReadBigInt16(base, byteOffset)
#define BYTE_ReadFromBe32(base, byteOffset) OSReadBigInt32(base, byteOffset)
#define BYTE_ReadFromBe64(base, byteOffset) OSReadBigInt64(base, byteOffset)
#define BYTE_ReadFromLe16(base, byteOffset) OSReadLittleInt16(base, byteOffset)
#define BYTE_ReadFromLe32(base, byteOffset) OSReadLittleInt32(base, byteOffset)
#define BYTE_ReadFromLe64(base, byteOffset) OSReadLittleInt64(base, byteOffset)
#define BYTE_WriteToBe16(base, byteOffset, data)                               \
    OSWriteBigInt16(base, byteOffset, data)
#define BYTE_WriteToBe32(base, byteOffset, data)                               \
    OSWriteBigInt32(base, byteOffset, data)
#define BYTE_WriteToBe64(base, byteOffset, data)                               \
    OSWriteBigInt64(base, byteOffset, dat)
#define BYTE_WriteToLe16(base, byteOffset, data)                               \
    OSWriteLittleInt16(base, byteOffset, data)
#define BYTE_WriteToLe32(base, byteOffset, data)                               \
    OSWriteLittleInt32(base, byteOffset, data)
#define BYTE_WriteToLe64(base, byteOffset, data)                               \
    OSWriteLittleInt64(base, byteOffset, data)
#define BYTE_ORDER_LITTLE OSLittleEndian
#define BYTE_ORDER_BIG OSBigEndian
#define BYTE_ORDER_NATIVE (OSHostByteOrder())
#elif defined(_MSC_VER)
// https://msdn.microsoft.com/en-us/library/windows/desktop/ms724884%28v=vs.85%29.aspx
#include <Windows.h>
#include <stdlib.h>
#define BYTE_Swap16(x) _byteswap_ushort(x)
#define BYTE_Swap32(x) _byteswap_ulong(x)
#define BYTE_Swap64(x) _byteswap_uint64(x)
#define BYTE_ORDER_LITTLE REG_DWORD_LITTLE_ENDIAN
#define BYTE_ORDER_BIG REG_DWORD_BIG_ENDIAN
#define BYTE_ORDER_NATIVE REG_DWORD
#else
#error platform not supported
#endif


#ifndef BYTE_HToBe16
#if BYTE_ORDER_NATIVE == BYTE_ORDER_LITTLE
#define BYTE_HToBe16(x) BYTE_swap16(x)
#define BYTE_HToBe32(x) BYTE_swap32(x)
#define BYTE_HToBe64(x) BYTE_swap64(x)
#define BYTE_HToLe16(x) (x)
#define BYTE_HToLe32(x) (x)
#define BYTE_HToLe64(x) (x)
#define BYTE_BeToH16(x) BYTE_swap16(x)
#define BYTE_BeToH32(x) BYTE_swap32(x)
#define BYTE_BeToH64(x) BYTE_swap64(x)
#define BYTE_LeToH16(x) (x)
#define BYTE_LeToH32(x) (x)
#define BYTE_LeToH64(x) (x)
#else // if BYTE_ORDER_NATIVE == BYTE_ORDER_LITTLE
#define BYTE_HToBe16(x) (x)
#define BYTE_HToBe32(x) (x)
#define BYTE_HToBe64(x) (x)
#define BYTE_HToLe16(x) BYTE_swap16(x)
#define BYTE_HToLe32(x) BYTE_swap32(x)
#define BYTE_HToLe64(x) BYTE_swap64(x)
#define BYTE_BeToH16(x) (x)
#define BYTE_BeToH32(x) (x)
#define BYTE_BeToH64(x) (x)
#define BYTE_LeToH16(x) BYTE_swap16(x)
#define BYTE_LeToH32(x) BYTE_swap32(x)
#define BYTE_LeToH64(x) BYTE_swap64(x)
#endif // if BYTE_ORDER_NATIVE == BYTE_ORDER_LITTLE
#endif // ifndef BYTE_HToBe16


static inline const uint16_t BYTE_Read16(const void *base,
                                         uintptr_t byteOffset) {
    return *(uint16_t *)((uintptr_t)base + byteOffset);
}
static inline const uint32_t BYTE_Read32(const void *base,
                                         uintptr_t byteOffset) {
    return *(uint32_t *)((uintptr_t)base + byteOffset);
}
static inline const uint64_t BYTE_Read64(const void *base,
                                         uintptr_t byteOffset) {
    return *(uint64_t *)((uintptr_t)base + byteOffset);
}

static inline void BYTE_Write16(void *base, uintptr_t byteOffset,
                                uint16_t data) {
    *(uint16_t *)((uintptr_t)base + byteOffset) = data;
}
static inline void BYTE_Write32(void *base, uintptr_t byteOffset,
                                uint32_t data) {
    *(uint32_t *)((uintptr_t)base + byteOffset) = data;
}
static inline void BYTE_Write64(void *base, uintptr_t byteOffset,
                                uint64_t data) {
    *(uint64_t *)((uintptr_t)base + byteOffset) = data;
}

#ifndef BYTE_ReadFromBe16
#define BYTE_ReadFromBe16(base, byteOffset)                                    \
    BYTE_BeToH16(BYTE_Read16(base, byteOffset))
#define BYTE_ReadFromBe32(base, byteOffset)                                    \
    BYTE_BeToH32(BYTE_Read32(base, byteOffset))
#define BYTE_ReadFromBe64(base, byteOffset)                                    \
    BYTE_BeToH64(BYTE_Read64(base, byteOffset))
#define BYTE_ReadFromLe16(base, byteOffset)                                    \
    BYTE_LeToH16(BYTE_Read16(base, byteOffset))
#define BYTE_ReadFromLe32(base, byteOffset)                                    \
    BYTE_LeToH32(BYTE_Read32(base, byteOffset))
#define BYTE_ReadFromLe64(base, byteOffset)                                    \
    BYTE_LeToH64(BYTE_Read64(base, byteOffset))
#define BYTE_WriteToBe16(base, byteOffset, data)                               \
    BYTE_Write16(base, byteOffset, BYTE_HToBe16(data))
#define BYTE_WriteToBe32(base, byteOffset, data)                               \
    BYTE_Write32(base, byteOffset, BYTE_HToBe32(data))
#define BYTE_WriteToBe64(base, byteOffset, data)                               \
    BYTE_Write64(base, byteOffset, BYTE_HToBe64(data))
#define BYTE_WriteToLe16(base, byteOffset, data)                               \
    BYTE_Write16(base, byteOffset, BYTE_HToLe16(data))
#define BYTE_WriteToLe32(base, byteOffset, data)                               \
    BYTE_Write32(base, byteOffset, BYTE_HToLe32(data))
#define BYTE_WriteToLe64(base, byteOffset, data)                               \
    BYTE_Write64(base, byteOffset, BYTE_HToLe64(data))
#endif // ifndef BYTE_ReadFromBe16


//==============================================================================
//= Functions
//==============================================================================


#ifdef __cplusplus
}
#endif
