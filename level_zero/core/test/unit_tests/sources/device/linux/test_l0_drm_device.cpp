/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

using DrmDeviceTests = Test<DeviceFixture>;

TEST_F(DrmDeviceTests, whenMemoryAccessPropertiesQueriedThenConcurrentDeviceSharedMemSupportDependsOnMemoryManagerHelper) {
    constexpr auto rootDeviceIndex{0U};

    execEnv->rootDeviceEnvironments[rootDeviceIndex]->osInterface.reset(new NEO::OSInterface);
    auto drm{new DrmMock{*execEnv->rootDeviceEnvironments[rootDeviceIndex]}};
    execEnv->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>{drm});

    auto *origMemoryManager{device->getDriverHandle()->getMemoryManager()};
    auto *proxyMemoryManager{new TestedDrmMemoryManager{*execEnv}};
    device->getDriverHandle()->setMemoryManager(proxyMemoryManager);

    auto &productHelper = device->getProductHelper();
    ze_device_memory_access_properties_t properties;
    for (auto pfSupported : std::array{false, true}) {
        drm->pageFaultSupported = pfSupported;
        bool isKmdMigrationAvailable{proxyMemoryManager->isKmdMigrationAvailable(rootDeviceIndex)};

        proxyMemoryManager->isKmdMigrationAvailableCalled = 0U;
        auto result = device->getMemoryAccessProperties(&properties);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(proxyMemoryManager->isKmdMigrationAvailableCalled, 1U);

        auto expectedSharedSingleDeviceAllocCapabilities = static_cast<ze_memory_access_cap_flags_t>(productHelper.getSingleDeviceSharedMemCapabilities(isKmdMigrationAvailable));
        EXPECT_EQ(expectedSharedSingleDeviceAllocCapabilities, properties.sharedSingleDeviceAllocCapabilities);
    }

    device->getDriverHandle()->setMemoryManager(origMemoryManager);
    delete proxyMemoryManager;
}

TEST_F(DrmDeviceTests, givenDriverModelIsNotDrmWhenUsingTheApiThenUnsupportedFeatureErrorIsReturned) {
    constexpr auto rootDeviceIndex{0U};

    execEnv->rootDeviceEnvironments[rootDeviceIndex]->osInterface.reset(new NEO::OSInterface);
    auto drm{new DrmMock{*execEnv->rootDeviceEnvironments[rootDeviceIndex]}};
    drm->getDriverModelTypeCallBase = false;
    drm->getDriverModelTypeResult = DriverModelType::unknown;
    execEnv->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>{drm});

    EXPECT_NE(nullptr, neoDevice->getRootDeviceEnvironment().osInterface.get());
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, device->reserveCache(3, 0));
}

TEST_F(DrmDeviceTests, givenCacheLevelUnsupportedViaCacheReservationApiWhenUsingTheApiThenUnsupportedFeatureErrorIsReturned) {
    constexpr auto rootDeviceIndex{0U};

    execEnv->rootDeviceEnvironments[rootDeviceIndex]->osInterface.reset(new NEO::OSInterface);
    auto drm{new DrmMock{*execEnv->rootDeviceEnvironments[rootDeviceIndex]}};
    execEnv->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>{drm});

    EXPECT_NE(nullptr, neoDevice->getRootDeviceEnvironment().osInterface.get());
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, device->reserveCache(1, 0));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, device->reserveCache(2, 0));
}

} // namespace ult
} // namespace L0