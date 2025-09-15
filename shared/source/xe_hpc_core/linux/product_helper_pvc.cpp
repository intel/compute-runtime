/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/pci_path.h"
#include "shared/source/os_interface/linux/pmt_util.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/utilities/directory.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"

constexpr static auto gfxProduct = IGFX_PVC;

namespace NEO {
const std::map<std::string, std::pair<uint32_t, uint32_t>> guidUuidOffsetMap = {
    // add new values for guid in the form of {"guid", {offset, size}} for each platform
    {"0x41fe79a5", {64u, 8u}}};
#include "shared/source/os_interface/linux/product_helper_uuid_xehp_and_later.inl"
#include "shared/source/os_interface/linux/product_helper_xe_hpc_and_later.inl"
} // namespace NEO
#include "shared/source/xe_hpc_core/os_agnostic_product_helper_xe_hpc_core.inl"
#include "shared/source/xe_hpc_core/pvc/os_agnostic_product_helper_pvc.inl"
namespace NEO {

template <>
uint64_t ProductHelperHw<gfxProduct>::getDeviceMemoryPhysicalSizeInBytes(const OSInterface *osIface, uint32_t subDeviceIndex) const {

    if (osIface == nullptr) {
        return 0;
    }
    auto pDrm = osIface->getDriverModel()->as<Drm>();
    uint64_t memoryPhysicalSize = 0;
    if (pDrm->getDeviceMemoryPhysicalSizeInBytes(subDeviceIndex, memoryPhysicalSize) == false) {
        return 0;
    }

    return memoryPhysicalSize;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getDeviceMemoryMaxClkRate(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) const {

    if (osIface == nullptr) {
        return 0;
    }

    auto pDrm = osIface->getDriverModel()->as<Drm>();
    uint32_t memoryMaxClkRateInMhz = 0;
    if (pDrm->getDeviceMemoryMaxClockRateInMhz(subDeviceIndex, memoryMaxClkRateInMhz) == false) {
        return 0;
    }

    return memoryMaxClkRateInMhz;
}

template <>
uint64_t ProductHelperHw<gfxProduct>::getDeviceMemoryMaxBandWidthInBytesPerSecond(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) const {
    uint64_t memoryMaxClkRateInMhz = getDeviceMemoryMaxClkRate(hwInfo, osIface, subDeviceIndex);
    const uint64_t numberOfHbmStacksPerTile = 4u;
    const uint64_t memoryBusWidth = 128u;
    return memoryMaxClkRateInMhz * 1000 * 1000 * numberOfHbmStacksPerTile * memoryBusWidth / 8;
}

template <>
bool ProductHelperHw<gfxProduct>::isPageFaultSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isDisableScratchPagesSupported() const {
    return true;
}

} // namespace NEO

#include "shared/source/xe_hpc_core/linux/product_helper_xe_hpc_core.inl"

template class NEO::ProductHelperHw<gfxProduct>;
