/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/global_operations/sysman_global_operations_imp.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/string.h"

#include "level_zero/sysman/source/sysman_const.h"

#include <algorithm>

namespace L0 {
namespace Sysman {

ze_result_t GlobalOperationsImp::processesGetState(uint32_t *pCount, zes_process_state_t *pProcesses) {
    initGlobalOperations();
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
    initGlobalOperations();
    pProperties->numSubdevices = pOsSysman->getSubDeviceCount();

    std::array<uint8_t, NEO::ProductHelper::uuidSize> deviceUuid;
    bool uuidValid = pOsGlobalOperations->getUuid(deviceUuid);
    if (uuidValid) {
        std::copy_n(std::begin(deviceUuid), ZE_MAX_DEVICE_UUID_SIZE, std::begin(pProperties->core.uuid.id));
    }

    auto &hardwareInfo = pOsSysman->getHardwareInfo();
    pProperties->core.type = ZE_DEVICE_TYPE_GPU;
    if (hardwareInfo.capabilityTable.isIntegratedDevice) {
        pProperties->core.flags |= ZE_DEVICE_PROPERTY_FLAG_INTEGRATED;
    }
    if (hardwareInfo.capabilityTable.supportsOnDemandPageFaults) {
        pProperties->core.flags |= ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING;
    }

    zes_base_properties_t *pNext = static_cast<zes_base_properties_t *>(pProperties->pNext);
    while (pNext) {

        if (pNext->stype == ZES_STRUCTURE_TYPE_DEVICE_EXT_PROPERTIES) {
            auto extendedProperties = reinterpret_cast<zes_device_ext_properties_t *>(pNext);

            extendedProperties->type = ZES_DEVICE_TYPE_GPU;

            if (hardwareInfo.capabilityTable.isIntegratedDevice) {
                extendedProperties->flags |= ZES_DEVICE_PROPERTY_FLAG_INTEGRATED;
            }

            if (hardwareInfo.capabilityTable.supportsOnDemandPageFaults) {
                extendedProperties->flags |= ZES_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING;
            }

            if (uuidValid) {
                std::copy_n(std::begin(deviceUuid), ZE_MAX_DEVICE_UUID_SIZE, std::begin(extendedProperties->uuid.id));
            }
        }

        pNext = static_cast<zes_base_properties_t *>(pNext->pNext);
    }

    pOsGlobalOperations->getVendorName(pProperties->vendorName);
    pOsGlobalOperations->getDriverVersion(pProperties->driverVersion);
    pOsGlobalOperations->getModelName(pProperties->modelName);
    pOsGlobalOperations->getBrandName(pProperties->brandName);
    memset(pProperties->boardNumber, 0, ZES_STRING_PROPERTY_SIZE);
    if (!pOsGlobalOperations->getBoardNumber(pProperties->boardNumber)) {
        memcpy_s(pProperties->boardNumber, ZES_STRING_PROPERTY_SIZE, unknown.c_str(), unknown.length() + 1);
    }
    memset(pProperties->serialNumber, 0, ZES_STRING_PROPERTY_SIZE);
    if (!pOsGlobalOperations->getSerialNumber(pProperties->serialNumber)) {
        memcpy_s(pProperties->serialNumber, ZES_STRING_PROPERTY_SIZE, unknown.c_str(), unknown.length() + 1);
    }

    return ZE_RESULT_SUCCESS;
}

bool GlobalOperationsImp::getDeviceInfoByUuid(zes_uuid_t uuid, ze_bool_t *onSubdevice, uint32_t *subdeviceId) {
    initGlobalOperations();
    return pOsGlobalOperations->getDeviceInfoByUuid(uuid, onSubdevice, subdeviceId);
}

ze_result_t GlobalOperationsImp::deviceGetSubDeviceProperties(uint32_t *pCount, zes_subdevice_exp_properties_t *pSubdeviceProps) {
    initGlobalOperations();
    return pOsGlobalOperations->getSubDeviceProperties(pCount, pSubdeviceProps);
}

ze_result_t GlobalOperationsImp::reset(ze_bool_t force) {
    initGlobalOperations();
    return pOsGlobalOperations->reset(force);
}

ze_result_t GlobalOperationsImp::resetExt(zes_reset_properties_t *pProperties) {
    initGlobalOperations();
    return pOsGlobalOperations->resetExt(pProperties);
}

ze_result_t GlobalOperationsImp::deviceGetState(zes_device_state_t *pState) {
    initGlobalOperations();
    return pOsGlobalOperations->deviceGetState(pState);
}

void GlobalOperationsImp::init() {
    if (pOsGlobalOperations == nullptr) {
        pOsGlobalOperations = OsGlobalOperations::create(pOsSysman);
    }
    UNRECOVERABLE_IF(nullptr == pOsGlobalOperations);
}
void GlobalOperationsImp::initGlobalOperations() {
    std::call_once(initGlobalOpOnce, [this]() {
        this->init();
    });
}
GlobalOperationsImp::~GlobalOperationsImp() {
    if (nullptr != pOsGlobalOperations) {
        delete pOsGlobalOperations;
    }
}

} // namespace Sysman
} // namespace L0
