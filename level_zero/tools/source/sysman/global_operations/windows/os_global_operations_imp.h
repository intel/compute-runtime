/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "sysman/global_operations/os_global_operations.h"
#include "sysman/windows/os_sysman_imp.h"

namespace L0 {

class KmdSysManager;
class WddmGlobalOperationsImp : public OsGlobalOperations, NEO::NonCopyableOrMovableClass {
  public:
    bool getSerialNumber(char (&serialNumber)[ZES_STRING_PROPERTY_SIZE]) override;
    void getBoardNumber(char (&boardNumber)[ZES_STRING_PROPERTY_SIZE]) override;
    void getBrandName(char (&brandName)[ZES_STRING_PROPERTY_SIZE]) override;
    void getModelName(char (&modelName)[ZES_STRING_PROPERTY_SIZE]) override;
    void getVendorName(char (&vendorName)[ZES_STRING_PROPERTY_SIZE]) override;
    void getDriverVersion(char (&driverVersion)[ZES_STRING_PROPERTY_SIZE]) override;
    void getWedgedStatus(zes_device_state_t *pState) override;
    void getRepairStatus(zes_device_state_t *pState) override;
    Device *getDevice() override;
    ze_result_t reset(ze_bool_t force) override;
    ze_result_t scanProcessesState(std::vector<zes_process_state_t> &pProcessList) override;
    ze_result_t deviceGetState(zes_device_state_t *pState) override;

    WddmGlobalOperationsImp(OsSysman *pOsSysman);
    WddmGlobalOperationsImp(const WddmGlobalOperationsImp &obj) = delete;
    WddmGlobalOperationsImp() = default;
    WddmGlobalOperationsImp &operator=(const WddmGlobalOperationsImp &obj) = delete;
    ~WddmGlobalOperationsImp() override = default;

  private:
    Device *pDevice = nullptr;

  protected:
    KmdSysManager *pKmdSysManager = nullptr;
};

} // namespace L0
