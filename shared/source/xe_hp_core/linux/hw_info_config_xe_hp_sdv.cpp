/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/sub_device.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_xehp_and_later.inl"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/pci_path.h"
#include "shared/source/os_interface/linux/pmt_util.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/utilities/directory.h"
#include "shared/source/xe_hp_core/hw_cmds_base.h"

#include "platforms.h"

namespace NEO {
constexpr static auto gfxProduct = IGFX_XE_HP_SDV;
const std::map<std::string, std::pair<uint32_t, uint32_t>> guidUuidOffsetMap = {
    // add new values for guid in the form of {"guid", {offset, size}} for each platform
    {"0xfdc76195", {64u, 8u}}};

#include "shared/source/os_interface/linux/hw_info_config_uuid_xehp_and_later.inl"
#include "shared/source/xe_hp_core/os_agnostic_hw_info_config_xe_hp_core.inl"

template <>
int HwInfoConfigHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    if (allowCompression(*hwInfo)) {
        enableCompression(hwInfo);
    }

    hwInfo->featureTable.flags.ftrRcsNode = false;
    if (DebugManager.flags.NodeOrdinal.get() == static_cast<int32_t>(aub_stream::EngineType::ENGINE_RCS)) {
        hwInfo->featureTable.flags.ftrRcsNode = true;
    }

    enableBlitterOperationsSupport(hwInfo);

    auto &kmdNotifyProperties = hwInfo->capabilityTable.kmdNotifyProperties;
    kmdNotifyProperties.enableKmdNotify = true;
    kmdNotifyProperties.delayKmdNotifyMicroseconds = 150;
    kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission = true;
    kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds = 20;

    return 0;
}

template <>
bool HwInfoConfigHw<gfxProduct>::getHostMemCapabilitiesSupported(const HardwareInfo *hwInfo) {
    HwHelper &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);
    if (hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, *hwInfo) && (getLocalMemoryAccessMode(*hwInfo) == LocalMemoryAccessMode::CpuAccessAllowed)) {
        return false;
    }

    return true;
}

template <>
uint64_t HwInfoConfigHw<gfxProduct>::getHostMemCapabilitiesValue() {
    return UNIFIED_SHARED_MEMORY_ACCESS;
}

template <>
void HwInfoConfigHw<gfxProduct>::getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) {
    *fp16 = 0u;
    *fp32 = FP_ATOMIC_EXT_FLAG_GLOBAL_ADD;
    *fp64 = 0u;
}

template <>
uint32_t HwInfoConfigHw<gfxProduct>::getDeviceMemoryMaxClkRate(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) {
    return 2800u;
}

template <>
bool HwInfoConfigHw<gfxProduct>::isFlushTaskAllowed() const {
    return false;
}

template class HwInfoConfigHw<gfxProduct>;
} // namespace NEO
