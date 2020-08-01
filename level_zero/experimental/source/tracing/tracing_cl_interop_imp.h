/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "third_party/level_zero/ze_api_ext.h"

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceRegisterCLMemory_Tracing(ze_device_handle_t hDevice,
                                 cl_context context,
                                 cl_mem mem,
                                 void **ptr);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceRegisterCLProgram_Tracing(ze_device_handle_t hDevice,
                                  cl_context context,
                                  cl_program program,
                                  ze_module_handle_t *phModule);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceRegisterCLCommandQueue_Tracing(ze_device_handle_t hDevice,
                                       cl_context context,
                                       cl_command_queue commandQueue,
                                       ze_command_queue_handle_t *phCommandQueue);
}
