/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/firmware/linux/os_firmware_imp.h"
#include "level_zero/tools/source/sysman/firmware_util/firmware_util.h"

namespace L0 {

ze_result_t LinuxFirmwareImp::getFirmwareVersion(std::string fwType, zes_firmware_properties_t *pProperties) {
    std::string fwVersion;
    ze_result_t result = pFwInterface->getFwVersion(fwType, fwVersion);
    if (ZE_RESULT_SUCCESS == result) {
        strncpy_s(static_cast<char *>(pProperties->version), ZES_STRING_PROPERTY_SIZE, fwVersion.c_str(), ZES_STRING_PROPERTY_SIZE - 1);
    }
    return result;
}

} // namespace L0
