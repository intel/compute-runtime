/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/global_operations/windows/os_global_operations_imp.h"

namespace L0 {

Device *WddmGlobalOperationsImp::getDevice() {
    return pDevice;
}

bool WddmGlobalOperationsImp::getSerialNumber(char (&serialNumber)[ZES_STRING_PROPERTY_SIZE]) {
    return false;
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

void WddmGlobalOperationsImp::getWedgedStatus(zes_device_state_t *pState) {
}
void WddmGlobalOperationsImp::getRepairStatus(zes_device_state_t *pState) {
}
ze_result_t WddmGlobalOperationsImp::reset(ze_bool_t force) {
    uint32_t value = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;
    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::GlobalOperationsComponent;
    request.requestId = KmdSysman::Requests::GlobalOperation::TriggerDeviceLevelReset;
    request.dataSize = sizeof(uint32_t);
    value = static_cast<uint32_t>(force);
    memcpy_s(request.dataBuffer, sizeof(uint32_t), &value, sizeof(uint32_t));
    return pKmdSysManager->requestSingle(request, response);
}

ze_result_t WddmGlobalOperationsImp::scanProcessesState(std::vector<zes_process_state_t> &pProcessList) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmGlobalOperationsImp::deviceGetState(zes_device_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

WddmGlobalOperationsImp::WddmGlobalOperationsImp(OsSysman *pOsSysman) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    pDevice = pWddmSysmanImp->getDeviceHandle();
    pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
}

OsGlobalOperations *OsGlobalOperations::create(OsSysman *pOsSysman) {
    WddmGlobalOperationsImp *pWddmGlobalOperationsImp = new WddmGlobalOperationsImp(pOsSysman);
    return static_cast<OsGlobalOperations *>(pWddmGlobalOperationsImp);
}

} // namespace L0
