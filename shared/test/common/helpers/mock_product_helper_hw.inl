/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

namespace NEO {

template <>
std::vector<int32_t> MockProductHelperHw<gfxProduct>::getKernelSupportedThreadArbitrationPolicies() const {
    return threadArbPolicies;
}
template <>
bool MockProductHelperHw<gfxProduct>::isCooperativeEngineSupported(const HardwareInfo &hwInfo) const {
    return isCooperativeEngineSupportedValue;
}

template <>
bool MockProductHelperHw<gfxProduct>::getUuid(NEO::DriverModel *driverModel, const uint32_t subDeviceCount, const uint32_t deviceIndex, std::array<uint8_t, ProductHelper::uuidSize> &uuid) const {
    return false;
}

template <>
uint32_t MockProductHelperHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    return returnedStepping;
}

template <>
int MockProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    FeatureTable *featureTable = &hwInfo->featureTable;
    featureTable->flags.ftrGpGpuThreadGroupLevelPreempt = 0;
    featureTable->flags.ftrGpGpuMidBatchPreempt = 0;

    if (use128MbEdram) {
        GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;
        gtSystemInfo->EdramSizeInKb = 128 * 1000;
    }
    if (enableMidThreadPreemption) {
        featureTable->flags.ftrWalkerMTP = 1;
    }
    if (enableThreadGroupPreemption) {
        featureTable->flags.ftrGpGpuThreadGroupLevelPreempt = 1;
    }
    if (enableMidBatchPreemption) {
        featureTable->flags.ftrGpGpuMidBatchPreempt = 1;
    }
    return (failOnConfigureHardwareCustom) ? -1 : 0;
}

template <>
uint64_t MockProductHelperHw<gfxProduct>::getDeviceMemoryPhysicalSizeInBytes(const OSInterface *osIface, uint32_t subDeviceIndex) const {
    return 1024u;
}

template <>
uint32_t MockProductHelperHw<gfxProduct>::getDeviceMemoryMaxClkRate(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) const {
    return 800u;
}

template <>
uint32_t MockProductHelperHw<gfxProduct>::getL1CachePolicy(bool isDebuggerActive) const {
    if (isDebuggerActive) {
        return this->returnedL1CachePolicyIfDebugger;
    }
    return this->returnedL1CachePolicy;
}

template <>
bool MockProductHelperHw<gfxProduct>::isUnlockingLockedPtrNecessary(const HardwareInfo &hwInfo) const {
    return this->returnedIsUnlockingLockedPtrNecessary;
}

template <>
bool MockProductHelperHw<gfxProduct>::isBufferPoolAllocatorSupported() const {
    return this->isBufferPoolAllocatorSupportedValue;
}

template <>
std::vector<uint32_t> MockProductHelperHw<gfxProduct>::getSupportedNumGrfs(const ReleaseHelper *releaseHelper) const {
    if (releaseHelper) {
        return releaseHelper->getSupportedNumGrfs();
    }
    return {128u};
}

template <>
aub_stream::EngineType MockProductHelperHw<gfxProduct>::getDefaultCopyEngine() const {
    return this->mockDefaultCopyEngine;
}
} // namespace NEO
