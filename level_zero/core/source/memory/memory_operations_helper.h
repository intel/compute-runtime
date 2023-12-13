/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/memory_operations_status.h"

#include <level_zero/ze_api.h>

static ze_result_t changeMemoryOperationStatusToL0ResultType(NEO::MemoryOperationsStatus status) {
    switch (status) {
    case NEO::MemoryOperationsStatus::success:
        return ZE_RESULT_SUCCESS;

    case NEO::MemoryOperationsStatus::memoryNotFound:
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;

    case NEO::MemoryOperationsStatus::outOfMemory:
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;

    case NEO::MemoryOperationsStatus::failed:
        return ZE_RESULT_ERROR_DEVICE_LOST;

    case NEO::MemoryOperationsStatus::deviceUninitialized:
        return ZE_RESULT_ERROR_UNINITIALIZED;

    case NEO::MemoryOperationsStatus::unsupported:
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;

    default:
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
