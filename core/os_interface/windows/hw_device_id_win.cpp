/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/debug_helpers.h"
#include "core/os_interface/windows/gdi_interface.h"
#include "core/os_interface/windows/hw_device_id.h"
namespace NEO {

HwDeviceId::~HwDeviceId() {
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    D3DKMT_CLOSEADAPTER CloseAdapter = {0};
    CloseAdapter.hAdapter = adapter;
    status = gdi->closeAdapter(&CloseAdapter);
    DEBUG_BREAK_IF(status != STATUS_SUCCESS);
}
HwDeviceId::HwDeviceId(D3DKMT_HANDLE adapterIn, LUID adapterLuidIn, std::unique_ptr<Gdi> gdiIn) : adapter(adapterIn),
                                                                                                  adapterLuid(adapterLuidIn),
                                                                                                  gdi(std::move(gdiIn)){};
} // namespace NEO
