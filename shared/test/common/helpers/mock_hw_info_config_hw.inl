/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
std::vector<int32_t> MockHwInfoConfigHw<gfxProduct>::getKernelSupportedThreadArbitrationPolicies() {
    return threadArbPolicies;
}
template <>
bool MockHwInfoConfigHw<gfxProduct>::isCooperativeEngineSupported(const HardwareInfo &hwInfo) const {
    return isCooperativeEngineSupportedValue;
}

template <>
bool MockHwInfoConfigHw<gfxProduct>::getUuid(Device *device, std::array<uint8_t, HwInfoConfig::uuidSize> &uuid) const {
    return false;
}

template <>
uint32_t MockHwInfoConfigHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    return returnedStepping;
}

template <>
int MockHwInfoConfigHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
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
uint64_t MockHwInfoConfigHw<gfxProduct>::getDeviceMemoryPhysicalSizeInBytes(const OSInterface *osIface, uint32_t subDeviceIndex) {
    return 1024u;
}

template <>
uint32_t MockHwInfoConfigHw<gfxProduct>::getDeviceMemoryMaxClkRate(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) {
    return 800u;
}
