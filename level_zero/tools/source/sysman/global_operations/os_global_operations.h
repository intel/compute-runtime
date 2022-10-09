/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/sysman/os_sysman.h"
#include <level_zero/zes_api.h>

#include <string>
#include <vector>

namespace L0 {

class OsGlobalOperations {
  public:
    virtual bool getSerialNumber(char (&serialNumber)[ZES_STRING_PROPERTY_SIZE]) = 0;
    virtual void getBoardNumber(char (&boardNumber)[ZES_STRING_PROPERTY_SIZE]) = 0;
    virtual void getBrandName(char (&brandName)[ZES_STRING_PROPERTY_SIZE]) = 0;
    virtual void getModelName(char (&modelName)[ZES_STRING_PROPERTY_SIZE]) = 0;
    virtual void getVendorName(char (&vendorName)[ZES_STRING_PROPERTY_SIZE]) = 0;
    virtual void getDriverVersion(char (&driverVersion)[ZES_STRING_PROPERTY_SIZE]) = 0;
    virtual void getWedgedStatus(zes_device_state_t *pState) = 0;
    virtual void getRepairStatus(zes_device_state_t *pState) = 0;
    virtual Device *getDevice() = 0;
    virtual ze_result_t reset(ze_bool_t force) = 0;
    virtual ze_result_t scanProcessesState(std::vector<zes_process_state_t> &pProcessList) = 0;
    virtual ze_result_t deviceGetState(zes_device_state_t *pState) = 0;
    static OsGlobalOperations *create(OsSysman *pOsSysman);
    virtual ~OsGlobalOperations() {}
};

} // namespace L0
