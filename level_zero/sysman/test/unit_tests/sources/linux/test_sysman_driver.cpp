/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/sysman/source/device/sysman_device.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_driver.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_drm.h"

#include "gtest/gtest.h"

#include <bitset>
#include <cstring>

namespace L0 {
namespace Sysman {
namespace ult {

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

TEST(zesInit, whenCallingZesInitWhenDriverInitFailsThenUninitializedIsReturned) {
    MockSysmanDriver driver;
    driver.useBaseInit = false;
    driver.useBaseDriverInit = true;
    driver.sysmanInitFail = true;

    auto result = zesInit(0);
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
    EXPECT_EQ(1u, driver.initCalledCount);
}

TEST(zesInit, whenCallingZesDriverGetWithoutZesInitThenZesDriverGetCallNotSucceed) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, zesDriverGet(&count, nullptr));
}

struct SysmanDriverTestMultipleFamilySupport : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < numRootDevices; i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<NEO::OSInterface>();
            executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::make_unique<SysmanMockDrm>(*executionEnvironment->rootDeviceEnvironments[i]));
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
    VariableBackup<decltype(NEO::SysCalls::sysCallsRealpath)> mockRealPath(&NEO::SysCalls::sysCallsRealpath, [](const char *path, char *buf) -> char * {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:02.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:02.0");
        return buf;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        std::string str = "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128";
        std::memcpy(buf, str.c_str(), str.size());
        return static_cast<int>(str.size());
    });

    ze_result_t returnValue;

    auto driverHandle = L0::Sysman::SysmanDriverHandle::create(*executionEnvironment, &returnValue);
    EXPECT_NE(nullptr, driverHandle);

    L0::Sysman::SysmanDriverHandleImp *driverHandleImp = reinterpret_cast<L0::Sysman::SysmanDriverHandleImp *>(driverHandle);
    EXPECT_EQ(numSupportedRootDevices, driverHandleImp->sysmanDevices.size());

    for (auto d : driverHandleImp->sysmanDevices) {
        EXPECT_TRUE(d->getHardwareInfo().capabilityTable.levelZeroSupported);
    }

    delete driverHandle;
    L0::Sysman::globalSysmanDriver = nullptr;
}

struct SysmanDriverTestMultipleFamilyNoSupport : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
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

struct SysmanDriverTestNoDeviceCreate : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->getMutableHardwareInfo()->capabilityTable.levelZeroSupported = true;
            executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<NEO::OSInterface>();
            executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::make_unique<SysmanMockDrm>(*executionEnvironment->rootDeviceEnvironments[i]));
        }
    }

    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    const uint32_t numRootDevices = 3u;
};

TEST_F(SysmanDriverTestNoDeviceCreate, GivenReadLinkSysCallFailWhenInitializingSysmanDriverWithArrayOfDevicesThenSysmanDeviceCreateFailAndDriverIsNull) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsRealpath)> mockRealPath(&NEO::SysCalls::sysCallsRealpath, [](const char *path, char *buf) -> char * {
        constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:02.0");
        strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:02.0");
        return buf;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
        return -1;
    });
    ze_result_t returnValue;
    this->executionEnvironment->incRefInternal();
    auto driverHandle = L0::Sysman::SysmanDriverHandle::create(*executionEnvironment, &returnValue);
    EXPECT_EQ(nullptr, driverHandle);
    EXPECT_EQ(returnValue, ZE_RESULT_ERROR_UNINITIALIZED);
    this->executionEnvironment->decRefInternal();
}

TEST_F(SysmanDriverTestNoDeviceCreate, GivenRealpathSysCallFailWhenInitializingSysmanDriverWithArrayOfDevicesThenSysmanDeviceCreateFailAndDriverIsNull) {
    VariableBackup<decltype(NEO::SysCalls::sysCallsRealpath)> mockRealPath(&NEO::SysCalls::sysCallsRealpath, [](const char *path, char *buf) -> char * {
        return nullptr;
    });
    ze_result_t returnValue;
    this->executionEnvironment->incRefInternal();
    auto driverHandle = L0::Sysman::SysmanDriverHandle::create(*executionEnvironment, &returnValue);
    EXPECT_EQ(nullptr, driverHandle);
    EXPECT_EQ(returnValue, ZE_RESULT_ERROR_UNINITIALIZED);
    this->executionEnvironment->decRefInternal();
}

