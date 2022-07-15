/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device_imp.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/windows/hw_device_id.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace L0 {

ze_result_t DeviceImp::queryDeviceLuid(ze_device_luid_ext_properties_t *deviceLuidProperties) {
    NEO::Device *activeDevice = getActiveDevice();
    NEO::CommandStreamReceiver *csr = activeDevice->getDefaultEngine().commandStreamReceiver;
    NEO::OsContextWin *context = static_cast<NEO::OsContextWin *>(&csr->getOsContext());
    std::vector<uint8_t> luidData;
    context->getDeviceLuidArray(luidData, ZE_MAX_DEVICE_LUID_SIZE_EXT);
    std::copy_n(luidData.begin(), ZE_MAX_DEVICE_LUID_SIZE_EXT, std::begin(deviceLuidProperties->luid.id));
    return ZE_RESULT_SUCCESS;
}

} // namespace L0