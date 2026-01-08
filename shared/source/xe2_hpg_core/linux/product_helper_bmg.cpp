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
bool ProductHelperHw<gfxProduct>::isDisableScratchPagesSupported() const {
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
    return false;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getDeviceMemoryMaxClkRate(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) const {
    if (osIface == nullptr) {
        return 0;
    }

    auto driverModel = osIface->getDriverModel();
    if (driverModel->getDriverModelType() != DriverModelType::drm) {
        return 0;
    }

    auto pDrm = driverModel->as<Drm>();
    uint32_t memoryMaxClkRateInMhz = 0;
    if (pDrm->getDeviceMemoryMaxClockRateInMhz(subDeviceIndex, memoryMaxClkRateInMhz) == false) {
        return 0;
    }

    return memoryMaxClkRateInMhz;
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
