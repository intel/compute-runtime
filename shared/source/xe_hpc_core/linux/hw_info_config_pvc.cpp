/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/sub_device.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_dg2_and_later.inl"
#include "shared/source/os_interface/hw_info_config_xehp_and_later.inl"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/pci_path.h"
#include "shared/source/os_interface/linux/pmt_util.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/utilities/directory.h"

namespace NEO {
constexpr static auto gfxProduct = IGFX_PVC;
const std::map<std::string, std::pair<uint32_t, uint32_t>> guidUuidOffsetMap = {
    // add new values for guid in the form of {"guid", {offset, size}} for each platform
    {"0x0", {0x0, 0}}};

#include "shared/source/os_interface/linux/hw_info_config_uuid_xehp_and_later.inl"
#include "shared/source/xe_hpc_core/os_agnostic_hw_info_config_pvc.inl"
#include "shared/source/xe_hpc_core/os_agnostic_hw_info_config_xe_hpc_core.inl"

#include "os_agnostic_hw_info_config_pvc_extra.inl"

template <>
int HwInfoConfigHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    enableCompression(hwInfo);

    hwInfo->featureTable.flags.ftr57bGPUAddressing = (hwInfo->capabilityTable.gpuAddressSpace == maxNBitValue(57));

    enableBlitterOperationsSupport(hwInfo);

    hwInfo->featureTable.flags.ftrRcsNode = false;
    if (DebugManager.flags.NodeOrdinal.get() == static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCCS)) {
        hwInfo->featureTable.flags.ftrRcsNode = true;
    }

    auto &kmdNotifyProperties = hwInfo->capabilityTable.kmdNotifyProperties;
    kmdNotifyProperties.enableKmdNotify = true;
    kmdNotifyProperties.delayKmdNotifyMicroseconds = 150;
    kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission = true;
    kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds = 20;

    return 0;
}

template class HwInfoConfigHw<gfxProduct>;
} // namespace NEO
