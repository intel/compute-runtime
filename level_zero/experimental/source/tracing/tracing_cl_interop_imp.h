/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

__zedllexport ze_result_t __zecall
zeDeviceRegisterCLMemory_Tracing(ze_device_handle_t hDevice,
                                 cl_context context,
                                 cl_mem mem,
                                 void **ptr);

__zedllexport ze_result_t __zecall
zeDeviceRegisterCLProgram_Tracing(ze_device_handle_t hDevice,
                                  cl_context context,
                                  cl_program program,
                                  ze_module_handle_t *phModule);

__zedllexport ze_result_t __zecall
zeDeviceRegisterCLCommandQueue_Tracing(ze_device_handle_t hDevice,
                                       cl_context context,
                                       cl_command_queue commandQueue,
                                       ze_command_queue_handle_t *phCommandQueue);
}
