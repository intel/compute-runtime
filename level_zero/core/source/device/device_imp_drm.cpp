/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device_imp.h"

namespace L0 {

ze_result_t DeviceImp::queryDeviceLuid(ze_device_luid_ext_properties_t *deviceLuidProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0