/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/os_sysman.h"
#include <level_zero/zes_api.h>

#include <string>
#include <vector>

namespace L0 {

class OsGlobalOperations {
  public:
    virtual void getSerialNumber(int8_t (&serialNumber)[ZES_STRING_PROPERTY_SIZE]) = 0;
    virtual void getBoardNumber(int8_t (&boardNumber)[ZES_STRING_PROPERTY_SIZE]) = 0;
    virtual void getBrandName(int8_t (&brandName)[ZES_STRING_PROPERTY_SIZE]) = 0;
    virtual void getModelName(int8_t (&modelName)[ZES_STRING_PROPERTY_SIZE]) = 0;
    virtual void getVendorName(int8_t (&vendorName)[ZES_STRING_PROPERTY_SIZE]) = 0;
    virtual void getDriverVersion(int8_t (&driverVersion)[ZES_STRING_PROPERTY_SIZE]) = 0;
    virtual ze_result_t reset() = 0;
    virtual ze_result_t scanProcessesState(std::vector<zes_process_state_t> &pProcessList) = 0;
    static OsGlobalOperations *create(OsSysman *pOsSysman);
    virtual ~OsGlobalOperations() {}
};

} // namespace L0
