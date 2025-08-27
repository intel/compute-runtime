/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/driver.h"

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/pin/pin.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/tools/source/metrics/metric.h"

#include "driver_version.h"

#include <memory>
#include <mutex>
#include <thread>

namespace L0 {

std::vector<_ze_driver_handle_t *> *globalDriverHandles;
bool levelZeroDriverInitialized = false;

void DriverImp::initialize(ze_result_t *result) {
    *result = ZE_RESULT_ERROR_UNINITIALIZED;
    pid = NEO::SysCalls::getCurrentProcessId();

    NEO::EnvironmentVariableReader envReader;
    L0EnvVariables envVariables = {};
    envVariables.affinityMask =
        envReader.getSetting("ZE_AFFINITY_MASK", std::string(""));
    envVariables.programDebugging =
        envReader.getSetting("ZET_ENABLE_PROGRAM_DEBUGGING", 0);
    envVariables.metrics =
        envReader.getSetting("ZET_ENABLE_METRICS", false);
    envVariables.pin =
        envReader.getSetting("ZET_ENABLE_PROGRAM_INSTRUMENTATION", false);
    envVariables.sysman =
        envReader.getSetting("ZES_ENABLE_SYSMAN", false);
    envVariables.pciIdDeviceOrder =
        envReader.getSetting("ZE_ENABLE_PCI_ID_DEVICE_ORDER", false);
    envVariables.fp64Emulation =
        envReader.getSetting("NEO_FP64_EMULATION", false);

    bool oneApiPvcWa = envReader.getSetting("ONEAPI_PVC_SEND_WAR_WA", true);

    auto executionEnvironment = new NEO::ExecutionEnvironment();
    UNRECOVERABLE_IF(nullptr == executionEnvironment);

    if (!NEO::debugManager.flags.ExperimentalEnableL0DebuggerForOpenCL.get()) {
        const auto dbgMode = NEO::getDebuggingMode(envVariables.programDebugging);
        executionEnvironment->setDebuggingMode(dbgMode);
    }

    if (envVariables.fp64Emulation) {
        executionEnvironment->setFP64EmulationEnabled();
    }

    executionEnvironment->setMetricsEnabled(envVariables.metrics);
    executionEnvironment->setOneApiPvcWaEnv(oneApiPvcWa);

    executionEnvironment->incRefInternal();
    auto neoDevices = NEO::DeviceFactory::createDevices(*executionEnvironment);
    bool isDevicePermissionError = executionEnvironment->isDevicePermissionError();
    executionEnvironment->decRefInternal();
    if (neoDevices.empty()) {
        if (isDevicePermissionError) {
            *result = ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
        }
        return;
    }

    auto deviceGroups = NEO::Device::groupDevices(std::move(neoDevices));
    for (auto &devices : deviceGroups) {
        auto driverHandle = DriverHandle::create(std::move(devices), envVariables, result);
        if (driverHandle) {
            globalDriverHandles->push_back(driverHandle);

            auto &devicesToExpose = static_cast<DriverHandleImp *>(driverHandle)->devicesToExpose;
            std::vector<NEO::Device *> neoDeviceToExpose;
            neoDeviceToExpose.reserve(devicesToExpose.size());
            for (auto deviceToExpose : devicesToExpose) {
                neoDeviceToExpose.push_back(Device::fromHandle(deviceToExpose)->getNEODevice());
            }

            NEO::Device::initializePeerAccessForDevices(DeviceImp::queryPeerAccess, neoDeviceToExpose);
        }
    }

    if (globalDriverHandles->size() > 0) {
        *result = ZE_RESULT_SUCCESS;

        if (envVariables.metrics) {
            *result = MetricDeviceContext::enableMetricApi();
        }
        if (*result != ZE_RESULT_SUCCESS) {
            for (auto &driverHandle : *globalDriverHandles) {
                delete static_cast<BaseDriver *>(driverHandle);
            }
            globalDriverHandles->clear();
        } else if (envVariables.pin) {
            std::unique_lock<std::mutex> mtx{this->gtpinInitMtx};
            this->gtPinInitializationNeeded = true;
        }
    }
}

ze_result_t DriverImp::initStatus(ZE_RESULT_ERROR_UNINITIALIZED);

ze_result_t DriverImp::driverInit() {
    std::call_once(initDriverOnce, [this]() {
        ze_result_t result;
        this->initialize(&result);
        initStatus = result;
    });
    return initStatus;
}

ze_result_t DriverImp::driverHandleGet(uint32_t *pCount, ze_driver_handle_t *phDriverHandles) {
    // Only attempt to Init GtPin when driverHandleGet is called requesting handles.
    if (phDriverHandles != nullptr && *pCount > 0) {
        Driver::get()->tryInitGtpin();
    }
    auto driverCount = static_cast<uint32_t>(globalDriverHandles->size());
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
        phDriverHandles[i] = (*globalDriverHandles)[i];
    }

    return ZE_RESULT_SUCCESS;
}

void DriverImp::tryInitGtpin() {
    if (!this->gtPinInitializationNeeded) {
        return;
    }
    std::unique_lock<std::mutex> mtx{this->gtpinInitMtx};
    if (this->gtPinInitializationNeeded) {
        this->gtPinInitializationNeeded = false;
        std::string gtpinFuncName{"OpenGTPin"};
        NEO::PinContext::init(gtpinFuncName);
    }
}

static DriverImp driverImp;
Driver *Driver::driver = &driverImp;
std::mutex driverInitMutex;

ze_result_t initDriver() {
    auto pid = NEO::SysCalls::getCurrentProcessId();

    ze_result_t result = Driver::get()->driverInit();

    if (Driver::get()->getPid() != pid) {
        std::lock_guard<std::mutex> lock(driverInitMutex);

        if (Driver::get()->getPid() != pid) {
            Driver::get()->initialize(&result);
        }
    }

    if (result == ZE_RESULT_SUCCESS) {
        L0::levelZeroDriverInitialized = true;
    } else {
        L0::levelZeroDriverInitialized = false;
    }
    return result;
}

ze_result_t init(ze_init_flags_t flags) {
    if (flags && !(flags & ZE_INIT_FLAG_GPU_ONLY)) {
        L0::levelZeroDriverInitialized = false;
        return ZE_RESULT_ERROR_UNINITIALIZED;
    } else {
        return initDriver();
    }
}

ze_result_t initDrivers(uint32_t *pCount, ze_driver_handle_t *phDrivers, ze_init_driver_type_desc_t *desc) {
    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    if (desc->flags & ZE_INIT_DRIVER_TYPE_FLAG_GPU) {
        result = initDriver();
        if (result == ZE_RESULT_SUCCESS) {
            result = Driver::get()->driverHandleGet(pCount, phDrivers);
        }
    }
    return result;
}

} // namespace L0
