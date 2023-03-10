/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/sub_device.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/pci_path.h"
#include "shared/source/os_interface/linux/pmt_util.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_xehp_and_later.inl"
#include "shared/source/utilities/directory.h"
#include "shared/source/xe_hp_core/hw_cmds_xe_hp_sdv.h"

#include "platforms.h"

constexpr static auto gfxProduct = IGFX_XE_HP_SDV;

namespace NEO {
const std::map<std::string, std::pair<uint32_t, uint32_t>> guidUuidOffsetMap = {
    // add new values for guid in the form of {"guid", {offset, size}} for each platform
    {"0xfdc76195", {64u, 8u}}};

#include "shared/source/os_interface/linux/product_helper_uuid_xehp_and_later.inl"
} // namespace NEO

#include "shared/source/xe_hp_core/os_agnostic_product_helper_xe_hp_core.inl"

namespace NEO {

template <>
int ProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    if (allowCompression(*hwInfo)) {
        enableCompression(hwInfo);
    }

    disableRcsExposure(hwInfo);

    enableBlitterOperationsSupport(hwInfo);

    auto &kmdNotifyProperties = hwInfo->capabilityTable.kmdNotifyProperties;
    kmdNotifyProperties.enableKmdNotify = true;
    kmdNotifyProperties.delayKmdNotifyMicroseconds = 150;
    kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission = true;
    kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds = 20;

    return 0;
}

template <>
bool ProductHelperHw<gfxProduct>::getHostMemCapabilitiesSupported(const HardwareInfo *hwInfo) const {
    if (GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, *hwInfo, *this) && (getLocalMemoryAccessMode(*hwInfo) == LocalMemoryAccessMode::CpuAccessAllowed)) {
        return false;
    }

    return true;
}

template <>
uint64_t ProductHelperHw<gfxProduct>::getHostMemCapabilitiesValue() const {
    return UNIFIED_SHARED_MEMORY_ACCESS;
}

template <>
void ProductHelperHw<gfxProduct>::getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) const {
    *fp16 = 0u;
    *fp32 = FP_ATOMIC_EXT_FLAG_GLOBAL_ADD;
    *fp64 = 0u;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getDeviceMemoryMaxClkRate(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) {
    return 2800u;
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
