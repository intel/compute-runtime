/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/semaphore/external_semaphore_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace L0 {
namespace ult {

class WddmSemaphoreFixture : public DeviceFixture {
  public:
    void setUp() {
        DeviceFixture::setUp();

        auto &rootDeviceEnvironment{*neoDevice->executionEnvironment->rootDeviceEnvironments[0]};

        auto wddm = new WddmMock(rootDeviceEnvironment);
        rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
    }

    void tearDown() {
        DeviceFixture::tearDown();
    }
};

using WddmExternalSemaphoreMTTest = Test<WddmSemaphoreFixture>;
using MockDriverHandle = Mock<L0::DriverHandle>;

HWTEST_F(WddmExternalSemaphoreMTTest, givenValidExternalSemaphoreWhenImportExternalSemaphoreExpIsCalledThenSuccessIsReturned) {
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandle = std::make_unique<MockDriverHandle>();
    ze_external_semaphore_ext_desc_t desc = {};
    ze_external_semaphore_ext_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;

    l0Device->setDriverHandle(driverHandle.get());

    ze_external_semaphore_win32_ext_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_D3D12_FENCE;

    win32Desc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXT_DESC;
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);

    desc.pNext = &win32Desc;

    ze_result_t result = zeDeviceImportExternalSemaphoreExt(l0Device->toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    result = zeDeviceReleaseExternalSemaphoreExt(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(WddmExternalSemaphoreMTTest, givenValidTimelineSemaphoreWhenImportExternalSemaphoreIsCalledThenSuccessIsReturned) {
    auto l0Device = std::make_unique<MockDeviceImp>(neoDevice);
    auto driverHandle = std::make_unique<MockDriverHandle>();
    ze_external_semaphore_ext_desc_t desc = {};
    ze_external_semaphore_ext_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;
    const char *extSemName = "timeline_semaphore_name";

    l0Device->setDriverHandle(driverHandle.get());

    ze_external_semaphore_win32_ext_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXT_FLAG_VK_TIMELINE_SEMAPHORE_WIN32;

    win32Desc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXT_DESC;
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);
    win32Desc.name = extSemName;

    desc.pNext = &win32Desc;

    ze_result_t result = zeDeviceImportExternalSemaphoreExt(l0Device->toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    result = zeDeviceReleaseExternalSemaphoreExt(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

} // namespace ult
} // namespace L0
