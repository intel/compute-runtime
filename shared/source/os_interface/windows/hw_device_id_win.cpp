/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/hw_device_id.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
namespace NEO {

HwDeviceIdWddm::~HwDeviceIdWddm() {
    [[maybe_unused]] NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_CLOSEADAPTER closeAdapter = {0};
    closeAdapter.hAdapter = adapter;
    status = static_cast<OsEnvironmentWin *>(osEnvironment)->gdi->closeAdapter(&closeAdapter);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);
}
HwDeviceIdWddm::HwDeviceIdWddm(D3DKMT_HANDLE adapterIn, LUID adapterLuidIn,
                               OsEnvironment *osEnvironmentIn, std::unique_ptr<UmKmDataTranslator> umKmDataTranslator)
    : HwDeviceId(DriverModelType::WDDM),
      adapterLuid(adapterLuidIn), umKmDataTranslator(std::move(umKmDataTranslator)),
      osEnvironment(osEnvironmentIn), adapter(adapterIn) {
}

Gdi *HwDeviceIdWddm::getGdi() const {
    return static_cast<OsEnvironmentWin *>(osEnvironment)->gdi.get();
};
} // namespace NEO
