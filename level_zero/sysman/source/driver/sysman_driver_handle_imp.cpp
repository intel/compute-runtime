/*
 * Copyright (C) 2023-2025 Intel Corporation
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
#include "level_zero/zes_intel_gpu_sysman.h"

#include <cstring>
#include <vector>

namespace L0 {
namespace Sysman {

struct SysmanDriverHandleImp *globalSysmanDriver;

SysmanDriverHandleImp::SysmanDriverHandleImp() = default;

void *getSysmanExtensionFunctionAddress(const std::string &functionName) {
#define RETURN_FUNC_PTR_IF_EXIST(name)    \
    {                                     \
        if (functionName == #name) {      \
            void *ret = ((void *)(name)); \
            return ret;                   \
        }                                 \
    }

    RETURN_FUNC_PTR_IF_EXIST(zesIntelDevicePciLinkSpeedUpdateExp);

#undef RETURN_FUNC_PTR_IF_EXIST

    return nullptr;
}

void SysmanDriverHandleImp::updateUuidMap(SysmanDevice *sysmanDevice) {
    std::vector<std::string> uuidArr;
    sysmanDevice->getDeviceUuids(uuidArr);
    for (auto &uuid : uuidArr) {
        uuidDeviceMap[uuid] = sysmanDevice;
    }
    return;
}

SysmanDevice *SysmanDriverHandleImp::findSysmanDeviceFromCoreToSysmanDeviceMap(ze_device_handle_t handle) {
    auto iterator = coreToSysmanDeviceMap.find(handle);
    if (iterator != coreToSysmanDeviceMap.end()) {
        SysmanDevice *sysmanDevice = iterator->second;
        return sysmanDevice;
    }
    return nullptr;
}

SysmanDriverHandle *SysmanDriverHandleImp::findSysmanDriverHandleFromCoreToSysmanDriverMap(ze_driver_handle_t handle) {
    auto iterator = coreToSysmanDriverMap.find(handle);
    if (iterator != coreToSysmanDriverMap.end()) {
        SysmanDriverHandle *sysmanDriverHandle = iterator->second;
        return sysmanDriverHandle;
    }
    return nullptr;
}

SysmanDevice *SysmanDriverHandleImp::getSysmanDeviceFromCoreDeviceHandle(ze_device_handle_t hDevice) {
    const std::lock_guard<std::mutex> lock(this->coreToSysmanDeviceMapLock);
    if (hDevice == nullptr) {
        return nullptr;
    }

    SysmanDevice *sysmanDevice = findSysmanDeviceFromCoreToSysmanDeviceMap(hDevice);
    if (sysmanDevice != nullptr) {
        return sysmanDevice;
    }

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    Device::fromHandle(hDevice)->getProperties(&deviceProperties);
    std::string uuid(reinterpret_cast<char const *>(deviceProperties.uuid.id));
    auto it = uuidDeviceMap.find(uuid);
    if (it == uuidDeviceMap.end()) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "SysmanDriverHandleImp::getSysmanDeviceFromCoreDeviceHandle() - sysman device handle equivalent to core device handle not found!! %s\n", "");
        return nullptr;
    }
    sysmanDevice = it->second;
    coreToSysmanDeviceMap[hDevice] = sysmanDevice;

    return sysmanDevice;
}

ze_result_t SysmanDriverHandleImp::initialize(NEO::ExecutionEnvironment &executionEnvironment) {
    for (uint32_t rootDeviceIndex = 0u; rootDeviceIndex < executionEnvironment.rootDeviceEnvironments.size(); rootDeviceIndex++) {
        auto pSysmanDevice = SysmanDevice::create(executionEnvironment, rootDeviceIndex);
        if (pSysmanDevice != nullptr) {
            this->sysmanDevices.push_back(pSysmanDevice);
            updateUuidMap(pSysmanDevice);
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
    *pfunc = getSysmanExtensionFunctionAddress(pFuncName);
    if (*pfunc) {
        return ZE_RESULT_SUCCESS;
    }
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

SysmanDriverHandle *SysmanDriverHandleImp::getSysmanDriverHandleFromCoreDriverHandle(ze_driver_handle_t handle) {
    const std::lock_guard<std::mutex> lock(this->coreToSysmanDriverMapLock);
    // search coreSysmanDriverMap for sysman driver handle
    auto sysmanDriverHandle = findSysmanDriverHandleFromCoreToSysmanDriverMap(handle);
    if (sysmanDriverHandle != nullptr) {
        return sysmanDriverHandle;
    }
    // Get Device List from Core Driver
    uint32_t deviceCount = 0;
    ze_result_t result = DriverHandle::fromHandle(handle)->getDevice(&deviceCount, nullptr);
    if (result != ZE_RESULT_SUCCESS || deviceCount == 0) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "failed to get device count for core driver handle\n");
        return nullptr;
    }

    std::vector<ze_device_handle_t> coreDevices(deviceCount);
    DriverHandle::fromHandle(handle)->getDevice(&deviceCount, coreDevices.data());

    // For first Device, Get Properties (UUID)
    zes_uuid_t deviceUuid = {};
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    Device::fromHandle(coreDevices[0])->getProperties(&deviceProperties);
    std::memcpy(deviceUuid.id, deviceProperties.uuid.id, sizeof(deviceUuid.id));

    // Enumerate Sysman Drivers
    uint32_t sysmanDriverCount = 0;
    result = L0::Sysman::driverHandleGet(&sysmanDriverCount, nullptr);
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "failed to get sysman driver count\n");
        return nullptr;
    }

    std::vector<zes_driver_handle_t> sysmanDrivers(sysmanDriverCount);
    L0::Sysman::driverHandleGet(&sysmanDriverCount, sysmanDrivers.data());

    // Check for Matching Sysman Device
    for (auto sysmanDriver : sysmanDrivers) {
        zes_device_handle_t sysmanDevice = nullptr;
        ze_bool_t onSubdevice = false;
        uint32_t subdeviceId = 0;

        // Check if this sysman driver has a device with matching UUID
        SysmanDriverHandle::fromHandle(sysmanDriver)->getDeviceByUuid(deviceUuid, &sysmanDevice, &onSubdevice, &subdeviceId);
        if (sysmanDevice != nullptr) {
            coreToSysmanDriverMap[handle] = static_cast<SysmanDriverHandle *>(sysmanDriver);
            return static_cast<SysmanDriverHandle *>(sysmanDriver);
        }
    }

    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "No matching sysman driver found for core driver\n");
    return nullptr;
}

SysmanDriverHandle *SysmanDriverHandle::fromHandle(zes_driver_handle_t handle) {
    SysmanDriverHandle *sysmanDriverHandle = nullptr;
    if (handle != nullptr) {
        // if driver handle is same as sysman driver handle
        if (handle == globalSysmanDriver) {
            sysmanDriverHandle = static_cast<SysmanDriverHandle *>(globalSysmanDriver);
        } else {
            sysmanDriverHandle = globalSysmanDriver->getSysmanDriverHandleFromCoreDriverHandle(handle);
        }
    }

    return sysmanDriverHandle;
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
