/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/sysman/source/device/sysman_device.h"
#include "level_zero/sysman/source/driver/sysman_driver.h"
#include "level_zero/zes_intel_gpu_sysman.h"

namespace L0 {

ze_result_t ZE_APICALL zesIntelDevicePciLinkSpeedUpdateExp(zes_device_handle_t hDevice, ze_bool_t downgradeUpgrade, zes_device_action_t *pendingAction) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::SysmanDevice::pciLinkSpeedUpdateExp(hDevice, downgradeUpgrade, pendingAction);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

} // namespace L0

extern "C" {

ze_result_t ZE_APICALL zesIntelDevicePciLinkSpeedUpdateExp(zes_device_handle_t hDevice, ze_bool_t downgradeUpgrade, zes_device_action_t *pendingAction) {
    return L0::zesIntelDevicePciLinkSpeedUpdateExp(hDevice, downgradeUpgrade, pendingAction);
};

} // extern "C"