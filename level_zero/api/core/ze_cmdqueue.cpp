/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdqueue.h"
#include <level_zero/ze_api.h>

#include <exception>
#include <new>

extern "C" {

__zedllexport ze_result_t __zecall
zeCommandQueueCreate(
    ze_device_handle_t hDevice,
    const ze_command_queue_desc_t *desc,
    ze_command_queue_handle_t *phCommandQueue) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == desc)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phCommandQueue)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT < desc->version)
                return ZE_RESULT_ERROR_UNKNOWN;
        }
        return L0::Device::fromHandle(hDevice)->createCommandQueue(desc, phCommandQueue);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandQueueDestroy(
    ze_command_queue_handle_t hCommandQueue) {
    try {
        {
            if (nullptr == hCommandQueue)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandQueue::fromHandle(hCommandQueue)->destroy();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandQueueExecuteCommandLists(
    ze_command_queue_handle_t hCommandQueue,
    uint32_t numCommandLists,
    ze_command_list_handle_t *phCommandLists,
    ze_fence_handle_t hFence) {
    try {
        {
            if (nullptr == hCommandQueue)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phCommandLists)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandQueue::fromHandle(hCommandQueue)->executeCommandLists(numCommandLists, phCommandLists, hFence, true);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandQueueSynchronize(
    ze_command_queue_handle_t hCommandQueue,
    uint32_t timeout) {
    try {
        {
            if (nullptr == hCommandQueue)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandQueue::fromHandle(hCommandQueue)->synchronize(timeout);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
}
