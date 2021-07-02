/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "global_operations_imp.h"

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/core/source/device/device.h"

namespace L0 {

ze_result_t GlobalOperationsImp::processesGetState(uint32_t *pCount, zes_process_state_t *pProcesses) {
    std::vector<zes_process_state_t> pProcessList;
    ze_result_t result = pOsGlobalOperations->scanProcessesState(pProcessList);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    if ((*pCount > 0) && (*pCount < pProcessList.size())) {
        result = ZE_RESULT_ERROR_INVALID_SIZE;
    }
    if (pProcesses != nullptr) {
        uint32_t limit = std::min(*pCount, static_cast<uint32_t>(pProcessList.size()));
        for (uint32_t i = 0; i < limit; i++) {
            pProcesses[i].processId = pProcessList[i].processId;
            pProcesses[i].engines = pProcessList[i].engines;
            pProcesses[i].memSize = pProcessList[i].memSize;
            pProcesses[i].sharedSize = pProcessList[i].sharedSize;
        }
    }
    *pCount = static_cast<uint32_t>(pProcessList.size());

    return result;
}

ze_result_t GlobalOperationsImp::deviceGetProperties(zes_device_properties_t *pProperties) {
    Device *device = pOsGlobalOperations->getDevice();
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    device->getProperties(&deviceProperties);
    sysmanProperties.core = deviceProperties;
    uint32_t count = 0;
    device->getSubDevices(&count, nullptr);
    sysmanProperties.numSubdevices = count;
    *pProperties = sysmanProperties;
    return ZE_RESULT_SUCCESS;
}

ze_result_t GlobalOperationsImp::reset(ze_bool_t force) {
    return pOsGlobalOperations->reset(force);
}

ze_result_t GlobalOperationsImp::deviceGetState(zes_device_state_t *pState) {
    return pOsGlobalOperations->deviceGetState(pState);
}

void GlobalOperationsImp::init() {
    if (pOsGlobalOperations == nullptr) {
        pOsGlobalOperations = OsGlobalOperations::create(pOsSysman);
    }
    UNRECOVERABLE_IF(nullptr == pOsGlobalOperations);
    pOsGlobalOperations->getVendorName(sysmanProperties.vendorName);
    pOsGlobalOperations->getDriverVersion(sysmanProperties.driverVersion);
    pOsGlobalOperations->getModelName(sysmanProperties.modelName);
    pOsGlobalOperations->getBrandName(sysmanProperties.brandName);
    pOsGlobalOperations->getBoardNumber(sysmanProperties.boardNumber);
    pOsGlobalOperations->getSerialNumber(sysmanProperties.serialNumber);
}

GlobalOperationsImp::~GlobalOperationsImp() {
    if (nullptr != pOsGlobalOperations) {
        delete pOsGlobalOperations;
    }
}

} // namespace L0
