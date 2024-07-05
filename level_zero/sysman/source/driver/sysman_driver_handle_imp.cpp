/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/driver/extension_function_address.h"
#include "level_zero/sysman/source/device/sysman_device.h"
#include "level_zero/sysman/source/driver/os_sysman_driver.h"
#include "level_zero/sysman/source/driver/sysman_driver.h"

#include <cstring>
#include <vector>

namespace L0 {
namespace Sysman {

struct SysmanDriverHandleImp *globalSysmanDriver;

SysmanDriverHandleImp::SysmanDriverHandleImp() = default;

ze_result_t SysmanDriverHandleImp::initialize(NEO::ExecutionEnvironment &executionEnvironment) {
    for (uint32_t rootDeviceIndex = 0u; rootDeviceIndex < executionEnvironment.rootDeviceEnvironments.size(); rootDeviceIndex++) {
        if (!executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->capabilityTable.levelZeroSupported) {
            continue;
        }
        auto pSysmanDevice = SysmanDevice::create(executionEnvironment, rootDeviceIndex);
        if (pSysmanDevice != nullptr) {
            this->sysmanDevices.push_back(pSysmanDevice);
        }
    }

    if (this->sysmanDevices.size() == 0) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    pOsSysmanDriver = L0::Sysman::OsSysmanDriver::create();
    this->numDevices = static_cast<uint32_t>(this->sysmanDevices.size());
    return ZE_RESULT_SUCCESS;
}

ze_result_t SysmanDriverHandleImp::getExtensionFunctionAddress(const char *pFuncName, void **pfunc) {
    *pfunc = ExtensionFunctionAddressHelper::getExtensionFunctionAddress(pFuncName);
    if (*pfunc) {
        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

SysmanDriverHandle *SysmanDriverHandle::create(NEO::ExecutionEnvironment &executionEnvironment, ze_result_t *returnValue) {
    SysmanDriverHandleImp *driverHandle = new SysmanDriverHandleImp;
    UNRECOVERABLE_IF(nullptr == driverHandle);

    ze_result_t res = driverHandle->initialize(executionEnvironment);
    if (res != ZE_RESULT_SUCCESS) {
        delete driverHandle;
        driverHandle = nullptr;
        *returnValue = res;
        return nullptr;
    }

    globalSysmanDriver = driverHandle;
    *returnValue = res;
    return driverHandle;
}

ze_result_t SysmanDriverHandleImp::getDevice(uint32_t *pCount, zes_device_handle_t *phDevices) {
    if (*pCount == 0) {
        *pCount = this->numDevices;
        return ZE_RESULT_SUCCESS;
    }

    if (*pCount > this->numDevices) {
        *pCount = this->numDevices;
    }

    if (phDevices == nullptr) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }

    for (uint32_t i = 0; i < *pCount; i++) {
        phDevices[i] = this->sysmanDevices[i];
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t SysmanDriverHandleImp::getDeviceByUuid(zes_uuid_t uuid, zes_device_handle_t *phDevice, ze_bool_t *onSubdevice, uint32_t *subdeviceId) {
    for (uint32_t index = 0; index < this->numDevices; index++) {
        ze_bool_t deviceFound = this->sysmanDevices[index]->getDeviceInfoByUuid(uuid, onSubdevice, subdeviceId);
        if (deviceFound) {
            *phDevice = this->sysmanDevices[index];
            return ZE_RESULT_SUCCESS;
        }
    }
    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

ze_result_t SysmanDriverHandleImp::getExtensionProperties(uint32_t *pCount, zes_driver_extension_properties_t *pExtensionProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanDriverHandleImp::sysmanEventsListen(uint32_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) {
    if (pOsSysmanDriver == nullptr) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "%s", "Os Sysman Driver Not initialized\n");
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pOsSysmanDriver->eventsListen(timeout, count, phDevices, pNumDeviceEvents, pEvents);
}

ze_result_t SysmanDriverHandleImp::sysmanEventsListenEx(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) {
    if (pOsSysmanDriver == nullptr) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "%s", "Os Sysman Driver Not initialized\n");
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pOsSysmanDriver->eventsListen(timeout, count, phDevices, pNumDeviceEvents, pEvents);
};

SysmanDriverHandleImp::~SysmanDriverHandleImp() {
    for (auto &device : this->sysmanDevices) {
        delete device;
    }
    this->sysmanDevices.clear();

    if (pOsSysmanDriver != nullptr) {
        delete pOsSysmanDriver;
        pOsSysmanDriver = nullptr;
    }
}

} // namespace Sysman
} // namespace L0
