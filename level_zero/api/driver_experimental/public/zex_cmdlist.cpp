/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/driver_experimental/zex_cmdlist.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdlist/cmdlist_host_function_parameters.h"
#include "level_zero/ze_intel_gpu.h"

namespace L0 {

ze_result_t ZE_APICALL
zexCommandListAppendWaitOnMemory(
    zex_command_list_handle_t hCommandList,
    zex_wait_on_mem_desc_t *desc,
    void *ptr,
    uint32_t data,
    zex_event_handle_t hSignalEvent) {
    try {
        {
            hCommandList = toInternalType(hCommandList);
            if (nullptr == hCommandList) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
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

ze_result_t ZE_APICALL
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

ze_result_t ZE_APICALL
zexCommandListAppendWriteToMemory(
    zex_command_list_handle_t hCommandList,
    zex_write_to_mem_desc_t *desc,
    void *ptr,
    uint64_t data) {
    try {
        {
            hCommandList = toInternalType(hCommandList);
            if (nullptr == hCommandList) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
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

ze_result_t ZE_APICALL
zeCommandListAppendHostFunction(
    ze_command_list_handle_t hCommandList,
    void *pHostFunction,
    void *pUserData,
    void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    CmdListHostFunctionParameters parameters{};
    return L0::CommandList::fromHandle(hCommandList)->appendHostFunction(pHostFunction, pUserData, pNext, hSignalEvent, numWaitEvents, phWaitEvents, parameters);
}

ze_result_t ZE_APICALL
zexCommandListAppendMemoryCopyWithParameters(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    const void *srcptr,
    size_t size,
    const void *pNext,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents,
    ze_event_handle_t hSignalEvent) {

    if (nullptr == hCommandList) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }
    if (nullptr == dstptr) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }
    if (nullptr == srcptr) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }
    if ((nullptr == phWaitEvents) && (0 < numWaitEvents)) {
        return ZE_RESULT_ERROR_INVALID_SIZE;
    }

    auto cmdList = L0::CommandList::fromHandle(hCommandList);

    return cmdList->appendMemoryCopyWithParameters(dstptr, srcptr, size, pNext, hSignalEvent, numWaitEvents, phWaitEvents);
}

ze_result_t ZE_APICALL
zexCommandListAppendMemoryFillWithParameters(
    ze_command_list_handle_t hCommandList,
    void *ptr,
    const void *pattern,
    size_t patternSize,
    size_t size,
    const void *pNext,
    ze_event_handle_t hEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    if (nullptr == hCommandList) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }
    if (nullptr == ptr) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }
    if (nullptr == pattern) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }
    if ((nullptr == phWaitEvents) && (0 < numWaitEvents)) {
        return ZE_RESULT_ERROR_INVALID_SIZE;
    }

    auto cmdList = L0::CommandList::fromHandle(hCommandList);

    return cmdList->appendMemoryFillWithParameters(ptr, pattern, patternSize, size, pNext, hEvent, numWaitEvents, phWaitEvents);
}

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zexCommandListAppendWaitOnMemory(
    zex_command_list_handle_t hCommandList,
    zex_wait_on_mem_desc_t *desc,
    void *ptr,
    uint32_t data,
    zex_event_handle_t hSignalEvent) {
    return L0::zexCommandListAppendWaitOnMemory(hCommandList, desc, ptr, data, hSignalEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zexCommandListAppendWaitOnMemory64(
    zex_command_list_handle_t hCommandList,
    zex_wait_on_mem_desc_t *desc,
    void *ptr,
    uint64_t data,
    zex_event_handle_t hSignalEvent) {
    return L0::zexCommandListAppendWaitOnMemory64(hCommandList, desc, ptr, data, hSignalEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zexCommandListAppendWriteToMemory(
    zex_command_list_handle_t hCommandList,
    zex_write_to_mem_desc_t *desc,
    void *ptr,
    uint64_t data) {
    return L0::zexCommandListAppendWriteToMemory(hCommandList, desc, ptr, data);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendHostFunction(
    ze_command_list_handle_t hCommandList,
    void *pHostFunction,
    void *pUserData,
    void *pNext,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeCommandListAppendHostFunction(hCommandList, pHostFunction, pUserData, pNext, hSignalEvent, numWaitEvents, phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zexCommandListAppendMemoryCopyWithParameters(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    const void *srcptr,
    size_t size,
    const void *pNext,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents,
    ze_event_handle_t hSignalEvent) {
    return L0::zexCommandListAppendMemoryCopyWithParameters(hCommandList, dstptr, srcptr, size, pNext, numWaitEvents, phWaitEvents, hSignalEvent);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zexCommandListAppendMemoryFillWithParameters(
    ze_command_list_handle_t hCommandList,
    void *ptr,
    const void *pattern,
    size_t patternSize,
    size_t size,
    const void *pNext,
    ze_event_handle_t hEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zexCommandListAppendMemoryFillWithParameters(hCommandList, ptr, pattern, patternSize, size, pNext, hEvent, numWaitEvents, phWaitEvents);
}

} // extern "C"
