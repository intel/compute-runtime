/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/os_sysman.h"
#include <level_zero/zes_api.h>

#include <memory>
#include <string>
#include <vector>

namespace L0 {

class OsFirmware {
  public:
    virtual void osGetFwProperties(zes_firmware_properties_t *pProperties) = 0;
    virtual ze_result_t osFirmwareFlash(void *pImage, uint32_t size) = 0;
    static std::unique_ptr<OsFirmware> create(OsSysman *pOsSysman, const std::string &fwType);
    static ze_result_t getSupportedFwTypes(std::vector<std::string> &supportedFwTypes, OsSysman *pOsSysman);
    virtual ~OsFirmware() {}
};

} // namespace L0