struct SysmanDriverHandleTest : public ::testing::Test {
    void SetUp() override {
        VariableBackup<decltype(NEO::SysCalls::sysCallsRealpath)> mockRealPath(&NEO::SysCalls::sysCallsRealpath, [](const char *path, char *buf) -> char * {
            constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:02.0");
            strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:02.0");
            return buf;
        });

        VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, [](const char *path, char *buf, size_t bufsize) -> int {
            std::string str = "../../devices/pci0000:37/0000:37:01.0/0000:38:00.0/0000:39:01.0/0000:3a:00.0/drm/renderD128";
            std::memcpy(buf, str.c_str(), str.size());
            return static_cast<int>(str.size());
        });
        ze_result_t returnValue;

        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<NEO::OSInterface>();
            executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::make_unique<SysmanMockDrm>(*executionEnvironment->rootDeviceEnvironments[i]));
        }

        driverHandle = L0::Sysman::SysmanDriverHandle::create(*executionEnvironment, &returnValue);
        L0::Sysman::globalSysmanDriverHandle = driverHandle;
        L0::Sysman::driverCount = 1;
        L0::Sysman::sysmanOnlyInit = true;
    }
    void TearDown() override {
        if (driverHandle) {
            delete driverHandle;
        }
        L0::Sysman::globalSysmanDriver = nullptr;
        L0::Sysman::globalSysmanDriverHandle = nullptr;
        L0::Sysman::driverCount = 0;
        L0::Sysman::sysmanOnlyInit = false;
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

TEST_F(SysmanDriverHandleTest, givenInitializedDriverWhenZesDriverGetIsCalledThenGlobalSysmanDriverHandleIsObtained) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    uint32_t count = 1;
    zes_driver_handle_t hDriverHandle = reinterpret_cast<zes_driver_handle_t>(&hDriverHandle);

    result = zesDriverGet(&count, &hDriverHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, hDriverHandle);
    EXPECT_EQ(hDriverHandle, L0::Sysman::globalSysmanDriver);
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

TEST_F(SysmanDriverHandleTest,
       givenInitializedDriverWhenzesDriverGetExtensionPropertiesIsCalledThenUnsupportedIsReturned) {
    uint32_t count = 0;
    ze_result_t result = zesDriverGet(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1U, count);

    ze_driver_handle_t driverHandle = {};
    result = zesDriverGet(&count, &driverHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, driverHandle);
    uint32_t pCount = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesDriverGetExtensionProperties(driverHandle, &pCount, nullptr));
}

TEST_F(SysmanDriverHandleTest,
       givenDriverWhenGetExtensionFunctionAddressIsCalledWithValidAndInvalidFunctionNamesThenCorrectResultIsReturned) {
    uint32_t count = 0;
    ze_result_t result = zesDriverGet(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1U, count);

    ze_driver_handle_t driverHandle = {};
    result = zesDriverGet(&count, &driverHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, driverHandle);
    void *funPtr = nullptr;

    result = zesDriverGetExtensionFunctionAddress(driverHandle, "zexDriverImportExternalPointer", &funPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = zesDriverGetExtensionFunctionAddress(driverHandle, "zexDriverImportUnKnownPointer", &funPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST(SysmanDriverInit, GivenValidSysmanImpObjectWhenCallingInitWithSysmanInitFromCoreSetAsTrueThenSysmanInitFails) {
    L0::sysmanInitFromCore = true;
    std::unique_ptr<SysmanDriverImp> pSysmanDriverImp = std::make_unique<SysmanDriverImp>();
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, pSysmanDriverImp->driverInit());
    EXPECT_FALSE(L0::Sysman::sysmanOnlyInit);
    L0::sysmanInitFromCore = false;
}

} // namespace ult
} // namespace Sysman
} // namespace L0
