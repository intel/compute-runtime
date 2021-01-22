/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/os_sysman.h"
#include <level_zero/zes_api.h>

namespace L0 {

class OsFirmware {
  public:
    virtual bool isFirmwareSupported(void) = 0;
    virtual void osGetFwProperties(zes_firmware_properties_t *pProperties) = 0;
    virtual ze_result_t osFirmwareFlash(void *pImage, uint32_t size) = 0;
    static OsFirmware *create(OsSysman *pOsSysman);
    virtual ~OsFirmware() {}
};

} // namespace L0
