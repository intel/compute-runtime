/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/sysman/source/device/sysman_hw_device_id.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"
#include "level_zero/sysman/source/driver/sysman_driver_imp.h"
#include "level_zero/sysman/source/driver/sysman_os_driver.h"

#include <cstring>
#include <vector>

namespace L0 {
namespace Sysman {
_ze_driver_handle_t *globalSysmanDriverHandle = nullptr;
uint32_t driverCount = 0;
bool sysmanOnlyInit = false;
uint32_t SysmanDriverImp::discoverAndInitializeDevices(NEO::ExecutionEnvironment &executionEnvironment, HwDeviceIds &hwDeviceIds,
                                                       const char *errorPrefix) {
    if (hwDeviceIds.empty()) {
        return 0;
    }
    executionEnvironment.prepareRootDeviceEnvironments(static_cast<uint32_t>(hwDeviceIds.size()));
    uint32_t rootDeviceIndex = 0u;
    for (auto &hwDeviceId : hwDeviceIds) {

        auto sysmanHwDeviceId = createSysmanHwDeviceId(hwDeviceId);
        auto initStatus = sysmanHwDeviceId != nullptr &&
                          executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->initOsInterface(std::move(sysmanHwDeviceId), rootDeviceIndex);

        if (!initStatus) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                         "%s : initialization failed for device : %d\n", errorPrefix, rootDeviceIndex);
            continue;
        }
        rootDeviceIndex++;
    }

    executionEnvironment.rootDeviceEnvironments.resize(rootDeviceIndex);
    return rootDeviceIndex;
}

void SysmanDriverImp::initialize(ze_result_t *result, zes_init_flags_t flags) {
    *result = ZE_RESULT_ERROR_UNINITIALIZED;

    // Extract experimental flag (bit 16)
    bool allowDeferredDiscovery = (flags & ZES_INTEL_INIT_FLAG_EXP_NO_GPUS) != 0;

    auto executionEnvironment = new NEO::ExecutionEnvironment();
    UNRECOVERABLE_IF(nullptr == executionEnvironment);
    executionEnvironment->incRefInternal();

    HwDeviceIds hwDeviceIds = discoverHwDevices(*executionEnvironment);
    auto rootDeviceIndex = discoverAndInitializeDevices(*executionEnvironment, hwDeviceIds, "SysmanDriverImp::initialize");

    if (rootDeviceIndex > 0) {
        globalSysmanDriverHandle = SysmanDriverHandle::create(*executionEnvironment, result);
        driverCount = 1;
    } else {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "%s\n", "No devices successfully initialized");
        *result = ZE_RESULT_ERROR_UNINITIALIZED;
    }

    // Check survivability before deciding on deferred mode
    std::unique_ptr<OsDriver> pOsDriverInterface = createOsDriver();
    if (globalSysmanDriverHandle != nullptr) {
        pOsDriverInterface->initSurvivabilityDevices(globalSysmanDriverHandle, result);
    } else {
        globalSysmanDriverHandle = pOsDriverInterface->initSurvivabilityDevicesWithDriver(result, &driverCount);
    }

    // Check if deferred discovery mode is possible
    if (globalSysmanDriverHandle == nullptr && allowDeferredDiscovery) {
        // Deferred discovery mode
        executionEnvironment->incRefInternal(); // Extra ref for deferred mode
        globalSysmanDriverHandle = createDeferredHandle(*executionEnvironment, result);
        driverCount = 1;
        *result = ZE_RESULT_SUCCESS;
    } else if (globalSysmanDriverHandle == nullptr) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "%s\n", "No devices found");
        *result = ZE_RESULT_ERROR_UNINITIALIZED;
    }

    executionEnvironment->decRefInternal();
}

SysmanDriverImp::HwDeviceIds SysmanDriverImp::discoverHwDevices(NEO::ExecutionEnvironment &executionEnvironment) {
    return NEO::OSInterface::discoverDevices(executionEnvironment);
}

SysmanDriverHandle *SysmanDriverImp::createDeferredHandle(NEO::ExecutionEnvironment &executionEnvironment, ze_result_t *result) {
    return SysmanDriverHandle::createDeferred(executionEnvironment, result);
}

std::unique_ptr<OsDriver> SysmanDriverImp::createOsDriver() {
    return OsDriver::create();
}

ze_result_t SysmanDriverImp::initStatus(ZE_RESULT_ERROR_UNINITIALIZED);

ze_result_t SysmanDriverImp::driverInit(zes_init_flags_t flags) {
    std::call_once(initDriverOnce, [this, flags]() {
        ze_result_t result;
        this->initialize(&result, flags);
        initStatus = result;
        if (result == ZE_RESULT_SUCCESS) {
            sysmanOnlyInit = true;
        }
    });
    return initStatus;
}

ze_result_t driverHandleGet(uint32_t *pCount, zes_driver_handle_t *phDriverHandles) {
    if (driverCount == 0) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    if (*pCount == 0) {
        *pCount = driverCount;
        return ZE_RESULT_SUCCESS;
    }

    if (*pCount > driverCount) {
        *pCount = driverCount;
    }

    if (phDriverHandles == nullptr) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    for (uint32_t i = 0; i < *pCount; i++) {
        phDriverHandles[i] = globalSysmanDriverHandle;
    }

    return ZE_RESULT_SUCCESS;
}

static SysmanDriverImp driverImp;
SysmanDriver *SysmanDriver::driver = &driverImp;

ze_result_t init(zes_init_flags_t flags) {
    constexpr auto allowedFlags =
        ZE_INIT_FLAG_GPU_ONLY | ZES_INTEL_INIT_FLAG_EXP_NO_GPUS;

    if (flags & ~allowedFlags) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    return SysmanDriver::get()->driverInit(flags);
}

} // namespace Sysman
} // namespace L0
