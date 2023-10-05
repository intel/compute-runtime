/*
 * Copyright (C) 2020-2023 Intel Corporation
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
#include "shared/source/pin/pin.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/tools/source/metrics/metric.h"

#include <memory>
#include <thread>

namespace L0 {

_ze_driver_handle_t *GlobalDriverHandle;
bool LevelZeroDriverInitialized = false;
uint32_t driverCount = 1;

void DriverImp::initialize(ze_result_t *result) {
    *result = ZE_RESULT_ERROR_UNINITIALIZED;

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
    envVariables.deviceHierarchyMode =
        envReader.getSetting("ZE_FLAT_DEVICE_HIERARCHY", std::string(NEO::deviceHierarchyUnk));

    auto executionEnvironment = new NEO::ExecutionEnvironment();
    UNRECOVERABLE_IF(nullptr == executionEnvironment);

    if (!NEO::DebugManager.flags.ExperimentalEnableL0DebuggerForOpenCL.get()) {
        const auto dbgMode = NEO::getDebuggingMode(envVariables.programDebugging);
        executionEnvironment->setDebuggingMode(dbgMode);
    }

    if (envVariables.fp64Emulation) {
        executionEnvironment->setFP64EmulationEnabled();
    }

    executionEnvironment->setMetricsEnabled(envVariables.metrics);

    executionEnvironment->incRefInternal();
    auto neoDevices = NEO::DeviceFactory::createDevices(*executionEnvironment);
    executionEnvironment->decRefInternal();
    if (!neoDevices.empty()) {
        GlobalDriverHandle = DriverHandle::create(std::move(neoDevices), envVariables, result);
        if (GlobalDriverHandle != nullptr) {
            *result = ZE_RESULT_SUCCESS;

            if (envVariables.metrics) {
                *result = MetricDeviceContext::enableMetricApi();
                if (*result != ZE_RESULT_SUCCESS) {
                    delete GlobalDriver;
                    GlobalDriverHandle = nullptr;
                    GlobalDriver = nullptr;
                }
            }

            if ((*result == ZE_RESULT_SUCCESS) && envVariables.pin) {
                std::string gtpinFuncName{"OpenGTPin"};
                if (false == NEO::PinContext::init(gtpinFuncName)) {
                    *result = ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
                    delete GlobalDriver;
                    GlobalDriverHandle = nullptr;
                    GlobalDriver = nullptr;
                }
            }
        }
    }
}

ze_result_t DriverImp::initStatus(ZE_RESULT_ERROR_UNINITIALIZED);

ze_result_t DriverImp::driverInit(ze_init_flags_t flags) {
    std::call_once(initDriverOnce, [this]() {
        ze_result_t result;
        this->initialize(&result);
        initStatus = result;
    });
    return initStatus;
}

ze_result_t driverHandleGet(uint32_t *pCount, ze_driver_handle_t *phDriverHandles) {
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
        phDriverHandles[i] = GlobalDriverHandle;
    }

    return ZE_RESULT_SUCCESS;
}

static DriverImp driverImp;
Driver *Driver::driver = &driverImp;

ze_result_t init(ze_init_flags_t flags) {
    if (flags && !(flags & ZE_INIT_FLAG_GPU_ONLY)) {
        L0::LevelZeroDriverInitialized = false;
        return ZE_RESULT_ERROR_UNINITIALIZED;
    } else {
        ze_result_t result = Driver::get()->driverInit(flags);
        if (result == ZE_RESULT_SUCCESS) {
            L0::LevelZeroDriverInitialized = true;
        } else {
            L0::LevelZeroDriverInitialized = false;
        }
        return result;
    }
}
} // namespace L0
