// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SCUMMVM_PS4_SYS_ENDIAN_H
#define SCUMMVM_PS4_SYS_ENDIAN_H

// SDL's PS4 headers expect the FreeBSD endian interface, which is absent from
// the packaged OpenOrbis sysroot.
#define _LITTLE_ENDIAN 1234
#define _BIG_ENDIAN 4321
#define _PDP_ENDIAN 3412

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
#define _BYTE_ORDER __BYTE_ORDER__
#else
#define _BYTE_ORDER _LITTLE_ENDIAN
#endif

#if _BYTE_ORDER == _LITTLE_ENDIAN
#define htobe16(x) __builtin_bswap16((unsigned short)(x))
#define htole16(x) ((unsigned short)(x))
#define be16toh(x) __builtin_bswap16((unsigned short)(x))
#define le16toh(x) ((unsigned short)(x))
#define htobe32(x) __builtin_bswap32((unsigned int)(x))
#define htole32(x) ((unsigned int)(x))
#define be32toh(x) __builtin_bswap32((unsigned int)(x))
#define le32toh(x) ((unsigned int)(x))
#define htobe64(x) __builtin_bswap64((unsigned long long)(x))
#define htole64(x) ((unsigned long long)(x))
#define be64toh(x) __builtin_bswap64((unsigned long long)(x))
#define le64toh(x) ((unsigned long long)(x))
#else
#define htobe16(x) ((unsigned short)(x))
#define htole16(x) __builtin_bswap16((unsigned short)(x))
#define be16toh(x) ((unsigned short)(x))
#define le16toh(x) __builtin_bswap16((unsigned short)(x))
#define htobe32(x) ((unsigned int)(x))
#define htole32(x) __builtin_bswap32((unsigned int)(x))
#define be32toh(x) ((unsigned int)(x))
#define le32toh(x) __builtin_bswap32((unsigned int)(x))
#define htobe64(x) ((unsigned long long)(x))
#define htole64(x) __builtin_bswap64((unsigned long long)(x))
#define be64toh(x) ((unsigned long long)(x))
#define le64toh(x) __builtin_bswap64((unsigned long long)(x))
#endif

#endif
