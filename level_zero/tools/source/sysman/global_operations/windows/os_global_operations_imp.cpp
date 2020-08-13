/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/global_operations/os_global_operations.h"
#include "level_zero/tools/source/sysman/windows/os_sysman_imp.h"

namespace L0 {

class WddmGlobalOperationsImp : public OsGlobalOperations {
  public:
    void getSerialNumber(char (&serialNumber)[ZES_STRING_PROPERTY_SIZE]) override;
    void getBoardNumber(char (&boardNumber)[ZES_STRING_PROPERTY_SIZE]) override;
    void getBrandName(char (&brandName)[ZES_STRING_PROPERTY_SIZE]) override;
    void getModelName(char (&modelName)[ZES_STRING_PROPERTY_SIZE]) override;
    void getVendorName(char (&vendorName)[ZES_STRING_PROPERTY_SIZE]) override;
    void getDriverVersion(char (&driverVersion)[ZES_STRING_PROPERTY_SIZE]) override;
    Device *getDevice() override;
    ze_result_t reset(ze_bool_t force) override;
    ze_result_t scanProcessesState(std::vector<zes_process_state_t> &pProcessList) override;

    WddmGlobalOperationsImp(OsSysman *pOsSysman);
    ~WddmGlobalOperationsImp() = default;

    // Don't allow copies of the WddmGlobalOperationsImp object
    WddmGlobalOperationsImp(const WddmGlobalOperationsImp &obj) = delete;
    WddmGlobalOperationsImp &operator=(const WddmGlobalOperationsImp &obj) = delete;

  private:
    Device *pDevice = nullptr;
};

Device *WddmGlobalOperationsImp::getDevice() {
    return pDevice;
}

void WddmGlobalOperationsImp::getSerialNumber(char (&serialNumber)[ZES_STRING_PROPERTY_SIZE]) {
}

void WddmGlobalOperationsImp::getBoardNumber(char (&boardNumber)[ZES_STRING_PROPERTY_SIZE]) {
}

void WddmGlobalOperationsImp::getBrandName(char (&brandName)[ZES_STRING_PROPERTY_SIZE]) {
}

void WddmGlobalOperationsImp::getModelName(char (&modelName)[ZES_STRING_PROPERTY_SIZE]) {
}

void WddmGlobalOperationsImp::getVendorName(char (&vendorName)[ZES_STRING_PROPERTY_SIZE]) {
}

void WddmGlobalOperationsImp::getDriverVersion(char (&driverVersion)[ZES_STRING_PROPERTY_SIZE]) {
}

ze_result_t WddmGlobalOperationsImp::reset(ze_bool_t force) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmGlobalOperationsImp::scanProcessesState(std::vector<zes_process_state_t> &pProcessList) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

WddmGlobalOperationsImp::WddmGlobalOperationsImp(OsSysman *pOsSysman) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    pDevice = pWddmSysmanImp->getDeviceHandle();
}

OsGlobalOperations *OsGlobalOperations::create(OsSysman *pOsSysman) {
    WddmGlobalOperationsImp *pWddmGlobalOperationsImp = new WddmGlobalOperationsImp(pOsSysman);
    return static_cast<OsGlobalOperations *>(pWddmGlobalOperationsImp);
}

} // namespace L0
