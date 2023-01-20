/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
std::vector<int32_t> MockProductHelperHw<gfxProduct>::getKernelSupportedThreadArbitrationPolicies() const {
    return threadArbPolicies;
}
template <>
bool MockProductHelperHw<gfxProduct>::isCooperativeEngineSupported(const HardwareInfo &hwInfo) const {
    return isCooperativeEngineSupportedValue;
}

template <>
bool MockProductHelperHw<gfxProduct>::getUuid(Device *device, std::array<uint8_t, ProductHelper::uuidSize> &uuid) const {
    return false;
}

template <>
uint32_t MockProductHelperHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    return returnedStepping;
}

template <>
int MockProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    FeatureTable *featureTable = &hwInfo->featureTable;
    featureTable->flags.ftrGpGpuMidThreadLevelPreempt = 0;
    featureTable->flags.ftrGpGpuThreadGroupLevelPreempt = 0;
    featureTable->flags.ftrGpGpuMidBatchPreempt = 0;

    if (use128MbEdram) {
        GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;
        gtSystemInfo->EdramSizeInKb = 128 * 1000;
    }
    if (enableMidThreadPreemption) {
        featureTable->flags.ftrGpGpuMidThreadLevelPreempt = 1;
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