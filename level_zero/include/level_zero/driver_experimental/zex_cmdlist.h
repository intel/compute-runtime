/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ZEX_CMDLIST_H
#define _ZEX_CMDLIST_H
#if defined(__cplusplus)
#pragma once
#endif

#include <level_zero/ze_api.h>

#include "zex_common.h"

#if defined(__cplusplus)
extern "C" {
#endif

ZE_APIEXPORT ze_result_t ZE_APICALL
zexCommandListAppendWaitOnMemory(
    zex_command_list_handle_t hCommandList,
    zex_wait_on_mem_desc_t *desc,
    void *ptr,
    uint32_t data,
    zex_event_handle_t hSignalEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL
zexCommandListAppendWaitOnMemory64(
    zex_command_list_handle_t hCommandList,
    zex_wait_on_mem_desc_t *desc,
    void *ptr,
    uint64_t data,
    zex_event_handle_t hSignalEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL
zexCommandListAppendWriteToMemory(
    zex_command_list_handle_t hCommandList,
    zex_write_to_mem_desc_t *desc,
    void *ptr,
    uint64_t data);

ZE_APIEXPORT ze_result_t ZE_APICALL
zexCommandListAppendHostFunction(
    ze_command_list_handle_t hCommandList,
    void *pHostFunction,
    void *pUserData,
    void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zexCommandListAppendMemoryCopyWithParameters(
    ze_command_list_handle_t hCommandList, ///< [in] handle of the command list
    void *dstptr,                          ///< [in] destination pointer
    const void *srcptr,                    ///< [in] source pointer
    size_t size,                           ///< [in] size in bytes to copy
    const void *pNext,                     ///< [in][optional] additional copy parameters
    uint32_t numWaitEvents,                ///< [in][optional] number of events to wait on before launching
    ze_event_handle_t *phWaitEvents,       ///< [in][optional][range(0, numWaitEvents)] handle of the events to wait on before launching
    ze_event_handle_t hSignalEvent);       ///< [in][optional] handle of the event to signal on completion

ZE_APIEXPORT ze_result_t ZE_APICALL
zexCommandListAppendMemoryFillWithParameters(
    ze_command_list_handle_t hCommandList, ///< [in] handle of the command list
    void *ptr,                             ///< [in] pointer to memory to initialize
    const void *pattern,                   ///< [in] pointer to value to initialize memory to
    size_t patternSize,                    ///< [in] size in bytes of the value to initialize memory to
    size_t size,                           ///< [in] size in bytes to initialize
    const void *pNext,                     ///< [in][optional] additional copy parameters
    ze_event_handle_t hEvent,              ///< [in][optional] handle of the event to signal on completion
    uint32_t numWaitEvents,                ///< [in][optional] number of events to wait on before launching
    ze_event_handle_t *phWaitEvents);      ///< [in][optional][range(0, numWaitEvents)] handle of the events to wait on before launching

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // _ZEX_CMDLIST_H
