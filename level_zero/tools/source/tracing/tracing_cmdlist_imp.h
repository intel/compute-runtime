/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

__zedllexport ze_result_t __zecall
zeCommandListCreate_Tracing(ze_device_handle_t hDevice,
                            const ze_command_list_desc_t *desc,
                            ze_command_list_handle_t *phCommandList);

__zedllexport ze_result_t __zecall
zeCommandListCreateImmediate_Tracing(ze_device_handle_t hDevice,
                                     const ze_command_queue_desc_t *altdesc,
                                     ze_command_list_handle_t *phCommandList);

__zedllexport ze_result_t __zecall
zeCommandListDestroy_Tracing(ze_command_list_handle_t hCommandList);

__zedllexport ze_result_t __zecall
zeCommandListClose_Tracing(ze_command_list_handle_t hCommandList);

__zedllexport ze_result_t __zecall
zeCommandListReset_Tracing(ze_command_list_handle_t hCommandList);

__zedllexport ze_result_t __zecall
zeCommandListResetParameters_Tracing(ze_command_list_handle_t hCommandList);
}
