/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe2_hpg_core/hw_cmds_bmg.h"
#include "shared/source/xe2_hpg_core/hw_info_bmg.h"

#include <algorithm>
#include <array>

constexpr static auto gfxProduct = IGFX_BMG;

#include "shared/source/os_interface/linux/product_helper_mtl_and_later.inl"
#include "shared/source/os_interface/linux/product_helper_xe2_and_later_drm_slm.inl"
#include "shared/source/xe2_hpg_core/bmg/os_agnostic_product_helper_bmg.inl"
#include "shared/source/xe2_hpg_core/os_agnostic_product_helper_xe2_hpg_core.inl"

namespace NEO {
#include "shared/source/os_interface/linux/product_helper_xe_hpc_and_later.inl"

template <>
int ProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    hwInfo->featureTable.flags.ftr57bGPUAddressing = (hwInfo->capabilityTable.gpuAddressSpace == maxNBitValue(57));

    enableBlitterOperationsSupport(hwInfo);

    return 0;
}

template <>
bool ProductHelperHw<gfxProduct>::isVmBindResourceDecompressionSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isDeferBackingEnabled() const {
    if (debugManager.flags.EnableDeferBacking.get() != -1) {
        return debugManager.flags.EnableDeferBacking.get();
    }
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isTlbFlushRequired() const {
    return false;
}

template <>
uint64_t ProductHelperHw<gfxProduct>::getSharedSystemPatIndex() const {
    return 0;
}

template <>
bool ProductHelperHw<gfxProduct>::useSharedSystemUsm() const {
    return true;
}

namespace {
struct BmgPublicSkuMemoryConfig {
    uint32_t memoryClockRateInMhz = 0u;
    uint32_t memoryBusWidthInBits = 0u;
};

BmgPublicSkuMemoryConfig getBmgPublicSkuMemoryConfig(unsigned short deviceId) {
    static constexpr std::array<unsigned short, 2> arcB580DeviceIds = {0xE209, 0xE20B};
    static constexpr std::array<unsigned short, 1> arcB570DeviceIds = {0xE20C};
    static constexpr std::array<unsigned short, 1> arcProB60DeviceIds = {0xE211};
    static constexpr std::array<unsigned short, 1> arcProB50DeviceIds = {0xE212};
    static constexpr std::array<unsigned short, 1> arcProB70DeviceIds = {0xE223};

    auto matches = [deviceId](const auto &deviceIds) {
        return std::find(deviceIds.begin(), deviceIds.end(), deviceId) != deviceIds.end();
    };

    if (matches(arcB580DeviceIds) || matches(arcProB60DeviceIds)) {
        return {19000u, 192u};
    }
    if (matches(arcB570DeviceIds)) {
        return {19000u, 160u};
    }
    if (matches(arcProB50DeviceIds)) {
        return {14000u, 128u};
    }
    if (matches(arcProB70DeviceIds)) {
        return {19000u, 256u};
    }
    return {};
}
} // namespace

template <>
uint32_t ProductHelperHw<gfxProduct>::getDeviceMemoryMaxClkRate(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) const {
    return getBmgPublicSkuMemoryConfig(hwInfo.platform.usDeviceID).memoryClockRateInMhz;
}

template <>
uint64_t ProductHelperHw<gfxProduct>::getDeviceMemoryPhysicalSizeInBytes(
    const OSInterface *osIface, uint32_t subDeviceIndex) const {

    if (osIface == nullptr) {
        return 0;
    }

    auto driverModel = osIface->getDriverModel();
    if (driverModel->getDriverModelType() != DriverModelType::drm) {
        return 0;
    }

    auto pDrm = driverModel->as<Drm>();
    uint64_t memoryPhysicalSize = 0;
    if (pDrm->getDeviceMemoryPhysicalSizeInBytes(subDeviceIndex, memoryPhysicalSize) == false) {
        return 0;
    }

    return memoryPhysicalSize;
}

template <>
uint64_t ProductHelperHw<gfxProduct>::getDeviceMemoryMaxBandWidthInBytesPerSecond(
    const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) const {

    const auto memoryConfig = getBmgPublicSkuMemoryConfig(hwInfo.platform.usDeviceID);
    return static_cast<uint64_t>(memoryConfig.memoryClockRateInMhz) * 1000u * 1000u * memoryConfig.memoryBusWidthInBits / 8u;
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
