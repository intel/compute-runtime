/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {
struct DeviceExtensionTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();
        executionEnvironment->incRefInternal();
        executionEnvironment->rootDeviceEnvironments[0]->initOsInterface(std::make_unique<NEO::HwDeviceId>(NEO::DriverModelType::wddm), rootDeviceIndex);
        executionEnvironment->memoryManager.reset(new MockWddmMemoryManager(*executionEnvironment));

        neoDevice = NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), executionEnvironment.get(), rootDeviceIndex);
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
    }

    void TearDown() override {
        driverHandle.reset(nullptr);
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    NEO::Device *neoDevice = nullptr;
    L0::Device *device = nullptr;
    const uint32_t rootDeviceIndex = 0u;
};

TEST_F(DeviceExtensionTest, whenGetExternalMemoryPropertiesIsCalledThenSuccessIsReturnedAndWin32OpaquePropertiesAreReturned) {
    ze_device_external_memory_properties_t externalMemoryProperties;

    ze_result_t result = device->getExternalMemoryProperties(&externalMemoryProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(externalMemoryProperties.imageExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32);
    EXPECT_FALSE(externalMemoryProperties.imageExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
    EXPECT_TRUE(externalMemoryProperties.imageImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32);
    EXPECT_TRUE(externalMemoryProperties.imageImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_HEAP);
    EXPECT_TRUE(externalMemoryProperties.imageImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_RESOURCE);
    EXPECT_TRUE(externalMemoryProperties.imageImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D11_TEXTURE);
    EXPECT_FALSE(externalMemoryProperties.imageImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
    EXPECT_TRUE(externalMemoryProperties.memoryAllocationExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32);
    EXPECT_FALSE(externalMemoryProperties.memoryAllocationExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
    EXPECT_TRUE(externalMemoryProperties.memoryAllocationImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32);
    EXPECT_TRUE(externalMemoryProperties.memoryAllocationImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_HEAP);
    EXPECT_TRUE(externalMemoryProperties.memoryAllocationImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_RESOURCE);
    EXPECT_FALSE(externalMemoryProperties.memoryAllocationImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
}

TEST_F(DeviceExtensionTest, givenDeviceCacheLineSizeExtensionThenGetCachePropertiesReturnsDeviceCachLineSizeGreaterThanZeroOnWddm) {
    ze_device_cache_line_size_ext_t cacheLineSizeExtDesc = {};
    cacheLineSizeExtDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_CACHELINE_SIZE_EXT;

    ze_device_cache_properties_t deviceCacheProperties = {};
    deviceCacheProperties.pNext = &cacheLineSizeExtDesc;

    uint32_t count = 1;
    ze_result_t res = device->getCacheProperties(&count, &deviceCacheProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_NE(0u, cacheLineSizeExtDesc.cacheLineSize);
}
} // namespace ult
} // namespace L0