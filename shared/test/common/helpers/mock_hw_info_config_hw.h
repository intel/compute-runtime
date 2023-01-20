/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

template <PRODUCT_FAMILY productFamily>
struct MockProductHelperHw : NEO::ProductHelperHw<productFamily> {
    using ProductHelper::getDefaultLocalMemoryAccessMode;
    std::vector<int32_t> getKernelSupportedThreadArbitrationPolicies() const override;
    bool isCooperativeEngineSupported(const HardwareInfo &hwInfo) const override;
    bool getUuid(Device *device, std::array<uint8_t, ProductHelper::uuidSize> &uuid) const override;
    uint32_t getSteppingFromHwRevId(const HardwareInfo &hwInfo) const override;
    int configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const override;
    uint64_t getDeviceMemoryPhysicalSizeInBytes(const OSInterface *osIface, uint32_t subDeviceIndex) const override;
    uint32_t getDeviceMemoryMaxClkRate(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) const override;
    uint32_t getL1CachePolicy(bool isDebuggerActive) const override;

    bool use128MbEdram = false;
    bool enableMidThreadPreemption = false;
    bool enableThreadGroupPreemption = false;
    bool enableMidBatchPreemption = false;
    bool failOnConfigureHardwareCustom = false;
    bool isCooperativeEngineSupportedValue = true;
    uint32_t returnedStepping = 0;
    uint32_t returnedL1CachePolicy = 0;
    uint32_t returnedL1CachePolicyIfDebugger = 0;
    std::vector<int32_t> threadArbPolicies = {};
};
} // namespace NEO