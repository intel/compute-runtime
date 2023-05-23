/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ZEX_SYSMAN_MEMORY_H
#define _ZEX_SYSMAN_MEMORY_H
#if defined(__cplusplus)
#pragma once
#endif

#include "level_zero/api/driver_experimental/public/zex_api.h"

namespace L0 {
///////////////////////////////////////////////////////////////////////////////
/// @brief Get memory bandwidth
///
/// @details
///     - The application may call this function from simultaneous threads.
///     - The implementation of this function should be lock-free.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
///     - ::ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hMemory`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == pBandwidth`
///     - ::ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS
///         + User does not have permissions to query this telemetry.
ze_result_t ZE_APICALL
zexSysmanMemoryGetBandwidth(
    zes_mem_handle_t hMemory, ///<[in] Handleforthecomponent.
    uint64_t *pReadCounters,  ///<[out] Total bytes read from memory in duration
    uint64_t *pWriteCounters, ///<[out] Total bytes written to memory in duration
    uint64_t *pMaxBandwidth,  ///<[out] Maximum bandwidth in units of bytes/sec
    uint64_t duration         ///<[in] duration in milliseconds
);

} // namespace L0
#endif
