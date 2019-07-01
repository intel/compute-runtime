/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>

constexpr bool is32bit = (sizeof(void *) == 4);
constexpr bool is64bit = (sizeof(void *) == 8);

template <uint8_t N>
constexpr uint64_t maxNBitValue = ((1ULL << N) - 1);
static_assert(maxNBitValue<8> == std::numeric_limits<uint8_t>::max(), "");
static_assert(maxNBitValue<16> == std::numeric_limits<uint16_t>::max(), "");
static_assert(maxNBitValue<32> == std::numeric_limits<uint32_t>::max(), "");

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
static const uint64_t max32BitAppAddress = maxNBitValue<31>;
static const uint64_t max64BitAppAddress = maxNBitValue<47>;
static const uint32_t sizeOf4GBinPageEntities = (MemoryConstants::gigaByte * 4 - MemoryConstants::pageSize) / MemoryConstants::pageSize;
static const uint64_t max32BitAddress = maxNBitValue<32>;
static const uint64_t max36BitAddress = ((1ULL << 36) - 1);
static const uint64_t max48BitAddress = maxNBitValue<48>;
static const uintptr_t page4kEntryMask = std::numeric_limits<uintptr_t>::max() & ~MemoryConstants::pageMask;
static const uintptr_t page64kEntryMask = std::numeric_limits<uintptr_t>::max() & ~MemoryConstants::page64kMask;
static const int GfxAddressBits = is64bit ? 48 : 32;
static const uint64_t maxSvmAddress = is64bit ? maxNBitValue<47> : maxNBitValue<32>;

} // namespace MemoryConstants

namespace BlitterConstants {
static constexpr uint64_t maxBlitWidth = 0x7FC0; // 0x7FFF aligned to cacheline size
static constexpr uint64_t maxBlitHeight = 0x7FFF;
enum class BlitDirection {
    BufferToHostPtr,
    HostPtrToBuffer,
    BufferToBuffer
};
} // namespace BlitterConstants
