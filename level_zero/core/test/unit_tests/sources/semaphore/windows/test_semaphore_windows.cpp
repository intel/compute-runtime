/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/semaphore/external_semaphore_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
#include "level_zero/ze_intel_gpu.h"

using namespace NEO;
#include "gtest/gtest.h"

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

using WddmExternalSemaphoreTest = Test<WddmSemaphoreFixture>;

HWTEST_F(WddmExternalSemaphoreTest, givenDriverModelWddmWhenImportExternalSemaphoreExpIsCalledThenSuccessIsReturned) {
    MockDeviceImp l0Device(neoDevice, neoDevice->getExecutionEnvironment());
    ze_intel_external_semaphore_exp_desc_t desc = {};
    ze_intel_external_semaphore_exp_handle_t hSemaphore;
    HANDLE extSemaphoreHandle = 0;

    ze_intel_external_semaphore_win32_exp_desc_t win32Desc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_OPAQUE_WIN32;

    win32Desc.stype = ZE_INTEL_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_WIN32_EXP_DESC; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    win32Desc.handle = reinterpret_cast<void *>(extSemaphoreHandle);

    desc.pNext = &win32Desc;

    ze_result_t result = zeIntelDeviceImportExternalSemaphoreExp(l0Device.toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
    result = zeIntelDeviceReleaseExternalSemaphoreExp(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

} // namespace ult
} // namespace L0