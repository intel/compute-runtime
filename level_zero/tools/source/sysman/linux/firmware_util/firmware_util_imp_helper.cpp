/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/sysman/linux/firmware_util/firmware_util_imp.h"

std::vector<std ::string> deviceSupportedFwTypes = {"GSC", "OptionROM"};

namespace L0 {
ze_result_t FirmwareUtilImp::fwIfrApplied(bool &ifrStatus) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t FirmwareUtilImp::fwGetMemoryErrorCount(zes_ras_error_type_t type, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t FirmwareUtilImp::fwSupportedDiagTests(std::vector<std::string> &supportedDiagTests) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t FirmwareUtilImp::fwRunDiagTests(std::string &osDiagType, zes_diag_result_t *pDiagResult) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

void FirmwareUtilImp::getDeviceSupportedFwTypes(std::vector<std::string> &fwTypes) {
    fwTypes = deviceSupportedFwTypes;
}

ze_result_t FirmwareUtilImp::getFwVersion(std::string fwType, std::string &firmwareVersion) {
    if (fwType == deviceSupportedFwTypes[0]) { //GSC
        return fwGetVersion(firmwareVersion);
    }
    if (fwType == deviceSupportedFwTypes[1]) { //OPROM
        return opromGetVersion(firmwareVersion);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t FirmwareUtilImp::flashFirmware(std::string fwType, void *pImage, uint32_t size) {
    if (fwType == deviceSupportedFwTypes[0]) { //GSC
        return fwFlashGSC(pImage, size);
    }
    if (fwType == deviceSupportedFwTypes[1]) { //OPROM
        return fwFlashOprom(pImage, size);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool FirmwareUtilImp::loadEntryPointsExt() {
    return true;
}
} // namespace L0
