/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device.h"
#include <level_zero/ze_api.h>

extern "C" {

__zedllexport ze_result_t __zecall
zeDeviceRegisterCLMemory(
    ze_device_handle_t hDevice,
    cl_context context,
    cl_mem mem,
    void **ptr) {
    return L0::Device::fromHandle(hDevice)->registerCLMemory(context, mem, ptr);
}

__zedllexport ze_result_t __zecall
zeDeviceRegisterCLProgram(
    ze_device_handle_t hDevice,
    cl_context context,
    cl_program program,
    ze_module_handle_t *phModule) {
    return L0::Device::fromHandle(hDevice)->registerCLProgram(context, program, phModule);
}

__zedllexport ze_result_t __zecall
zeDeviceRegisterCLCommandQueue(
    ze_device_handle_t hDevice,
    cl_context context,
    cl_command_queue commandQueue,
    ze_command_queue_handle_t *phCommandQueue) {
    return L0::Device::fromHandle(hDevice)->registerCLCommandQueue(context, commandQueue, phCommandQueue);
}

} // extern "C"
