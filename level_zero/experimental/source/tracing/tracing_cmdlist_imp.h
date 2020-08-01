/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "third_party/level_zero/ze_api_ext.h"

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListCreate_Tracing(ze_device_handle_t hDevice,
                            const ze_command_list_desc_t *desc,
                            ze_command_list_handle_t *phCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListCreateImmediate_Tracing(ze_device_handle_t hDevice,
                                     const ze_command_queue_desc_t *altdesc,
                                     ze_command_list_handle_t *phCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListDestroy_Tracing(ze_command_list_handle_t hCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListClose_Tracing(ze_command_list_handle_t hCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListReset_Tracing(ze_command_list_handle_t hCommandList);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListResetParameters_Tracing(ze_command_list_handle_t hCommandList);
}
