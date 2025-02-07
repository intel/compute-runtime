/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/definitions/engine_group_types.h"
#include "shared/source/helpers/device_bitfield.h"

#include <cstdint>
#include <limits>

inline constexpr bool is32bit = (sizeof(void *) == 4);
inline constexpr bool is64bit = (sizeof(void *) == 8);

inline constexpr NEO::DeviceBitfield systemMemoryBitfield(0b0);

constexpr uint64_t maxNBitValue(uint64_t n) {
    return ((n == 64) ? std::numeric_limits<uint64_t>::max()
                      : ((1ULL << n) - 1));
}
static_assert(maxNBitValue(8) == std::numeric_limits<uint8_t>::max(), "");
static_assert(maxNBitValue(16) == std::numeric_limits<uint16_t>::max(), "");
static_assert(maxNBitValue(32) == std::numeric_limits<uint32_t>::max(), "");
static_assert(maxNBitValue(64) == std::numeric_limits<uint64_t>::max(), "");

namespace MemoryConstants {
inline constexpr uint64_t zoneHigh = ~(uint64_t)0xFFFFFFFF;
inline constexpr uint64_t kiloByte = 1024;
inline constexpr uint64_t kiloByteShiftSize = 10;
inline constexpr uint64_t megaByte = 1024 * kiloByte;
inline constexpr uint64_t gigaByte = 1024 * megaByte;
inline constexpr uint64_t teraByte = 1024 * gigaByte;
inline constexpr uint64_t fullStatefulRegion = 4 * gigaByte;
inline constexpr size_t minBufferAlignment = 4;
inline constexpr size_t cacheLineSize = 64;
inline constexpr size_t pageSize = 4 * kiloByte;
inline constexpr size_t pageSize64k = 64 * kiloByte;
inline constexpr size_t pageSize2M = 2 * megaByte;
inline constexpr size_t preferredAlignment = pageSize;  // alignment preferred for performance reasons, i.e. internal allocations
inline constexpr size_t allocationAlignment = pageSize; // alignment required to gratify incoming pointer, i.e. passed host_ptr
inline constexpr size_t slmWindowAlignment = 128 * kiloByte;
inline constexpr size_t slmWindowSize = 64 * kiloByte;
inline constexpr uintptr_t pageMask = (pageSize - 1);
inline constexpr uintptr_t page64kMask = (pageSize64k - 1);
inline constexpr uint64_t max32BitAppAddress = maxNBitValue(31);
inline constexpr uint64_t max64BitAppAddress = maxNBitValue(47);
inline constexpr uint32_t sizeOf4GBinPageEntities = (MemoryConstants::gigaByte * 4 - MemoryConstants::pageSize) / MemoryConstants::pageSize;
inline constexpr uint64_t max32BitAddress = maxNBitValue(32);
inline constexpr uint64_t max36BitAddress = (maxNBitValue(36));
inline constexpr uint64_t max48BitAddress = maxNBitValue(48);
inline constexpr uintptr_t page4kEntryMask = std::numeric_limits<uintptr_t>::max() & ~MemoryConstants::pageMask;
inline constexpr uintptr_t page64kEntryMask = std::numeric_limits<uintptr_t>::max() & ~MemoryConstants::page64kMask;
inline constexpr int gfxAddressBits = is64bit ? 48 : 32;
inline constexpr uint64_t maxSvmAddress = is64bit ? maxNBitValue(47) : maxNBitValue(32);
inline constexpr size_t chunkThreshold = MemoryConstants::pageSize64k;

} // namespace MemoryConstants

namespace BlitterConstants {
inline constexpr uint64_t maxBlitWidth = 0x4000;
inline constexpr uint64_t maxBlitHeight = 0x4000;
inline constexpr uint64_t maxBlitSetWidth = 0x1FF80;  // 0x20000 aligned to 128
inline constexpr uint64_t maxBlitSetHeight = 0x1FFC0; // 0x20000 aligned to cacheline size

inline constexpr uint64_t maxBytesPerPixel = 0x10;
enum class BlitDirection : uint32_t {
    bufferToHostPtr,
    hostPtrToBuffer,
    bufferToBuffer,
    fill,
    hostPtrToImage,
    imageToHostPtr,
    imageToImage,
};

enum PostBlitMode : int32_t {
    defaultMode = -1,
    miArbCheck = 0,
    miFlush = 1,
    none = 2
};
} // namespace BlitterConstants

namespace CommonConstants {
inline constexpr uint64_t unsupportedPatIndex = std::numeric_limits<uint64_t>::max();
inline constexpr uint32_t unspecifiedDeviceIndex = std::numeric_limits<uint32_t>::max();
inline constexpr uint32_t invalidStepping = std::numeric_limits<uint32_t>::max();
inline constexpr uint32_t invalidRevisionID = std::numeric_limits<uint16_t>::max();
inline constexpr uint32_t maximalSimdSize = 32;
inline constexpr uint32_t maximalSizeOfAtomicType = 8;
inline constexpr uint32_t engineGroupCount = static_cast<uint32_t>(NEO::EngineGroupType::maxEngineGroups);
inline constexpr uint32_t maxWorkgroupSize = 1024u;
inline constexpr uint32_t minimalSyncBufferSize = 12;
inline constexpr uint32_t gpuHangCheckTimeInUS = 500'000;
inline constexpr double defaultProfilingTimerResolution = 83.333;
inline constexpr uint64_t nsecPerSec = 1000000000ull;
inline constexpr uint32_t maxAllowedEnvVariableSize = 4096u;
} // namespace CommonConstants
