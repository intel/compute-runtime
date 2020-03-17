/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/hw_device_id.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
namespace NEO {

HwDeviceId::~HwDeviceId() {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_CLOSEADAPTER CloseAdapter = {0};
    CloseAdapter.hAdapter = adapter;
    status = static_cast<OsEnvironmentWin *>(osEnvironment)->gdi->closeAdapter(&CloseAdapter);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);
}
HwDeviceId::HwDeviceId(D3DKMT_HANDLE adapterIn, LUID adapterLuidIn, OsEnvironment *osEnvironmentIn) : adapter(adapterIn),
                                                                                                      adapterLuid(adapterLuidIn),
                                                                                                      osEnvironment(osEnvironmentIn) {}
Gdi *HwDeviceId::getGdi() const {
    return static_cast<OsEnvironmentWin *>(osEnvironment)->gdi.get();
};
} // namespace NEO
