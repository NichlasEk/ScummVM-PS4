// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PICE_FORMATS_H
#define PICE_FORMATS_H
#include "common/endian.h"
namespace Pice {
inline uint16 resource16(const void *p,bool mac) { return mac ? READ_BE_UINT16(p) : READ_LE_UINT16(p); }
inline uint32 resource32(const void *p,bool mac) { return mac ? READ_BE_UINT32(p) : READ_LE_UINT32(p); }
}
#endif
