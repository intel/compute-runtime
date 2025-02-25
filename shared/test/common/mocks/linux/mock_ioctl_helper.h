/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include <optional>

namespace NEO {

constexpr uint64_t probedSizeRegionZero = 8 * MemoryConstants::gigaByte;
constexpr uint64_t probedSizeRegionOne = 16 * MemoryConstants::gigaByte;
constexpr uint64_t probedSizeRegionFour = 32 * MemoryConstants::gigaByte;
constexpr uint64_t unallocatedSizeRegionZero = 6 * MemoryConstants::gigaByte;
constexpr uint64_t unallocatedSizeRegionOne = 12 * MemoryConstants::gigaByte;
constexpr uint64_t unallocatedSizeRegionFour = 4 * MemoryConstants::gigaByte;

class MockIoctlHelper : public IoctlHelperPrelim20 {
    static constexpr int memoryClassSystem = 0x123;
    static constexpr int memoryClassDevice = 0x124;

  public:
    using IoctlHelperPrelim20::IoctlHelperPrelim20;
    ADDMETHOD_CONST_NOBASE(isImmediateVmBindRequired, bool, false, ());
    ADDMETHOD_CONST_NOBASE(getIoctlRequestValue, unsigned int, 1234u, (DrmIoctl));
    ADDMETHOD_CONST_NOBASE(isWaitBeforeBindRequired, bool, false, (bool));
    ADDMETHOD_CONST_NOBASE(makeResidentBeforeLockNeeded, bool, false, ());

    ADDMETHOD_NOBASE(vmBind, int, 0, (const VmBindParams &));
    ADDMETHOD_NOBASE(vmUnbind, int, 0, (const VmBindParams &));
    ADDMETHOD_NOBASE(allocateInterrupt, bool, true, (uint32_t &));
    ADDMETHOD_NOBASE(createMediaContext, bool, true, (uint32_t, void *, uint32_t, void *, uint32_t, void *&));
    ADDMETHOD_NOBASE(releaseMediaContext, bool, true, (void *));
    ADDMETHOD_CONST_NOBASE(getNumMediaDecoders, uint32_t, 0, ());
    ADDMETHOD_CONST_NOBASE(getNumMediaEncoders, uint32_t, 0, ());
    ADDMETHOD_NOBASE(queryDeviceParams, bool, true, (uint32_t *, uint16_t *));

    ADDMETHOD_NOBASE(closAlloc, CacheRegion, CacheRegion::none, (CacheLevel));
    ADDMETHOD_NOBASE(closFree, CacheRegion, CacheRegion::none, (CacheRegion));
    ADDMETHOD_NOBASE(closAllocWays, uint16_t, 0U, (CacheRegion, uint16_t, uint16_t));

    int getDrmParamValue(DrmParam drmParam) const override {
        if (drmParam == DrmParam::memoryClassSystem) {
            return memoryClassSystem;
        }
        if (drmParam == DrmParam::memoryClassDevice) {
            return memoryClassDevice;
        }
        return getDrmParamValueResult;
    }

    std::optional<uint32_t> getVmAdviseAtomicAttribute() override {
        if (callBaseVmAdviseAtomicAttribute)
            return IoctlHelperPrelim20::getVmAdviseAtomicAttribute();
        return vmAdviseAtomicAttribute;
    }

    bool releaseInterrupt(uint32_t handle) override {
        releaseInterruptCalled++;
        latestReleaseInterruptHandle = handle;
        return releaseInterruptResult;
    }

    std::unique_ptr<MemoryInfo> createMemoryInfo() override {

        std::vector<MemoryRegion> regionInfo(3);
        regionInfo[0].region = {memoryClassSystem, 0};
        regionInfo[0].probedSize = probedSizeRegionZero;
        regionInfo[0].unallocatedSize = unallocatedSizeRegionZero;
        regionInfo[1].region = {memoryClassDevice, 0};
        regionInfo[1].probedSize = probedSizeRegionOne;
        regionInfo[1].unallocatedSize = unallocatedSizeRegionOne;
        regionInfo[2].region = {memoryClassDevice, 1};
        regionInfo[2].probedSize = probedSizeRegionFour;
        regionInfo[2].unallocatedSize = unallocatedSizeRegionFour;

        std::unique_ptr<MemoryInfo> memoryInfo = std::make_unique<MemoryInfo>(regionInfo, drm);
        return memoryInfo;
    }
    bool getTopologyDataAndMap(const HardwareInfo &hwInfo, DrmQueryTopologyData &topologyData, TopologyMap &topologyMap) override {
        topologyData = topologyDataToSet;
        topologyMap = topologyMapToSet;
        return true;
    }
    DrmQueryTopologyData topologyDataToSet{};
    TopologyMap topologyMapToSet{};
    int getDrmParamValueResult = 1234;
    uint32_t releaseInterruptCalled = 0;
    uint32_t latestReleaseInterruptHandle = InterruptId::notUsed;

    bool releaseInterruptResult = true;
    bool callBaseVmAdviseAtomicAttribute = true;
    std::optional<uint32_t> vmAdviseAtomicAttribute{};
};
} // namespace NEO
