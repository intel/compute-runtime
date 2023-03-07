/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.inl"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/product_helper_dg2_and_later.inl"
#include "shared/source/os_interface/product_helper_xehp_and_later.inl"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"

#include "platforms.h"

constexpr static auto gfxProduct = IGFX_DG2;

#include "shared/source/xe_hpg_core/dg2/os_agnostic_product_helper_dg2.inl"
#include "shared/source/xe_hpg_core/os_agnostic_product_helper_xe_hpg_core.inl"

#include "os_agnostic_product_helper_dg2_extra.inl"

namespace NEO {

template <>
int ProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
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
bool ProductHelperHw<gfxProduct>::getUuid(Device *device, std::array<uint8_t, ProductHelper::uuidSize> &uuid) const {

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

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
