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
struct MockHwInfoConfigHw : NEO::HwInfoConfigHw<productFamily> {
    using HwInfoConfig::getDefaultLocalMemoryAccessMode;
    std::vector<int32_t> getKernelSupportedThreadArbitrationPolicies() override;
    bool isCooperativeEngineSupported(const HardwareInfo &hwInfo) const override;
    bool getUuid(Device *device, std::array<uint8_t, HwInfoConfig::uuidSize> &uuid) const override;
    uint32_t getSteppingFromHwRevId(const HardwareInfo &hwInfo) const override;
    int configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) override;
    uint64_t getDeviceMemoryPhysicalSizeInBytes(const OSInterface *osIface, uint32_t subDeviceIndex) override;
    uint32_t getDeviceMemoryMaxClkRate(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) override;

    bool use128MbEdram = false;
    bool enableMidThreadPreemption = false;
    bool enableThreadGroupPreemption = false;
    bool enableMidBatchPreemption = false;
    bool failOnConfigureHardwareCustom = false;
    bool isCooperativeEngineSupportedValue = true;
    uint32_t returnedStepping = 0;
    std::vector<int32_t> threadArbPolicies = {};
};
} // namespace NEO