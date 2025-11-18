/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef KERNEL_TYPES_H
#define KERNEL_TYPES_H 1
#ifdef WIDE_STATELESS
typedef ulong idx_t;
typedef ulong2 coord2_t;
typedef ulong4 coord4_t;
typedef ulong offset_t;
#else
typedef uint idx_t;
typedef uint2 coord2_t;
typedef uint4 coord4_t;
typedef uint offset_t;
#endif
#endif