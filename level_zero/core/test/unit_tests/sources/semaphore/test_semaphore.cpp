/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
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

using ExternalSemaphoreTest = Test<DeviceFixture>;
HWTEST_F(ExternalSemaphoreTest, givenInvalidDescriptorAndImportExternalSemaphoreExpIsCalledThenInvalidArgumentsIsReturned) {
    ze_device_handle_t hDevice = device->toHandle();
    const ze_intel_external_semaphore_exp_desc_t desc = {};
    ze_intel_external_semaphore_exp_handle_t hSemaphore;
    ze_result_t result = zeIntelDeviceImportExternalSemaphoreExp(hDevice, &desc, &hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

HWTEST_F(ExternalSemaphoreTest, givenAppendWaitExternalSemaphoresExpIsCalledThenUnsupportedFeatureIsReturned) {
    ze_command_list_handle_t hCmdList = nullptr;
    ze_intel_external_semaphore_exp_handle_t *hSemaphores = nullptr;
    const ze_intel_external_semaphore_wait_params_exp_t *params = nullptr;

    ze_result_t result = zeIntelCommandListAppendWaitExternalSemaphoresExp(hCmdList, 0, hSemaphores, params, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST_F(ExternalSemaphoreTest, givenAppendSignalExternalSemaphoresExpIsCalledThenUnsupportedFeatureIsReturned) {
    ze_command_list_handle_t hCmdList = nullptr;
    ze_intel_external_semaphore_exp_handle_t *hSemaphores = nullptr;
    const ze_intel_external_semaphore_signal_params_exp_t *params = nullptr;

    ze_result_t result = zeIntelCommandListAppendSignalExternalSemaphoresExp(hCmdList, 0, hSemaphores, params, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST_F(ExternalSemaphoreTest, givenValidParametersWhenAppendWaitIsCalledThenSuccessIsReturned) {
    MockCommandStreamReceiver csr(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    auto externalSemaphoreImp = std::make_unique<ExternalSemaphoreImp>();
    ze_result_t result = externalSemaphoreImp->appendWait(&csr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST_F(ExternalSemaphoreTest, givenValidParametersWhenAppendSignalIsCalledThenSuccessIsReturned) {
    MockCommandStreamReceiver csr(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    auto externalSemaphoreImp = std::make_unique<ExternalSemaphoreImp>();
    ze_result_t result = externalSemaphoreImp->appendSignal(&csr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

HWTEST_F(ExternalSemaphoreTest, givenValidParametersWhenReleaseExternalSemaphoreIsCalledThenSuccessIsReturned) {
    auto externalSemaphoreImp = new ExternalSemaphoreImp();
    ze_result_t result = externalSemaphoreImp->releaseExternalSemaphore();
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST_F(ExternalSemaphoreTest, givenInvalidDescriptorWhenInitializeIsCalledThenInvalidArgumentIsReturned) {
    ze_device_handle_t hDevice = device->toHandle();
    const ze_intel_external_semaphore_exp_desc_t desc = {};
    auto externalSemaphoreImp = new ExternalSemaphoreImp();
    ze_result_t result = externalSemaphoreImp->initialize(hDevice, &desc);
    EXPECT_EQ(result, ZE_RESULT_ERROR_INVALID_ARGUMENT);
    delete externalSemaphoreImp;
}

} // namespace ult
} // namespace L0