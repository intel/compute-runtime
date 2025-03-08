/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/driver_experimental/zex_cmdlist.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/ze_intel_gpu.h"

namespace L0 {
ZE_APIEXPORT ze_result_t ZE_APICALL
zexCommandListAppendWaitOnMemory(
    zex_command_list_handle_t hCommandList,
    zex_wait_on_mem_desc_t *desc,
    void *ptr,
    uint32_t data,
    zex_event_handle_t hSignalEvent) {
    try {
        {
            hCommandList = toInternalType(hCommandList);
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendWaitOnMemory(reinterpret_cast<void *>(desc), ptr, static_cast<uint64_t>(data), static_cast<ze_event_handle_t>(hSignalEvent), false);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zexCommandListAppendWaitOnMemory64(
    zex_command_list_handle_t hCommandList,
    zex_wait_on_mem_desc_t *desc,
    void *ptr,
    uint64_t data,
    zex_event_handle_t hSignalEvent) {

    hCommandList = toInternalType(hCommandList);
    if (!hCommandList) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return L0::CommandList::fromHandle(hCommandList)->appendWaitOnMemory(reinterpret_cast<void *>(desc), ptr, data, static_cast<ze_event_handle_t>(hSignalEvent), true);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zexCommandListAppendWriteToMemory(
    zex_command_list_handle_t hCommandList,
    zex_write_to_mem_desc_t *desc,
    void *ptr,
    uint64_t data) {
    try {
        {
            hCommandList = toInternalType(hCommandList);
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendWriteToMemory(reinterpret_cast<void *>(desc), ptr, data);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
} // namespace L0