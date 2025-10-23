/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/ze_intel_gpu.h"

namespace L0 {

ze_context_handle_t zeDriverGetDefaultContext(ze_driver_handle_t hDriver);

ze_result_t zeDeviceSynchronize(ze_device_handle_t hDevice);

ze_result_t zeCommandListAppendLaunchKernelWithParameters(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hKernel,
    const ze_group_count_t *pGroupCounts,
    const void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t zeCommandListAppendLaunchKernelWithArguments(
    ze_command_list_handle_t hCommandList,
    ze_kernel_handle_t hKernel,
    const ze_group_count_t groupCounts,
    const ze_group_size_t groupSizes,
    void **pArguments,
    const void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents

);

ze_result_t zerGetLastErrorDescription(const char **ppString);

uint32_t zerTranslateDeviceHandleToIdentifier(ze_device_handle_t device);

ze_device_handle_t zerTranslateIdentifierToDeviceHandle(uint32_t identifier);

ze_context_handle_t zerGetDefaultContext();
} // namespace L0
