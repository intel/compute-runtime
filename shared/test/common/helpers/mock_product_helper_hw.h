/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper_hw.h"

namespace NEO {

template <PRODUCT_FAMILY productFamily>
struct MockProductHelperHw : NEO::ProductHelperHw<productFamily> {
    using ProductHelper::getDefaultLocalMemoryAccessMode;
    std::vector<int32_t> getKernelSupportedThreadArbitrationPolicies() const override;
    bool isCooperativeEngineSupported(const HardwareInfo &hwInfo) const override;
    bool getUuid(NEO::DriverModel *driverModel, const uint32_t subDeviceCount, const uint32_t deviceIndex, std::array<uint8_t, ProductHelper::uuidSize> &uuid) const override;
    uint32_t getSteppingFromHwRevId(const HardwareInfo &hwInfo) const override;
    int configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const override;
    uint64_t getDeviceMemoryPhysicalSizeInBytes(const OSInterface *osIface, uint32_t subDeviceIndex) const override;
    uint32_t getDeviceMemoryMaxClkRate(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) const override;
    uint32_t getL1CachePolicy(bool isDebuggerActive) const override;
    bool isUnlockingLockedPtrNecessary(const HardwareInfo &hwInfo) const override;
    std::vector<uint32_t> getSupportedNumGrfs(const ReleaseHelper *releaseHelper) const override;
    aub_stream::EngineType getDefaultCopyEngine() const override;
    bool isBufferPoolAllocatorSupported() const override;
    bool useAdditionalBlitProperties() const override { return enableAdditionalBlitProperties; }

    bool use128MbEdram = false;
    bool enableMidThreadPreemption = false;
    bool enableThreadGroupPreemption = false;
    bool enableMidBatchPreemption = false;
    bool failOnConfigureHardwareCustom = false;
    bool isCooperativeEngineSupportedValue = true;
    bool returnedIsUnlockingLockedPtrNecessary = false;
    bool isBufferPoolAllocatorSupportedValue = true;
    bool enableAdditionalBlitProperties = false;
    uint32_t returnedStepping = 0;
    uint32_t returnedL1CachePolicy = 0;
    uint32_t returnedL1CachePolicyIfDebugger = 0;
    uint32_t returnedExtraKernelCapabilities = 0;
    std::vector<int32_t> threadArbPolicies = {};
    aub_stream::EngineType mockDefaultCopyEngine = aub_stream::EngineType::ENGINE_BCS;
};
} // namespace NEO
