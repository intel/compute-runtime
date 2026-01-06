/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_driver.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_drm.h"
#include "level_zero/zes_intel_gpu_sysman.h"

#include "gtest/gtest.h"

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

struct SysmanDriverTestNoDeviceCreate : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
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

bool verifyExtensionDefinition(std::vector<zes_driver_extension_properties_t> &extensionsReturned, uint32_t count) {
    const std::vector<std::pair<std::string, uint32_t>> supportedExtensions = {
        {ZES_DEVICE_ECC_DEFAULT_PROPERTIES_EXT_NAME, ZES_DEVICE_ECC_DEFAULT_PROPERTIES_EXT_VERSION_CURRENT},
        {ZES_POWER_LIMITS_EXT_NAME, ZES_POWER_LIMITS_EXT_VERSION_CURRENT},
        {ZES_ENGINE_ACTIVITY_EXT_NAME, ZES_ENGINE_ACTIVITY_EXT_VERSION_CURRENT},
        {ZES_RAS_GET_STATE_EXP_NAME, ZES_RAS_STATE_EXP_VERSION_CURRENT},
        {ZES_MEM_PAGE_OFFLINE_STATE_EXP_NAME, ZES_MEM_PAGE_OFFLINE_STATE_EXP_VERSION_CURRENT},
        {ZES_MEMORY_BANDWIDTH_COUNTER_BITS_EXP_PROPERTIES_NAME, ZES_MEM_BANDWIDTH_COUNTER_BITS_EXP_VERSION_CURRENT},
        {ZES_SYSMAN_DEVICE_MAPPING_EXP_NAME, ZES_SYSMAN_DEVICE_MAPPING_EXP_VERSION_CURRENT},
        {ZES_VIRTUAL_FUNCTION_MANAGEMENT_EXP_NAME, ZES_VF_MANAGEMENT_EXP_VERSION_CURRENT},
        {ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_STATE_NAME, ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_STATE_VERSION_CURRENT},
        {ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_PROPERTY_NAME, ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_PROPERTIES_VERSION_CURRENT},
        {ZES_INTEL_DRIVER_NAME_EXP_PROPERTY_NAME, ZES_INTEL_DRIVER_NAME_EXP_PROPERTIES_VERSION_CURRENT},
        {ZES_INTEL_RAS_ERROR_THRESHOLD_MANAGEMENT_EXTENSION_NAME, ZES_INTEL_RAS_CONFIG_EXP_VERSION_CURRENT}};
    for (uint32_t i = 0; i < count; i++) {
        if (extensionsReturned[i].name != supportedExtensions[i].first) {
            return false;
        }
        if (extensionsReturned[i].version != supportedExtensions[i].second) {
            return false;
        }
    }
    return true;
}

TEST_F(SysmanDriverHandleTest,
       givenInitializedDriverWhenzesDriverGetExtensionPropertiesIsCalledThenAllSupportedExtensionsAreReturned) {
    uint32_t count = 0;
    ze_result_t result = zesDriverGet(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1U, count);

    ze_driver_handle_t driverHandle = {};
    result = zesDriverGet(&count, &driverHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, driverHandle);
    uint32_t pCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverGetExtensionProperties(driverHandle, &pCount, nullptr));
    EXPECT_GE(pCount, 0u);
    std::vector<zes_driver_extension_properties_t> extensionsSupported(pCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverGetExtensionProperties(driverHandle, &pCount, extensionsSupported.data()));
    EXPECT_TRUE(verifyExtensionDefinition(extensionsSupported, pCount));
}

TEST_F(SysmanDriverHandleTest,
       givenInitializedDriverWhenzesDriverGetExtensionPropertiesIsCalledWithInvalidCountThenValidSupportedExtensionsCountIsReturned) {
    uint32_t count = 0;
    ze_result_t result = zesDriverGet(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1U, count);

    ze_driver_handle_t driverHandle = {};
    result = zesDriverGet(&count, &driverHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, driverHandle);
    uint32_t pCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverGetExtensionProperties(driverHandle, &pCount, nullptr));
    EXPECT_GE(pCount, 0u);
    std::vector<zes_driver_extension_properties_t> extensionsSupported(pCount);
    auto actualCount = pCount; // Store the actual count
    pCount += 5;               // Intentionally increase the count to check if function adjusts it correctly
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverGetExtensionProperties(driverHandle, &pCount, extensionsSupported.data()));
    EXPECT_EQ(actualCount, pCount);
}

TEST_F(SysmanDriverHandleTest,
       givenInitializedDriverWhenzesDriverGetExtensionPropertiesIsCalledWithCountLessThanActualSizeThenSameNumberOfSupportedExtensionsReturned) {
    uint32_t count = 0;
    ze_result_t result = zesDriverGet(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1U, count);

    ze_driver_handle_t driverHandle = {};
    result = zesDriverGet(&count, &driverHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, driverHandle);
    uint32_t pCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverGetExtensionProperties(driverHandle, &pCount, nullptr));
    EXPECT_GE(pCount, 0u);
    auto actualCount = pCount; // Store the actual count

    std::vector<zes_driver_extension_properties_t> extensionsSupported(actualCount);
    pCount -= 1; // Intentionally decrease the count to check if function adjusts it correctly
    // Filled with invalid data to ensure only valid extensions are filled by driver function
    strcpy_s(extensionsSupported[pCount].name, ZES_MAX_EXTENSION_NAME, "Invalid_Extension_Name");
    extensionsSupported[pCount].version = 1234;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverGetExtensionProperties(driverHandle, &pCount, extensionsSupported.data()));
    EXPECT_EQ(actualCount - 1, pCount);
    EXPECT_TRUE(verifyExtensionDefinition(extensionsSupported, pCount));
    // Verify that the last entry remains unchanged
    EXPECT_EQ(strcmp(extensionsSupported[actualCount - 1].name, "Invalid_Extension_Name"), 0);
    EXPECT_EQ(extensionsSupported[actualCount - 1].version, static_cast<uint32_t>(1234));
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

    result = zesDriverGetExtensionFunctionAddress(driverHandle, "zesIntelDevicePciLinkSpeedUpdateExp", &funPtr);
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
