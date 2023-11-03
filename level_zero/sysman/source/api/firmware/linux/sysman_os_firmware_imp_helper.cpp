/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/firmware/linux/sysman_os_firmware_imp.h"
#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"

namespace L0 {
namespace Sysman {

ze_result_t LinuxFirmwareImp::getFirmwareVersion(std::string fwType, zes_firmware_properties_t *pProperties) {
    std::string fwVersion;
    ze_result_t result = pFwInterface->getFwVersion(fwType, fwVersion);
    if (ZE_RESULT_SUCCESS == result) {
        strncpy_s(static_cast<char *>(pProperties->version), ZES_STRING_PROPERTY_SIZE, fwVersion.c_str(), ZES_STRING_PROPERTY_SIZE - 1);
    }
    return result;
}

} // namespace Sysman
} // namespace L0
