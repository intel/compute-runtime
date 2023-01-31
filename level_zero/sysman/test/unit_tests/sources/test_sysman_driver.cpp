/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "level_zero/sysman/source/sysman_device.h"
#include "level_zero/sysman/source/sysman_driver_handle.h"
#include "level_zero/sysman/source/sysman_driver_handle_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/mock_sysman_driver.h"

#include "gtest/gtest.h"

#include <bitset>

namespace L0 {
namespace ult {
namespace Sysman {

TEST(zesInit, whenCallingZesInitThenInitializeOnDriverIsCalled) {
    MockSysmanDriver driver;

    auto result = zesInit(ZE_INIT_FLAG_GPU_ONLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, driver.initCalledCount);
}

TEST(zesInit, whenCallingZesInitWithNoFlagsThenInitializeOnDriverIsCalled) {
    MockSysmanDriver driver;

    auto result = zesInit(0);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, driver.initCalledCount);
}

TEST(zesInit, whenCallingZesInitWithoutGpuOnlyFlagThenInitializeOnDriverIsNotCalled) {
    MockSysmanDriver driver;

    auto result = zesInit(ZE_INIT_FLAG_VPU_ONLY);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
    EXPECT_EQ(0u, driver.initCalledCount);
}

struct SysmanDriverTestMultipleFamilySupport : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < numRootDevices; i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            if (i < numSupportedRootDevices) {
                executionEnvironment->rootDeviceEnvironments[i]->getMutableHardwareInfo()->capabilityTable.levelZeroSupported = true;
            } else {
                executionEnvironment->rootDeviceEnvironments[i]->getMutableHardwareInfo()->capabilityTable.levelZeroSupported = false;
            }
        }
    }

    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    const uint32_t numRootDevices = 3u;
    const uint32_t numSubDevices = 2u;
    const uint32_t numSupportedRootDevices = 2u;
};

TEST_F(SysmanDriverTestMultipleFamilySupport, whenInitializingSysmanDriverWithArrayOfDevicesThenDriverIsInitializedOnlyWithThoseSupported) {
    ze_result_t returnValue;

    auto driverHandle = L0::Sysman::SysmanDriverHandle::create(*executionEnvironment, &returnValue);
    EXPECT_NE(nullptr, driverHandle);

    L0::Sysman::SysmanDriverHandleImp *driverHandleImp = reinterpret_cast<L0::Sysman::SysmanDriverHandleImp *>(driverHandle);
    EXPECT_EQ(numSupportedRootDevices, driverHandleImp->sysmanDevices.size());

    for (auto d : driverHandleImp->sysmanDevices) {
        EXPECT_TRUE(d->getHardwareInfo().capabilityTable.levelZeroSupported);
    }

    delete driverHandle;
    L0::Sysman::GlobalDriver = nullptr;
}

struct SysmanDriverTestMultipleFamilyNoSupport : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
        }

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->getMutableHardwareInfo()->capabilityTable.levelZeroSupported = false;
        }
    }

    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    const uint32_t numRootDevices = 3u;
};

TEST_F(SysmanDriverTestMultipleFamilyNoSupport, whenInitializingSysmanDriverWithArrayOfNotSupportedDevicesThenDriverIsNull) {
    ze_result_t returnValue;
    this->executionEnvironment->incRefInternal();
    auto driverHandle = L0::Sysman::SysmanDriverHandle::create(*executionEnvironment, &returnValue);
    EXPECT_EQ(nullptr, driverHandle);
    EXPECT_EQ(returnValue, ZE_RESULT_ERROR_UNINITIALIZED);
    this->executionEnvironment->decRefInternal();
}

struct SysmanDriverHandleTest : public ::testing::Test {
    void SetUp() override {

        ze_result_t returnValue;
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        hwInfo.capabilityTable.levelZeroSupported = true;

        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            // executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        }

        driverHandle = L0::Sysman::SysmanDriverHandle::create(*executionEnvironment, &returnValue);
        L0::Sysman::GlobalDriverHandle = driverHandle;
    }
    void TearDown() override {
        delete driverHandle;
        L0::Sysman::GlobalDriver = nullptr;
        L0::Sysman::GlobalDriverHandle = nullptr;
    }
    L0::Sysman::SysmanDriverHandle *driverHandle;
    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    const uint32_t numRootDevices = 3u;
};

TEST_F(SysmanDriverHandleTest, givenInitializedDriverWhenZesDriverGetIsCalledThenDriverHandleCountIsObtained) {
    uint32_t count = 0;
    auto result = zesDriverGet(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1U, count);
}

TEST_F(SysmanDriverHandleTest,
       givenInitializedDriverWhenZesDriverGetIsCalledWithGreaterThanCountAvailableThenCorrectCountIsReturned) {
    uint32_t count = 0;
    ze_result_t result = zesDriverGet(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1U, count);

    count++;
    ze_driver_handle_t driverHandle = {};
    result = zesDriverGet(&count, &driverHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1U, count);
    EXPECT_NE(nullptr, driverHandle);
}

TEST_F(SysmanDriverHandleTest,
       givenInitializedDriverWhenZesDriverGetIsCalledWithGreaterThanZeroCountAndNullDriverHandleThenInvalidNullPointerIsReturned) {
    uint32_t count = 0;
    ze_result_t result = zesDriverGet(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1U, count);

    result = zesDriverGet(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, result);
}

TEST_F(SysmanDriverHandleTest, givenInitializedDriverWhenZesDriverGetIsCalledThenDriverHandleIsObtained) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t count = 0;
    result = zesDriverGet(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1U, count);

    ze_driver_handle_t *phDriverHandles = new ze_driver_handle_t[count];

    result = zesDriverGet(&count, phDriverHandles);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(driverHandle->toHandle(), phDriverHandles[0]);
    delete[] phDriverHandles;
}

TEST_F(SysmanDriverHandleTest, givenInitializedDriverWhenZesDriverGetIsCalledThenGlobalDriverHandleIsObtained) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    uint32_t count = 1;
    zes_driver_handle_t hDriverHandle = reinterpret_cast<zes_driver_handle_t>(&hDriverHandle);

    result = zesDriverGet(&count, &hDriverHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, hDriverHandle);
    EXPECT_EQ(hDriverHandle, L0::Sysman::GlobalDriver);
}

TEST_F(SysmanDriverHandleTest, givenInitializedDriverWhenGetDeviceIsCalledThenOneDeviceIsObtained) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t count = 1;

    ze_device_handle_t device;
    result = driverHandle->getDevice(&count, &device);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, &device);
}

TEST_F(SysmanDriverHandleTest, whenQueryingForDevicesWithCountGreaterThanZeroAndNullDevicePointerThenNullHandleIsReturned) {
    uint32_t count = 1;
    ze_result_t result = driverHandle->getDevice(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_HANDLE, result);
}

} // namespace Sysman
} // namespace ult
} // namespace L0
