/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_dg2_and_later.inl"
#include "shared/source/os_interface/product_helper_xehp_and_later.inl"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"

constexpr static auto gfxProduct = IGFX_DG2;

#include "shared/source/xe_hpg_core/dg2/os_agnostic_product_helper_dg2.inl"

#include "windows_product_helper_dg2_extra.inl"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::isDirectSubmissionSupported(ReleaseHelper *releaseHelper) const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::getUuid(NEO::DriverModel *driverModel, uint32_t subDeviceCount, uint32_t deviceIndex, std::array<uint8_t, ProductHelper::uuidSize> &uuid) const {

    if (driverModel->getDriverModelType() != DriverModelType::WDDM) {
        return false;
    }

    NEO::Wddm *pWddm = driverModel->as<NEO::Wddm>();
    GT_SYSTEM_INFO *gtInfo = pWddm->getGtSysInfo();

    uint64_t uuidValue = gtInfo->DeviceUUID;
    if (uuidValue == 0) {
        return false;
    }

    uuid.fill(0);
    memcpy_s(uuid.data(), uuid.size(), &uuidValue, sizeof(uuidValue));

    return true;
}

template <>
int ProductHelperHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) const {
    if (allowCompression(*hwInfo)) {
        enableCompression(hwInfo);
    }
    DG2::adjustHardwareInfo(hwInfo);
    enableBlitterOperationsSupport(hwInfo);

    hwInfo->workaroundTable.flags.wa_15010089951 = true;

    return 0;
}

template class ProductHelperHw<gfxProduct>;
} // namespace NEO
