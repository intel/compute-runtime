/*
 * Copyright (C) 2021 Intel Corporation
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

ze_result_t FirmwareUtilImp::fwSupportedDiagTests(std::vector<std::string> &supportedDiagTests) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t FirmwareUtilImp::fwRunDiagTests(std::string &osDiagType, zes_diag_result_t *pDiagResult, uint32_t subDeviceId) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
} // namespace L0
