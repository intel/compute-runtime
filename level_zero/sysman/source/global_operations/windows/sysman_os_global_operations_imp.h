/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/global_operations/sysman_os_global_operations.h"
#include "level_zero/sysman/source/windows/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {

class WddmGlobalOperationsImp : public OsGlobalOperations, NEO::NonCopyableOrMovableClass {
  public:
    bool getSerialNumber(char (&serialNumber)[ZES_STRING_PROPERTY_SIZE]) override;
    bool getBoardNumber(char (&boardNumber)[ZES_STRING_PROPERTY_SIZE]) override;
    void getBrandName(char (&brandName)[ZES_STRING_PROPERTY_SIZE]) override;
    void getModelName(char (&modelName)[ZES_STRING_PROPERTY_SIZE]) override;
    void getVendorName(char (&vendorName)[ZES_STRING_PROPERTY_SIZE]) override;
    void getDriverVersion(char (&driverVersion)[ZES_STRING_PROPERTY_SIZE]) override;
    void getWedgedStatus(zes_device_state_t *pState) override;
    void getRepairStatus(zes_device_state_t *pState) override;
    ze_result_t reset(ze_bool_t force) override;
    ze_result_t scanProcessesState(std::vector<zes_process_state_t> &pProcessList) override;
    ze_result_t deviceGetState(zes_device_state_t *pState) override;

    WddmGlobalOperationsImp(OsSysman *pOsSysman);
    WddmGlobalOperationsImp() = default;
    ~WddmGlobalOperationsImp() override = default;
};

} // namespace Sysman
} // namespace L0
