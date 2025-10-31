/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_driver.h"
#include "level_zero/zes_intel_gpu_sysman.h"

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
        {ZES_INTEL_DRIVER_NAME_EXP_PROPERTY_NAME, ZES_INTEL_DRIVER_NAME_EXP_PROPERTIES_VERSION_CURRENT}};
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
    uint32_t pCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverGetExtensionProperties(driverHandle->toHandle(), &pCount, nullptr));
    EXPECT_GE(pCount, 0u);
    std::vector<zes_driver_extension_properties_t> extensionsSupported(pCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverGetExtensionProperties(driverHandle->toHandle(), &pCount, extensionsSupported.data()));
    EXPECT_TRUE(verifyExtensionDefinition(extensionsSupported, pCount));
}

TEST_F(SysmanDriverHandleTest,
       givenInitializedDriverWhenzesDriverGetExtensionPropertiesIsCalledWithInvalidCountThenValidSupportedExtensionsCountIsReturned) {
    uint32_t pCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverGetExtensionProperties(driverHandle->toHandle(), &pCount, nullptr));
    EXPECT_GE(pCount, 0u);
    std::vector<zes_driver_extension_properties_t> extensionsSupported(pCount);
    auto actualCount = pCount; // Store the actual count
    pCount += 5;               // Intentionally increase the count to check if function adjusts it correctly
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverGetExtensionProperties(driverHandle->toHandle(), &pCount, extensionsSupported.data()));
    EXPECT_EQ(actualCount, pCount);
}

TEST_F(SysmanDriverHandleTest,
       givenInitializedDriverWhenzesDriverGetExtensionPropertiesIsCalledWithCountLessThanActualSizeThenSameNumberOfSupportedExtensionsReturned) {
    uint32_t pCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverGetExtensionProperties(driverHandle->toHandle(), &pCount, nullptr));
    EXPECT_GE(pCount, 0u);
    auto actualCount = pCount; // Store the actual count

    std::vector<zes_driver_extension_properties_t> extensionsSupported(actualCount);
    pCount -= 1; // Intentionally decrease the count to check if function adjusts it correctly
    // Filled with invalid data to ensure only valid extensions are filled by driver function
    strcpy_s(extensionsSupported[pCount].name, ZES_MAX_EXTENSION_NAME, "Invalid_Extension_Name");
    extensionsSupported[pCount].version = 1234;

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDriverGetExtensionProperties(driverHandle->toHandle(), &pCount, extensionsSupported.data()));
    EXPECT_EQ(actualCount - 1, pCount);
    EXPECT_TRUE(verifyExtensionDefinition(extensionsSupported, pCount));
    // Verify that the last entry remains unchanged
    EXPECT_EQ(strcmp(extensionsSupported[actualCount - 1].name, "Invalid_Extension_Name"), 0);
    EXPECT_EQ(extensionsSupported[actualCount - 1].version, static_cast<uint32_t>(1234));
}

TEST_F(SysmanDriverHandleTest,
       givenDriverWhenGetExtensionFunctionAddressIsCalledWithValidAndInvalidFunctionNamesThenCorrectResultIsReturned) {
    void *funPtr = nullptr;

    auto result = zesDriverGetExtensionFunctionAddress(driverHandle->toHandle(), "zexDriverImportUnKnownPointer", &funPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = zesDriverGetExtensionFunctionAddress(driverHandle->toHandle(), "zesIntelDevicePciLinkSpeedUpdateExp", &funPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
