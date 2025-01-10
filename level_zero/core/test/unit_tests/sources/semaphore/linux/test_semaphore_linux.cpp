/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/linux/drm_mock_cache_info.h"
#include "shared/test/common/os_interface/linux/drm_mock_extended.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/semaphore/external_semaphore_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/ze_intel_gpu.h"

using namespace NEO;
#include "gtest/gtest.h"

namespace L0 {
namespace ult {

class DrmSemaphoreFixture : public DeviceFixture {
  public:
    void setUp() {
        DeviceFixture::setUp();

        auto &rootDeviceEnvironment{*neoDevice->executionEnvironment->rootDeviceEnvironments[0]};
        drmMock = new DrmMock(rootDeviceEnvironment);

        rootDeviceEnvironment.osInterface.reset(new NEO::OSInterface);
        rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<NEO::Drm>(drmMock));
    }
    void tearDown() {
        DeviceFixture::tearDown();
    }

    DrmMock *drmMock = nullptr;
};

using DrmExternalSemaphoreTest = Test<DrmSemaphoreFixture>;

HWTEST_F(DrmExternalSemaphoreTest, givenDriverModelDrmWhenImportExternalSemaphoreExpIsCalledThenUnsupportedFeatureIsReturned) {
    MockDeviceImp l0Device(neoDevice, neoDevice->getExecutionEnvironment());
    ze_intel_external_semaphore_exp_desc_t desc = {};
    ze_intel_external_semaphore_exp_handle_t hSemaphore;
    int fd = 0;

    ze_intel_external_semaphore_desc_fd_exp_desc_t fdDesc = {};

    desc.flags = ZE_EXTERNAL_SEMAPHORE_EXP_FLAGS_OPAQUE_FD;

    fdDesc.stype = ZE_INTEL_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_FD_EXP_DESC; // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    fdDesc.fd = fd;

    desc.pNext = &fdDesc;

    ze_result_t result = zeIntelDeviceImportExternalSemaphoreExp(l0Device.toHandle(), &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

} // namespace ult
} // namespace L0