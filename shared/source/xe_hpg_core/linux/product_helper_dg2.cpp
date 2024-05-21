/*
 * Copyright (C) 2021-2024 Intel Corporation
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
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"

constexpr static auto gfxProduct = IGFX_DG2;

#include "shared/source/xe_hpg_core/dg2/os_agnostic_product_helper_dg2.inl"

namespace NEO {

template <>
int ProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    if (allowCompression(*hwInfo)) {
        enableCompression(hwInfo);
    }

    enableBlitterOperationsSupport(hwInfo);

    hwInfo->workaroundTable.flags.wa_15010089951 = true;

    auto &kmdNotifyProperties = hwInfo->capabilityTable.kmdNotifyProperties;
    kmdNotifyProperties.enableKmdNotify = true;
    kmdNotifyProperties.delayKmdNotifyMicroseconds = 150;
    kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission = true;
    kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds = 20;

    return 0;
}

template <>
bool ProductHelperHw<gfxProduct>::getUuid(NEO::DriverModel *driverModel, const uint32_t subDeviceCount, const uint32_t deviceIndex, std::array<uint8_t, ProductHelper::uuidSize> &uuid) const {
    if (driverModel->getDriverModelType() != DriverModelType::drm) {
        return false;
    }

    auto pDrm = driverModel->as<Drm>();
    errno = 0;
    std::string readString(64u, '\0');
    if (pDrm->readSysFsAsString("/prelim_csc_unique_id", readString) == false) {
        return false;
    }
    char *endPtr = nullptr;
    uint64_t uuidValue = std::strtoull(readString.data(), &endPtr, 16);
    if (endPtr == readString.data() || (errno != 0)) {
        return false;
    }

    uuid.fill(0);
    memcpy_s(uuid.data(), uuid.size(), &uuidValue, sizeof(uuidValue));

    return true;
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
