/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/device/os_sysman.h"
#include <level_zero/zes_api.h>

#include <memory>
#include <string>
#include <vector>

namespace L0 {
namespace Sysman {
class OsFirmware {
  public:
    virtual void osGetFwProperties(zes_firmware_properties_t *pProperties) = 0;
    virtual ze_result_t osFirmwareFlash(void *pImage, uint32_t size) = 0;
    virtual ze_result_t osGetFirmwareFlashProgress(uint32_t *pCompletionPercent) = 0;
    virtual ze_result_t osGetSecurityVersion(char *pVersion) = 0;
    virtual ze_result_t osSetSecurityVersion() = 0;
    virtual ze_result_t osGetConsoleLogs(size_t *pSize, char *pFirmwareLog) = 0;
    static std::unique_ptr<OsFirmware> create(OsSysman *pOsSysman, const std::string &fwType);
    static void getSupportedFwTypes(std::vector<std::string> &supportedFwTypes, OsSysman *pOsSysman);
    virtual ~OsFirmware() {}
};
} // namespace Sysman
} // namespace L0
