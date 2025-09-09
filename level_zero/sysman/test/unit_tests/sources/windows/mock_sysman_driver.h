/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/default_hw_info.h"

#include "level_zero/sysman/source/driver/sysman_driver.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"
#include "level_zero/sysman/source/driver/sysman_driver_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_wddm.h"

#include "gtest/gtest.h"

namespace L0 {
namespace Sysman {
namespace ult {

struct MockSysmanDriver : public ::L0::Sysman::SysmanDriverImp {
    MockSysmanDriver() {
        previousDriver = driver;
        driver = this;
    }
    ~MockSysmanDriver() override {
        driver = previousDriver;
    }

    ze_result_t driverInit() override {
        initCalledCount++;
        if (useBaseDriverInit) {
            return SysmanDriverImp::driverInit();
        }
        return ZE_RESULT_SUCCESS;
    }

    void initialize(ze_result_t *result) override {
        if (useBaseInit) {
            SysmanDriverImp::initialize(result);
            return;
        }
        if (sysmanInitFail) {
            *result = ZE_RESULT_ERROR_UNINITIALIZED;
            return;
        }
        *result = ZE_RESULT_SUCCESS;
        return;
    }

    ::L0::Sysman::SysmanDriver *previousDriver = nullptr;
    uint32_t initCalledCount = 0;
    bool useBaseDriverInit = false;
    bool sysmanInitFail = false;
    bool useBaseInit = true;
};

class SysmanDriverHandleTest : public ::testing::Test {
  public:
    void SetUp() override {

        execEnv = new NEO::ExecutionEnvironment();
        execEnv->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < execEnv->rootDeviceEnvironments.size(); i++) {
            execEnv->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            execEnv->rootDeviceEnvironments[i]->osInterface = std::make_unique<NEO::OSInterface>();
            execEnv->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::make_unique<SysmanMockWddm>(*execEnv->rootDeviceEnvironments[i]));
        }

        driverHandle = std::make_unique<L0::Sysman::SysmanDriverHandleImp>();
        driverHandle->initialize(*execEnv);
        L0::Sysman::globalSysmanDriver = driverHandle.get();

        L0::Sysman::sysmanOnlyInit = true;
    }

    void TearDown() override {
        L0::Sysman::sysmanOnlyInit = false;
        L0::Sysman::globalSysmanDriver = nullptr;
    }

    NEO::ExecutionEnvironment *execEnv = nullptr;
    std::unique_ptr<L0::Sysman::SysmanDriverHandleImp> driverHandle;
    const uint32_t numRootDevices = 1u;
};

} // namespace ult
} // namespace Sysman
} // namespace L0
