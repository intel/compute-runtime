/*
 * Copyright (C) 2019-2026 Intel Corporation
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

    virtual ~MemoryInfo() {};

    MemoryInfo(const RegionContainer &regionInfo, const Drm &drm);

    void assignRegionsFromDistances(const std::vector<DistanceInfo> &distances);

    MOCKABLE_VIRTUAL int createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, uint64_t patIndex, std::optional<uint32_t> vmId, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, bool isUSMHostAllocation, GemCreateExtHint hint, std::optional<bool> deferBacking);
    int createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, uint64_t patIndex, std::optional<uint32_t> vmId, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, bool isUSMHostAllocation, GemCreateExtHint hint) {
        return createGemExt(memClassInstances, allocSize, handle, patIndex, vmId, pairHandle, isChunked, numOfChunks, isUSMHostAllocation, hint, std::nullopt);
    }
    int createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, uint64_t patIndex, std::optional<uint32_t> vmId, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, bool isUSMHostAllocation) {
        return createGemExt(memClassInstances, allocSize, handle, patIndex, vmId, pairHandle, isChunked, numOfChunks, isUSMHostAllocation, GemCreateExtHint::none, std::nullopt);
    }

    MemoryClassInstance getMemoryRegionClassAndInstance(DeviceBitfield deviceBitfield, const HardwareInfo &hwInfo);

    MOCKABLE_VIRTUAL size_t getMemoryRegionSize(uint32_t memoryBank) const;

    const MemoryRegion &getMemoryRegion(DeviceBitfield deviceBitfield) const;

    void printRegionSizes() const;

    uint32_t getLocalMemoryRegionIndex(DeviceBitfield deviceBitfield) const;

    MOCKABLE_VIRTUAL int createGemExtWithSingleRegion(DeviceBitfield memoryBanks, size_t allocSize, uint32_t &handle, uint64_t patIndex, int32_t pairHandle, bool isUSMHostAllocation, GemCreateExtHint hint, std::optional<bool> deferBacking);
    int createGemExtWithSingleRegion(DeviceBitfield memoryBanks, size_t allocSize, uint32_t &handle, uint64_t patIndex, int32_t pairHandle, bool isUSMHostAllocation, GemCreateExtHint hint) {
        return createGemExtWithSingleRegion(memoryBanks, allocSize, handle, patIndex, pairHandle, isUSMHostAllocation, hint, std::nullopt);
    }
    int createGemExtWithSingleRegion(DeviceBitfield memoryBanks, size_t allocSize, uint32_t &handle, uint64_t patIndex, int32_t pairHandle, bool isUSMHostAllocation) {
        return createGemExtWithSingleRegion(memoryBanks, allocSize, handle, patIndex, pairHandle, isUSMHostAllocation, GemCreateExtHint::none, std::nullopt);
    }
    MOCKABLE_VIRTUAL int createGemExtWithMultipleRegions(DeviceBitfield memoryBanks, size_t allocSize, uint32_t &handle, uint64_t patIndex, bool isUSMHostAllocation, std::optional<bool> deferBacking);
    int createGemExtWithMultipleRegions(DeviceBitfield memoryBanks, size_t allocSize, uint32_t &handle, uint64_t patIndex, bool isUSMHostAllocation) {
        return createGemExtWithMultipleRegions(memoryBanks, allocSize, handle, patIndex, isUSMHostAllocation, std::nullopt);
    }
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
