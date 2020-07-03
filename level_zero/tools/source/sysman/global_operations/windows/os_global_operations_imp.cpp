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
    void getSerialNumber(int8_t (&serialNumber)[ZET_STRING_PROPERTY_SIZE]) override;
    void getBoardNumber(int8_t (&boardNumber)[ZET_STRING_PROPERTY_SIZE]) override;
    void getBrandName(int8_t (&brandName)[ZET_STRING_PROPERTY_SIZE]) override;
    void getModelName(int8_t (&modelName)[ZET_STRING_PROPERTY_SIZE]) override;
    void getVendorName(int8_t (&vendorName)[ZET_STRING_PROPERTY_SIZE]) override;
    void getDriverVersion(int8_t (&driverVersion)[ZET_STRING_PROPERTY_SIZE]) override;
    ze_result_t reset() override;
    ze_result_t scanProcessesState(std::vector<zet_process_state_t> &pProcessList) override;

    WddmGlobalOperationsImp(OsSysman *pOsSysman);
    ~WddmGlobalOperationsImp() = default;

    // Don't allow copies of the WddmGlobalOperationsImp object
    WddmGlobalOperationsImp(const WddmGlobalOperationsImp &obj) = delete;
    WddmGlobalOperationsImp &operator=(const WddmGlobalOperationsImp &obj) = delete;
};

void WddmGlobalOperationsImp::getSerialNumber(int8_t (&serialNumber)[ZET_STRING_PROPERTY_SIZE]) {
}

void WddmGlobalOperationsImp::getBoardNumber(int8_t (&boardNumber)[ZET_STRING_PROPERTY_SIZE]) {
}

void WddmGlobalOperationsImp::getBrandName(int8_t (&brandName)[ZET_STRING_PROPERTY_SIZE]) {
}

void WddmGlobalOperationsImp::getModelName(int8_t (&modelName)[ZET_STRING_PROPERTY_SIZE]) {
}

void WddmGlobalOperationsImp::getVendorName(int8_t (&vendorName)[ZET_STRING_PROPERTY_SIZE]) {
}

void WddmGlobalOperationsImp::getDriverVersion(int8_t (&driverVersion)[ZET_STRING_PROPERTY_SIZE]) {
}

ze_result_t WddmGlobalOperationsImp::reset() {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmGlobalOperationsImp::scanProcessesState(std::vector<zet_process_state_t> &pProcessList) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

WddmGlobalOperationsImp::WddmGlobalOperationsImp(OsSysman *pOsSysman) {
}

OsGlobalOperations *OsGlobalOperations::create(OsSysman *pOsSysman) {
    WddmGlobalOperationsImp *pWddmGlobalOperationsImp = new WddmGlobalOperationsImp(pOsSysman);
    return static_cast<OsGlobalOperations *>(pWddmGlobalOperationsImp);
}

} // namespace L0
