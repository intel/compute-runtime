/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/driver/sysman_driver_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/driver/mock_sysman_driver_handle.h"
#include "level_zero/sysman/test/unit_tests/sources/driver/mock_sysman_os_driver.h"

namespace L0 {
namespace Sysman {
namespace ult {

struct OsAgnosticMockSysmanDriver : public ::L0::Sysman::SysmanDriverImp {
    OsAgnosticMockSysmanDriver() {
        previousDriver = driver;
        driver = this;
        pMockOsDriver = std::make_unique<MockOsDriver>();
    }
    ~OsAgnosticMockSysmanDriver() override {
        driver = previousDriver;
    }

    // Inheritance-based mock: override the virtual discovery seam so no OS syscalls are made.
    HwDeviceIds discoverHwDevices(NEO::ExecutionEnvironment &) override {
        return {};
    }

    // Inject a MockSysmanDriverHandleImp instead of a real SysmanDriverHandleImp when
    // initialize() enters deferred-discovery mode.
    SysmanDriverHandle *createDeferredHandle(NEO::ExecutionEnvironment &env, ze_result_t *result) override {
        auto *handle = new MockSysmanDriverHandleImp();
        handle->initializeDeferredMode(&env);
        globalSysmanDriver = handle;
        *result = ZE_RESULT_SUCCESS;
        return handle;
    }

    // Inject MockOsDriver so tests can configure survivability behaviour without OS calls.
    // Configure pMockOsDriver members before calling initialize().
    std::unique_ptr<OsDriver> createOsDriver() override {
        return std::move(pMockOsDriver);
    }

    ze_result_t driverInit(zes_init_flags_t flags) override {
        initCalledCount++;
        if (useBaseDriverInit) {
            return SysmanDriverImp::driverInit(flags);
        }
        return ZE_RESULT_SUCCESS;
    }

    void initialize(ze_result_t *result, zes_init_flags_t flags) override {
        if (useBaseInit) {
            SysmanDriverImp::initialize(result, flags);
            return;
        }
        if (sysmanInitFail) {
            *result = ZE_RESULT_ERROR_UNINITIALIZED;
            return;
        }
        *result = ZE_RESULT_SUCCESS;
    }

    ::L0::Sysman::SysmanDriver *previousDriver = nullptr;
    uint32_t initCalledCount = 0;
    bool useBaseDriverInit = false;
    bool sysmanInitFail = false;
    bool useBaseInit = true;

    std::unique_ptr<MockOsDriver> pMockOsDriver;
};

// Exposes createDeferredHandle so tests can call the real SysmanDriverImp implementation.
struct SysmanDriverImpWithRealDeferred : public SysmanDriverImp {
    HwDeviceIds discoverHwDevices(NEO::ExecutionEnvironment &) override { return {}; }
    std::unique_ptr<OsDriver> createOsDriver() override { return std::make_unique<MockOsDriver>(); }
    using SysmanDriverImp::createDeferredHandle;
};

struct MockSysmanDriverWithDevices : public OsAgnosticMockSysmanDriver {
    SysmanDriverHandle *createDeferredHandle(NEO::ExecutionEnvironment &env, ze_result_t *result) override {
        struct HandleWithDevices : public MockSysmanDriverHandleImp {
            ze_result_t performDeferredDiscovery() override {
                deferredDiscoveryCallCount++;
                if (savedExecutionEnvironment != nullptr) {
                    savedExecutionEnvironment->decRefInternal();
                    savedExecutionEnvironment = nullptr;
                }
                devicesDiscovered = true;
                numDevices = 2u;
                return ZE_RESULT_SUCCESS;
            }
        };
        auto *handle = new HandleWithDevices();
        handle->initializeDeferredMode(&env);
        globalSysmanDriver = handle;
        *result = ZE_RESULT_SUCCESS;
        return handle;
    }
};

struct MockSysmanDriverWithFailingDiscovery : public OsAgnosticMockSysmanDriver {
    SysmanDriverHandle *createDeferredHandle(NEO::ExecutionEnvironment &env, ze_result_t *result) override {
        struct HandleWithFailingDiscovery : public MockSysmanDriverHandleImp {
            ze_result_t performDeferredDiscovery() override {
                deferredDiscoveryCallCount++;
                if (savedExecutionEnvironment != nullptr) {
                    savedExecutionEnvironment->decRefInternal();
                    savedExecutionEnvironment = nullptr;
                }
                devicesDiscovered = true;
                return ZE_RESULT_ERROR_UNINITIALIZED;
            }
        };
        auto *handle = new HandleWithFailingDiscovery();
        handle->initializeDeferredMode(&env);
        globalSysmanDriver = handle;
        *result = ZE_RESULT_SUCCESS;
        return handle;
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
