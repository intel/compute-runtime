/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/hw_device_id.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/drm_mock_extended.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"

#include "level_zero/core/source/cache/cache_reservation.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {
struct DeviceWddmExtensionTest : public ::testing::Test {
    void SetUp() override {
        debugManager.flags.EnableChipsetUniqueUUID.set(0);
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

TEST_F(DeviceWddmExtensionTest, whenGetExternalMemoryPropertiesIsCalledThenSuccessIsReturnedAndWin32OpaquePropertiesAreReturned) {
    ze_device_external_memory_properties_t externalMemoryProperties;

    ze_result_t result = device->getExternalMemoryProperties(&externalMemoryProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(externalMemoryProperties.imageExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32);
    EXPECT_FALSE(externalMemoryProperties.imageExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
    EXPECT_TRUE(externalMemoryProperties.imageImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32);
    EXPECT_FALSE(externalMemoryProperties.imageImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
    EXPECT_TRUE(externalMemoryProperties.memoryAllocationExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32);
    EXPECT_FALSE(externalMemoryProperties.memoryAllocationExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
    EXPECT_TRUE(externalMemoryProperties.memoryAllocationImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32);
    EXPECT_FALSE(externalMemoryProperties.memoryAllocationImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
}

struct DeviceDrmExtensionTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();
        executionEnvironment->incRefInternal();
        executionEnvironment->rootDeviceEnvironments[0]->initOsInterface(std::make_unique<NEO::HwDeviceIdDrm>(0, ""), rootDeviceIndex);
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

TEST_F(DeviceDrmExtensionTest, whenGetExternalMemoryPropertiesIsCalledThenSuccessIsReturnedAndDmaBufPropertiesAreReturned) {
    ze_device_external_memory_properties_t externalMemoryProperties;

    ze_result_t result = device->getExternalMemoryProperties(&externalMemoryProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(externalMemoryProperties.imageExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32);
    EXPECT_FALSE(externalMemoryProperties.imageExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
    EXPECT_FALSE(externalMemoryProperties.imageImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32);
    EXPECT_FALSE(externalMemoryProperties.imageImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
    EXPECT_FALSE(externalMemoryProperties.memoryAllocationExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32);
    EXPECT_TRUE(externalMemoryProperties.memoryAllocationExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
    EXPECT_FALSE(externalMemoryProperties.memoryAllocationImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32);
    EXPECT_TRUE(externalMemoryProperties.memoryAllocationImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
}

struct DeviceExtensionTest : public ::testing::Test {
    void SetUp() override {
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), rootDeviceIndex);
        execEnv = neoDevice->getExecutionEnvironment();
        execEnv->incRefInternal();
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
    }

    void TearDown() override {
        driverHandle.reset(nullptr);
        execEnv->decRefInternal();
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::ExecutionEnvironment *execEnv;
    NEO::Device *neoDevice = nullptr;
    L0::Device *device = nullptr;
    const uint32_t rootDeviceIndex = 0u;
};

TEST_F(DeviceExtensionTest, whenGetExternalMemoryPropertiesWithoutOsInterfaceIsCalledThenSuccessIsReturnedAndNoPropertiesAreSet) {
    ze_device_external_memory_properties_t externalMemoryProperties{};

    ze_result_t result = device->getExternalMemoryProperties(&externalMemoryProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(externalMemoryProperties.imageExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32);
    EXPECT_FALSE(externalMemoryProperties.imageExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
    EXPECT_FALSE(externalMemoryProperties.imageImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32);
    EXPECT_FALSE(externalMemoryProperties.imageImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
    EXPECT_FALSE(externalMemoryProperties.memoryAllocationExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32);
    EXPECT_FALSE(externalMemoryProperties.memoryAllocationExportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
    EXPECT_FALSE(externalMemoryProperties.memoryAllocationImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32);
    EXPECT_FALSE(externalMemoryProperties.memoryAllocationImportTypes & ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF);
}

TEST_F(DeviceExtensionTest, givenDeviceCacheLineSizeExtensionThenGetCachePropertiesReturnsDeviceCachLineSizeGreaterThanZero) {
    ze_device_cache_line_size_ext_t cacheLineSizeExtDesc = {};
    cacheLineSizeExtDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_CACHELINE_SIZE_EXT;

    ze_device_cache_properties_t deviceCacheProperties = {};
    deviceCacheProperties.pNext = &cacheLineSizeExtDesc;

    uint32_t count = 1;
    ze_result_t res = device->getCacheProperties(&count, &deviceCacheProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_NE(0u, cacheLineSizeExtDesc.cacheLineSize);
}

class MockCacheReservation : public CacheReservation {
  public:
    ~MockCacheReservation() override = default;
    MockCacheReservation(L0::Device &device, bool initialize) : isInitialized(initialize){};

    bool reserveCache(size_t cacheLevel, size_t cacheReservationSize) override {
        receivedCacheLevel = cacheLevel;
        return isInitialized;
    }
    bool setCacheAdvice(void *ptr, size_t regionSize, ze_cache_ext_region_t cacheRegion) override {
        receivedCacheRegion = cacheRegion;
        return isInitialized;
    }
    size_t getMaxCacheReservationSize(size_t cacheLevel) override {
        return maxCacheReservationSize;
    }

    static size_t maxCacheReservationSize;

    bool isInitialized = false;
    size_t receivedCacheLevel = 3;
    ze_cache_ext_region_t receivedCacheRegion = ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_REGION_DEFAULT;
};

size_t MockCacheReservation::maxCacheReservationSize = 1024;

struct ZeDeviceCacheReservationTest : public ::testing::Test {
    void SetUp() override {
        neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), rootDeviceIndex);
        execEnv = neoDevice->getExecutionEnvironment();
        execEnv->incRefInternal();
        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];

        auto &rootDeviceEnvironment{neoDevice->getRootDeviceEnvironmentRef()};
        rootDeviceEnvironment.osInterface.reset(new NEO::OSInterface);
        mockDriverModel = new DrmMockExtended(rootDeviceEnvironment);
        rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDriverModel));
    }

    void TearDown() override {
        driverHandle.reset(nullptr);
        execEnv->decRefInternal();
    }

    DrmMockExtended *mockDriverModel = nullptr;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::ExecutionEnvironment *execEnv;
    NEO::Device *neoDevice = nullptr;
    L0::Device *device = nullptr;
    const uint32_t rootDeviceIndex = 1u;
};

TEST_F(ZeDeviceCacheReservationTest, givenDeviceCacheExtendedDescriptorWhenGetCachePropertiesCalledWithIncorrectStructureTypeThenReturnErrorUnsupportedEnumeration) {
    ze_cache_reservation_ext_desc_t cacheReservationExtDesc = {};

    ze_device_cache_properties_t deviceCacheProperties = {};
    deviceCacheProperties.pNext = &cacheReservationExtDesc;

    uint32_t count = 1;
    ze_result_t res = device->getCacheProperties(&count, &deviceCacheProperties);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION, res);
}

TEST_F(ZeDeviceCacheReservationTest, givenGreaterThanOneCountOfDeviceCachePropertiesWhenGetCachePropertiesIsCalledThenSetCountToOne) {
    static_cast<DeviceImp *>(device)->cacheReservation.reset(new MockCacheReservation(*device, true));
    ze_device_cache_properties_t deviceCacheProperties = {};

    uint32_t count = 10;
    ze_result_t res = device->getCacheProperties(&count, &deviceCacheProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_EQ(count, 1u);
}

TEST_F(ZeDeviceCacheReservationTest, givenDeviceCacheExtendedDescriptorWhenGetCachePropertiesCalledOnDeviceWithNoSupportForCacheReservationThenReturnZeroMaxCacheReservationSize) {
    VariableBackup<size_t> maxCacheReservationSizeBackup{&MockCacheReservation::maxCacheReservationSize, 0};
    static_cast<DeviceImp *>(device)->cacheReservation.reset(new MockCacheReservation(*device, true));

    ze_cache_reservation_ext_desc_t cacheReservationExtDesc = {};
    cacheReservationExtDesc.stype = ZE_STRUCTURE_TYPE_CACHE_RESERVATION_EXT_DESC;

    ze_device_cache_properties_t deviceCacheProperties = {};
    deviceCacheProperties.pNext = &cacheReservationExtDesc;

    uint32_t count = 1;
    ze_result_t res = device->getCacheProperties(&count, &deviceCacheProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(0u, cacheReservationExtDesc.maxCacheReservationSize);
}

TEST_F(ZeDeviceCacheReservationTest, givenDeviceCacheExtendedDescriptorWhenGetCachePropertiesCalledOnDeviceWithSupportForCacheReservationThenReturnNonZeroMaxCacheReservationSize) {
    static_cast<DeviceImp *>(device)->cacheReservation.reset(new MockCacheReservation(*device, true));

    ze_cache_reservation_ext_desc_t cacheReservationExtDesc = {};
    cacheReservationExtDesc.stype = ZE_STRUCTURE_TYPE_CACHE_RESERVATION_EXT_DESC;

    ze_device_cache_properties_t deviceCacheProperties = {};
    deviceCacheProperties.pNext = &cacheReservationExtDesc;

    uint32_t count = 1;
    ze_result_t res = device->getCacheProperties(&count, &deviceCacheProperties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_NE(0u, cacheReservationExtDesc.maxCacheReservationSize);
}

TEST_F(ZeDeviceCacheReservationTest, WhenCallingZeDeviceReserveCacheExtOnDeviceWithNoSupportForCacheReservationThenReturnErrorUnsupportedFeature) {
    VariableBackup<size_t> maxCacheReservationSizeBackup{&MockCacheReservation::maxCacheReservationSize, 0};
    static_cast<DeviceImp *>(device)->cacheReservation.reset(new MockCacheReservation(*device, true));

    size_t cacheLevel = 3;
    size_t cacheReservationSize = 1024;

    auto result = zeDeviceReserveCacheExt(device->toHandle(), cacheLevel, cacheReservationSize);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

TEST_F(ZeDeviceCacheReservationTest, WhenCallingZeDeviceReserveCacheExtWithCacheLevel0ThenDriverShouldDefaultToCacheLevel3) {
    auto mockCacheReservation = new MockCacheReservation(*device, true);
    static_cast<DeviceImp *>(device)->cacheReservation.reset(mockCacheReservation);

    size_t cacheLevel = 0;
    size_t cacheReservationSize = 1024;

    auto result = zeDeviceReserveCacheExt(device->toHandle(), cacheLevel, cacheReservationSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(3u, mockCacheReservation->receivedCacheLevel);
}

TEST_F(ZeDeviceCacheReservationTest, WhenCallingZeDeviceReserveCacheExtFailsToReserveCacheOnDeviceThenReturnErrorUninitialized) {
    size_t cacheLevel = 3;
    size_t cacheReservationSize = 1024;

    for (auto initialize : {false, true}) {
        auto mockCacheReservation = new MockCacheReservation(*device, initialize);
        static_cast<DeviceImp *>(device)->cacheReservation.reset(mockCacheReservation);

        auto result = zeDeviceReserveCacheExt(device->toHandle(), cacheLevel, cacheReservationSize);

        if (initialize) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        } else {
            EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
        }

        EXPECT_EQ(3u, mockCacheReservation->receivedCacheLevel);
    }
}

TEST_F(ZeDeviceCacheReservationTest, givenNonDrmDriverModelWhenCallingZeDeviceReserveCacheExtThenUnsupportedFeatureFlagReturned) {
    size_t cacheLevel = 3;
    size_t cacheReservationSize = 1024;

    auto mockCacheReservation = new MockCacheReservation(*device, true);
    static_cast<DeviceImp *>(device)->cacheReservation.reset(mockCacheReservation);
    mockDriverModel->getDriverModelTypeCallBase = false;

    auto result = zeDeviceReserveCacheExt(device->toHandle(), cacheLevel, cacheReservationSize);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

TEST_F(ZeDeviceCacheReservationTest, WhenCallingZeDeviceSetCacheAdviceExtWithDefaultCacheRegionThenDriverShouldDefaultToNonReservedRegion) {
    auto mockCacheReservation = new MockCacheReservation(*device, true);
    static_cast<DeviceImp *>(device)->cacheReservation.reset(mockCacheReservation);

    void *ptr = reinterpret_cast<void *>(0x123456789);
    size_t regionSize = 512;
    ze_cache_ext_region_t cacheRegion = ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_REGION_DEFAULT;

    auto result = zeDeviceSetCacheAdviceExt(device->toHandle(), ptr, regionSize, cacheRegion);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_NON_RESERVED_REGION, mockCacheReservation->receivedCacheRegion);
}

TEST_F(ZeDeviceCacheReservationTest, givenNonDrmDriverModelWhenCallingZeDeviceSetCacheAdviceExtThenUnsupportedFeatureFlagReturned) {
    auto mockCacheReservation = new MockCacheReservation(*device, true);
    static_cast<DeviceImp *>(device)->cacheReservation.reset(mockCacheReservation);

    void *ptr = reinterpret_cast<void *>(0x123456789);
    size_t regionSize = 512;
    ze_cache_ext_region_t cacheRegion = ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_REGION_DEFAULT;
    mockDriverModel->getDriverModelTypeCallBase = false;

    auto result = zeDeviceSetCacheAdviceExt(device->toHandle(), ptr, regionSize, cacheRegion);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

TEST_F(ZeDeviceCacheReservationTest, WhenCallingZeDeviceSetCacheAdviceExtOnDeviceWithNoSupportForCacheReservationThenReturnErrorUnsupportedFeature) {
    VariableBackup<size_t> maxCacheReservationSizeBackup{&MockCacheReservation::maxCacheReservationSize, 0};
    static_cast<DeviceImp *>(device)->cacheReservation.reset(new MockCacheReservation(*device, true));

    void *ptr = reinterpret_cast<void *>(0x123456789);
    size_t regionSize = 512;
    ze_cache_ext_region_t cacheRegion = ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_REGION_DEFAULT;

    auto result = zeDeviceSetCacheAdviceExt(device->toHandle(), ptr, regionSize, cacheRegion);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

TEST_F(ZeDeviceCacheReservationTest, WhenCallingZeDeviceSetCacheAdviceExtFailsToSetCacheRegionThenReturnErrorUnitialized) {
    void *ptr = reinterpret_cast<void *>(0x123456789);
    size_t regionSize = 512;
    ze_cache_ext_region_t cacheRegion = ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_RESERVE_REGION;

    for (auto initialize : {false, true}) {
        auto mockCacheReservation = new MockCacheReservation(*device, initialize);
        static_cast<DeviceImp *>(device)->cacheReservation.reset(mockCacheReservation);

        auto result = zeDeviceSetCacheAdviceExt(device->toHandle(), ptr, regionSize, cacheRegion);

        if (initialize) {
            EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        } else {
            EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
        }

        EXPECT_EQ(ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_RESERVE_REGION, mockCacheReservation->receivedCacheRegion);
    }
}

} // namespace ult
} // namespace L0