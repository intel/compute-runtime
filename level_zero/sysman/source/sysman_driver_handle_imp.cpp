/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/sysman_driver_handle_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/sysman/source/sysman_device.h"
#include "level_zero/sysman/source/sysman_driver.h"

#include <cstring>
#include <vector>

namespace L0 {
namespace Sysman {

struct SysmanDriverHandleImp *GlobalSysmanDriver;

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
    this->numDevices = static_cast<uint32_t>(this->sysmanDevices.size());
    return ZE_RESULT_SUCCESS;
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

    GlobalSysmanDriver = driverHandle;
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

SysmanDriverHandleImp::~SysmanDriverHandleImp() {
    for (auto &device : this->sysmanDevices) {
        delete device;
    }
    this->sysmanDevices.clear();
}

} // namespace Sysman
} // namespace L0
