/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/hw_info_config_dg2_and_later.inl"
#include "shared/source/os_interface/hw_info_config_xehp_and_later.inl"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"

#include "platforms.h"

namespace NEO {
constexpr static auto gfxProduct = IGFX_DG2;

#include "shared/source/xe_hpg_core/dg2/os_agnostic_hw_info_config_dg2.inl"
#include "shared/source/xe_hpg_core/os_agnostic_hw_info_config_xe_hpg_core.inl"

#include "os_agnostic_hw_info_config_dg2_extra.inl"

template <>
int HwInfoConfigHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    if (allowCompression(*hwInfo)) {
        enableCompression(hwInfo);
    }

    DG2::adjustHardwareInfo(hwInfo);
    enableBlitterOperationsSupport(hwInfo);

    disableRcsExposure(hwInfo);

    auto &kmdNotifyProperties = hwInfo->capabilityTable.kmdNotifyProperties;
    kmdNotifyProperties.enableKmdNotify = true;
    kmdNotifyProperties.delayKmdNotifyMicroseconds = 150;
    kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission = true;
    kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds = 20;

    return 0;
}

template <>
bool HwInfoConfigHw<gfxProduct>::getUuid(Device *device, std::array<uint8_t, HwInfoConfig::uuidSize> &uuid) const {

    UNRECOVERABLE_IF(device == nullptr);
    if (device->getRootDeviceEnvironment().osInterface == nullptr) {
        return false;
    }

    const auto driverModel = device->getRootDeviceEnvironment().osInterface->getDriverModel();
    if (driverModel->getDriverModelType() != DriverModelType::DRM) {
        return false;
    }

    auto pDrm = driverModel->as<Drm>();
    std::string readString(64u, '\0');
    errno = 0;
    if (pDrm->readSysFsAsString("/prelim_csc_unique_id", readString) == false) {
        return false;
    }

    char *endPtr = nullptr;
    uint64_t uuidValue = std::strtoull(readString.data(), &endPtr, 16);
    if ((endPtr == readString.data()) || (errno != 0)) {
        return false;
    }

    uuid.fill(0);
    memcpy_s(uuid.data(), uuid.size(), &uuidValue, sizeof(uuidValue));

    return true;
}

template class HwInfoConfigHw<gfxProduct>;
} // namespace NEO
