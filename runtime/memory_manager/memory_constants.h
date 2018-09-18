/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <cstddef>

namespace MemoryConstants {
static const uint64_t zoneHigh = ~(uint64_t)0xFFFFFFFF;
static const uint64_t kiloByte = 1024;
static const uint64_t kiloByteShiftSize = 10;
static const uint64_t megaByte = 1024 * kiloByte;
static const uint64_t gigaByte = 1024 * megaByte;
static const size_t minBufferAlignment = 4;
static const size_t cacheLineSize = 64;
static const size_t pageSize = 4 * kiloByte;
static const size_t pageSize64k = 64 * kiloByte;
static const size_t preferredAlignment = pageSize;  // alignment preferred for performance reasons, i.e. internal allocations
static const size_t allocationAlignment = pageSize; // alignment required to gratify incoming pointer, i.e. passed host_ptr
static const size_t slmWindowAlignment = 128 * kiloByte;
static const size_t slmWindowSize = 64 * kiloByte;
static const uintptr_t pageMask = (pageSize - 1);
static const uintptr_t page64kMask = (pageSize64k - 1);
static const uint64_t max32BitAppAddress = ((1ULL << 31) - 1);
static const uint64_t max64BitAppAddress = ((1ULL << 47) - 1);
static const uint32_t sizeOf4GBinPageEntities = (MemoryConstants::gigaByte * 4 - MemoryConstants::pageSize) / MemoryConstants::pageSize;
static const uint64_t max32BitAddress = ((1ULL << 32) - 1);
static const uint64_t max48BitAddress = ((1ULL << 48) - 1);
} // namespace MemoryConstants

const bool is32bit = (sizeof(void *) == 4) ? true : false;
const bool is64bit = (sizeof(void *) == 8) ? true : false;
