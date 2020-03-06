/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/fence.h"
#include <level_zero/ze_api.h>

#include <exception>
#include <new>

extern "C" {

__zedllexport ze_result_t __zecall
zeFenceCreate(
    ze_command_queue_handle_t hCommandQueue,
    const ze_fence_desc_t *desc,
    ze_fence_handle_t *phFence) {
    try {
        {
            if (nullptr == hCommandQueue)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == desc)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phFence)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (ZE_FENCE_DESC_VERSION_CURRENT < desc->version)
                return ZE_RESULT_ERROR_UNKNOWN;
        }
        return L0::fenceCreate(hCommandQueue, desc, phFence);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeFenceDestroy(
    ze_fence_handle_t hFence) {
    try {
        {
            if (nullptr == hFence)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::fenceDestroy(hFence);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeFenceHostSynchronize(
    ze_fence_handle_t hFence,
    uint32_t timeout) {
    try {
        {
            if (nullptr == hFence)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Fence::fromHandle(hFence)->hostSynchronize(timeout);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeFenceQueryStatus(
    ze_fence_handle_t hFence) {
    try {
        {
            if (nullptr == hFence)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Fence::fromHandle(hFence)->queryStatus();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeFenceReset(
    ze_fence_handle_t hFence) {
    try {
        {
            if (nullptr == hFence)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Fence::fromHandle(hFence)->reset();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
}
