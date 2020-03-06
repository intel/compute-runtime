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
zeCommandListCreate(
    ze_device_handle_t hDevice,
    const ze_command_list_desc_t *desc,
    ze_command_list_handle_t *phCommandList) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == desc)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (ZE_COMMAND_LIST_DESC_VERSION_CURRENT < desc->version)
                return ZE_RESULT_ERROR_UNKNOWN;
        }
        return L0::Device::fromHandle(hDevice)->createCommandList(desc, phCommandList);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListCreateImmediate(
    ze_device_handle_t hDevice,
    const ze_command_queue_desc_t *altdesc,
    ze_command_list_handle_t *phCommandList) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == altdesc)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT < altdesc->version)
                return ZE_RESULT_ERROR_UNKNOWN;
        }
        return L0::Device::fromHandle(hDevice)->createCommandListImmediate(altdesc, phCommandList);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListDestroy(
    ze_command_list_handle_t hCommandList) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->destroy();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListClose(
    ze_command_list_handle_t hCommandList) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->close();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeCommandListReset(
    ze_command_list_handle_t hCommandList) {
    try {
        {
            if (nullptr == hCommandList)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::CommandList::fromHandle(hCommandList)->reset();
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
}
