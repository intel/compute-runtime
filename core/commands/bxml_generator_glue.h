/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <cstring>

// Macro helpers
#ifndef STATIC_ASSERT
#define STATIC_ASSERT(e) static_assert(e, #e)
#endif

#ifndef SIZE32
#define SIZE32(x) (sizeof(x) / sizeof(uint32_t))
#endif // SIZE32

/*****************************************************************************\
MACRO: BITFIELD_RANGE
PURPOSE: Calculates the number of bits between the startbit and the endbit (0 based)
\*****************************************************************************/
#ifndef BITFIELD_RANGE
#define BITFIELD_RANGE(startbit, endbit) ((endbit) - (startbit) + 1)
#endif

/*****************************************************************************\
MACRO: BITFIELD_BIT
PURPOSE: Definition declared for clarity when creating structs
\*****************************************************************************/
#ifndef BITFIELD_BIT
#define BITFIELD_BIT(bit) 1
#endif
