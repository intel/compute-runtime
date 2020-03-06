/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device.h"
#include <level_zero/ze_api.h>

#include <exception>
#include <new>

extern "C" {

__zedllexport ze_result_t __zecall
zeDeviceRegisterCLMemory(
    ze_device_handle_t hDevice,
    cl_context context,
    cl_mem mem,
    void **ptr) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == ptr)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->registerCLMemory(context, mem, ptr);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDeviceRegisterCLProgram(
    ze_device_handle_t hDevice,
    cl_context context,
    cl_program program,
    ze_module_handle_t *phModule) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phModule)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->registerCLProgram(context, program, phModule);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

__zedllexport ze_result_t __zecall
zeDeviceRegisterCLCommandQueue(
    ze_device_handle_t hDevice,
    cl_context context,
    cl_command_queue commandQueue,
    ze_command_queue_handle_t *phCommandQueue) {
    try {
        {
            if (nullptr == hDevice)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            if (nullptr == phCommandQueue)
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        return L0::Device::fromHandle(hDevice)->registerCLCommandQueue(context, commandQueue, phCommandQueue);
    } catch (ze_result_t &result) {
        return result;
    } catch (std::bad_alloc &) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    } catch (std::exception &) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}
}
