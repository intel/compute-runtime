/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace NEO {
class Drm;
struct HardwareInfo;

class MemoryInfo {
  public:
    using RegionContainer = std::vector<MemoryRegion>;

    virtual ~MemoryInfo(){};

    MemoryInfo(const RegionContainer &regionInfo, const Drm &drm);

    void assignRegionsFromDistances(const std::vector<DistanceInfo> &distances);

    MOCKABLE_VIRTUAL int createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, uint64_t patIndex, std::optional<uint32_t> vmId, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, bool isUSMHostAllocation);

    MemoryClassInstance getMemoryRegionClassAndInstance(DeviceBitfield deviceBitfield, const HardwareInfo &hwInfo);

    MOCKABLE_VIRTUAL size_t getMemoryRegionSize(uint32_t memoryBank) const;

    const MemoryRegion &getMemoryRegion(DeviceBitfield deviceBitfield) const;

    void printRegionSizes() const;

    uint32_t getLocalMemoryRegionIndex(DeviceBitfield deviceBitfield) const;

    MOCKABLE_VIRTUAL int createGemExtWithSingleRegion(DeviceBitfield memoryBanks, size_t allocSize, uint32_t &handle, uint64_t patIndex, int32_t pairHandle, bool isUSMHostAllocation);
    MOCKABLE_VIRTUAL int createGemExtWithMultipleRegions(DeviceBitfield memoryBanks, size_t allocSize, uint32_t &handle, uint64_t patIndex, bool isUSMHostAllocation);
    MOCKABLE_VIRTUAL int createGemExtWithMultipleRegions(DeviceBitfield memoryBanks, size_t allocSize, uint32_t &handle, uint64_t patIndex, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, bool isUSMHostAllocation);
    void populateTileToLocalMemoryRegionIndexMap();
    uint64_t getLocalMemoryRegionSize(uint32_t tileId) const;

    const RegionContainer &getLocalMemoryRegions() const { return localMemoryRegions; }
    const RegionContainer &getDrmRegionInfos() const { return drmQueryRegions; }
    bool isMemPolicySupported() const { return memPolicySupported; }
    bool isSmallBarDetected() const { return smallBarDetected; }

  protected:
    const Drm &drm;
    const RegionContainer drmQueryRegions;

    const MemoryRegion &systemMemoryRegion;
    bool memPolicySupported;
    int memPolicyMode;
    RegionContainer localMemoryRegions;
    std::array<uint32_t, 4> tileToLocalMemoryRegionIndexMap{};
    bool smallBarDetected;
};

} // namespace NEO
