/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/os_sysman.h"
#include <level_zero/zet_api.h>

#include <string>

namespace L0 {

class OsSysmanDevice {
  public:
    virtual void getSerialNumber(int8_t (&serialNumber)[ZET_STRING_PROPERTY_SIZE]) = 0;
    virtual void getBoardNumber(int8_t (&boardNumber)[ZET_STRING_PROPERTY_SIZE]) = 0;
    virtual void getBrandName(int8_t (&brandName)[ZET_STRING_PROPERTY_SIZE]) = 0;
    virtual void getModelName(int8_t (&modelName)[ZET_STRING_PROPERTY_SIZE]) = 0;
    virtual void getVendorName(int8_t (&vendorName)[ZET_STRING_PROPERTY_SIZE]) = 0;
    virtual void getDriverVersion(int8_t (&driverVersion)[ZET_STRING_PROPERTY_SIZE]) = 0;
    virtual ze_result_t reset() = 0;
    static OsSysmanDevice *create(OsSysman *pOsSysman);
    virtual ~OsSysmanDevice() {}
};

} // namespace L0
