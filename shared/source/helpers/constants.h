/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/definitions/engine_group_types.h"

#include <cstddef>
#include <cstdint>
#include <limits>

constexpr bool is32bit = (sizeof(void *) == 4);
constexpr bool is64bit = (sizeof(void *) == 8);

constexpr NEO::DeviceBitfield systemMemoryBitfield(0b0);

constexpr uint64_t maxNBitValue(uint64_t n) {
    return ((n == 64) ? std::numeric_limits<uint64_t>::max()
                      : ((1ULL << n) - 1));
}
static_assert(maxNBitValue(8) == std::numeric_limits<uint8_t>::max(), "");
static_assert(maxNBitValue(16) == std::numeric_limits<uint16_t>::max(), "");
static_assert(maxNBitValue(32) == std::numeric_limits<uint32_t>::max(), "");
static_assert(maxNBitValue(64) == std::numeric_limits<uint64_t>::max(), "");

namespace MemoryConstants {
constexpr uint64_t zoneHigh = ~(uint64_t)0xFFFFFFFF;
constexpr uint64_t kiloByte = 1024;
constexpr uint64_t kiloByteShiftSize = 10;
constexpr uint64_t megaByte = 1024 * kiloByte;
constexpr uint64_t gigaByte = 1024 * megaByte;
constexpr size_t minBufferAlignment = 4;
constexpr size_t cacheLineSize = 64;
constexpr size_t pageSize = 4 * kiloByte;
constexpr size_t pageSize64k = 64 * kiloByte;
constexpr size_t pageSize2Mb = 2 * megaByte;
constexpr size_t preferredAlignment = pageSize;  // alignment preferred for performance reasons, i.e. internal allocations
constexpr size_t allocationAlignment = pageSize; // alignment required to gratify incoming pointer, i.e. passed host_ptr
constexpr size_t slmWindowAlignment = 128 * kiloByte;
constexpr size_t slmWindowSize = 64 * kiloByte;
constexpr uintptr_t pageMask = (pageSize - 1);
constexpr uintptr_t page64kMask = (pageSize64k - 1);
constexpr uint64_t max32BitAppAddress = maxNBitValue(31);
constexpr uint64_t max64BitAppAddress = maxNBitValue(47);
constexpr uint32_t sizeOf4GBinPageEntities = (MemoryConstants::gigaByte * 4 - MemoryConstants::pageSize) / MemoryConstants::pageSize;
constexpr uint64_t max32BitAddress = maxNBitValue(32);
constexpr uint64_t max36BitAddress = (maxNBitValue(36));
constexpr uint64_t max48BitAddress = maxNBitValue(48);
constexpr uintptr_t page4kEntryMask = std::numeric_limits<uintptr_t>::max() & ~MemoryConstants::pageMask;
constexpr uintptr_t page64kEntryMask = std::numeric_limits<uintptr_t>::max() & ~MemoryConstants::page64kMask;
constexpr int GfxAddressBits = is64bit ? 48 : 32;
constexpr uint64_t maxSvmAddress = is64bit ? maxNBitValue(47) : maxNBitValue(32);

} // namespace MemoryConstants

constexpr uint64_t KB = MemoryConstants::kiloByte;
constexpr uint64_t MB = MemoryConstants::megaByte;
constexpr uint64_t GB = MemoryConstants::gigaByte;

namespace BlitterConstants {
constexpr uint64_t maxBlitWidth = 0x4000;
constexpr uint64_t maxBlitHeight = 0x4000;
constexpr uint64_t maxBlitSetWidth = 0x1FF80;  // 0x20000 aligned to 128
constexpr uint64_t maxBlitSetHeight = 0x1FFC0; // 0x20000 aligned to cacheline size

constexpr uint64_t maxBytesPerPixel = 0x10;
enum class BlitDirection : uint32_t {
    BufferToHostPtr,
    HostPtrToBuffer,
    BufferToBuffer,
    HostPtrToImage,
    ImageToHostPtr,
    ImageToImage
};

enum PostBlitMode : int32_t {
    Default = -1,
    MiArbCheck = 0,
    MiFlush = 1,
    None = 2
};
} // namespace BlitterConstants

namespace CommonConstants {
constexpr uint64_t unsupportedPatIndex = std::numeric_limits<uint64_t>::max();
constexpr uint32_t unspecifiedDeviceIndex = std::numeric_limits<uint32_t>::max();
constexpr uint32_t invalidStepping = std::numeric_limits<uint32_t>::max();
constexpr uint32_t invalidRevisionID = std::numeric_limits<uint16_t>::max();
constexpr uint32_t maximalSimdSize = 32;
constexpr uint32_t maximalSizeOfAtomicType = 8;
constexpr uint32_t engineGroupCount = static_cast<uint32_t>(NEO::EngineGroupType::MaxEngineGroups);
} // namespace CommonConstants
