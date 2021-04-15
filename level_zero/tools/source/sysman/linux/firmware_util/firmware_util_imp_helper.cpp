/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/sysman/linux/firmware_util/firmware_util_imp.h"

namespace L0 {
ze_result_t FirmwareUtilImp::fwIfrApplied(bool &ifrStatus) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
} // namespace L0
