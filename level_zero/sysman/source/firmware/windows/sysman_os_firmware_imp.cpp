/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/firmware/windows/sysman_os_firmware_imp.h"

#include "level_zero/sysman/source/windows/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {

ze_result_t WddmFirmwareImp::osFirmwareFlash(void *pImage, uint32_t size) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

std::unique_ptr<OsFirmware> OsFirmware::create(OsSysman *pOsSysman, const std::string &fwType) {
    std::unique_ptr<WddmFirmwareImp> pWddmFirmwareImp = std::make_unique<WddmFirmwareImp>(pOsSysman, fwType);
    return pWddmFirmwareImp;
}

ze_result_t OsFirmware::getSupportedFwTypes(std::vector<std::string> &supportedFwTypes, OsSysman *pOsSysman) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace Sysman
} // namespace L0
