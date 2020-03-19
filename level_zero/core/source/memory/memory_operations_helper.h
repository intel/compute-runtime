/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/memory_operations_status.h"

#include <level_zero/ze_common.h>

static ze_result_t changeMemoryOperationStatusToL0ResultType(NEO::MemoryOperationsStatus status) {
    switch (status) {
    case NEO::MemoryOperationsStatus::SUCCESS:
        return ZE_RESULT_SUCCESS;

    case NEO::MemoryOperationsStatus::MEMORY_NOT_FOUND:
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;

    case NEO::MemoryOperationsStatus::OUT_OF_MEMORY:
    case NEO::MemoryOperationsStatus::FAILED:
        return ZE_RESULT_ERROR_DEVICE_LOST;

    case NEO::MemoryOperationsStatus::DEVICE_UNINITIALIZED:
        return ZE_RESULT_ERROR_UNINITIALIZED;

    case NEO::MemoryOperationsStatus::UNSUPPORTED:
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;

    default:
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
