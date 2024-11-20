/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/include/ze_intel_gpu.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

TEST(ExternalSemaphoreTest, givenImportExternalSemaphoreExpIsCalledThenUnsupportedFeatureIsReturned) {
    ze_device_handle_t hDevice = nullptr;
    const ze_intel_external_semaphore_exp_desc_t *desc = nullptr;
    ze_intel_external_semaphore_exp_handle_t *hSemaphore = nullptr;
    ze_result_t result = zeIntelDeviceImportExternalSemaphoreExp(hDevice, hSemaphore, desc);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST(ExternalSemaphoreTest, givenAppendWaitExternalSemaphoresExpIsCalledThenUnsupportedFeatureIsReturned) {
    ze_command_list_handle_t hCmdList = nullptr;
    ze_intel_external_semaphore_exp_handle_t *hSemaphores = nullptr;
    const ze_intel_external_semaphore_wait_exp_params_t *params = nullptr;

    ze_result_t result = zeIntelCommandListAppendWaitExternalSemaphoresExp(hCmdList, hSemaphores, params, 0, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST(ExternalSemaphoreTest, givenAppendSignalExternalSemaphoresExpIsCalledThenUnsupportedFeatureIsReturned) {
    ze_command_list_handle_t hCmdList = nullptr;
    ze_intel_external_semaphore_exp_handle_t *hSemaphores = nullptr;
    const ze_intel_external_semaphore_signal_exp_params_t *params = nullptr;

    ze_result_t result = zeIntelCommandListAppendSignalExternalSemaphoresExp(hCmdList, hSemaphores, params, 0, nullptr, 0, nullptr);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST(ExternalSemaphoreTest, givenReleaseExternalSemaphoreExpIsCalledThenUnsupportedFeatureIsReturned) {
    ze_intel_external_semaphore_exp_handle_t hSemaphore = nullptr;
    ze_result_t result = zeIntelDeviceReleaseExternalSemaphoreExp(hSemaphore);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

} // namespace ult
} // namespace L0
