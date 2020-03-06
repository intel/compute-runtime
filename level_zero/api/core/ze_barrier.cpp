/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist.h"
#include <level_zero/ze_api.h>

#include <exception>
#include <new>

extern "C" {

__zedllexport ze_result_t __zecall
zeCommandListAppendBarrier(
    ze_command_list_handle_t hCommandList,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendBarrier(hSignalEvent, numWaitEvents, phWaitEvents);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListAppendMemoryRangesBarrier(
    ze_command_list_handle_t hCommandList,
    uint32_t numRanges,
    const size_t *pRangeSizes,
    const void **pRanges,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pRangeSizes)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == pRanges)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->appendMemoryRangesBarrier(numRanges, pRangeSizes, pRanges, hSignalEvent, numWaitEvents, phWaitEvents);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDeviceSystemBarrier(
    ze_device_handle_t hDevice) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->systemBarrier();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
}
