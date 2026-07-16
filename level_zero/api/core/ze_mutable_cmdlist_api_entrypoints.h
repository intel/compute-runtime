/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

namespace L0 {

ze_result_t ZE_APICALL zeCommandListGetNextCommandIdExp(
    ze_command_list_handle_t hCommandList,
    const ze_mutable_command_id_exp_desc_t *desc,
    uint64_t *pCommandId);

ze_result_t ZE_APICALL zeCommandListUpdateMutableCommandsExp(
    ze_command_list_handle_t hCommandList,
    const ze_mutable_commands_exp_desc_t *desc);

ze_result_t ZE_APICALL zeCommandListUpdateMutableCommandSignalEventExp(
    ze_command_list_handle_t hCommandList,
    uint64_t commandId,
    ze_event_handle_t hSignalEvent);

ze_result_t ZE_APICALL zeCommandListUpdateMutableCommandWaitEventsExp(
    ze_command_list_handle_t hCommandList,
    uint64_t commandId,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ze_result_t ZE_APICALL zeCommandListGetNextCommandIdWithKernelsExp(
    ze_command_list_handle_t hCommandList,
    const ze_mutable_command_id_exp_desc_t *desc,
    uint32_t numKernels,
    ze_kernel_handle_t *phKernels,
    uint64_t *pCommandId);

ze_result_t ZE_APICALL zeCommandListUpdateMutableCommandKernelsExp(
    ze_command_list_handle_t hCommandList,
    uint32_t numKernels,
    uint64_t *pCommandId,
    ze_kernel_handle_t *phKernels);

ze_result_t ZE_APICALL zeCommandListIsMutableExp(
    ze_command_list_handle_t hCommandList,
    ze_bool_t *pIsMutable);

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListGetNextCommandIdExp(
    ze_command_list_handle_t hCommandList,
    const ze_mutable_command_id_exp_desc_t *desc,
    uint64_t *pCommandId);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListUpdateMutableCommandsExp(
    ze_command_list_handle_t hCommandList,
    const ze_mutable_commands_exp_desc_t *desc);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListUpdateMutableCommandSignalEventExp(
    ze_command_list_handle_t hCommandList,
    uint64_t commandId,
    ze_event_handle_t hSignalEvent);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListUpdateMutableCommandWaitEventsExp(
    ze_command_list_handle_t hCommandList,
    uint64_t commandId,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListGetNextCommandIdWithKernelsExp(
    ze_command_list_handle_t hCommandList,
    const ze_mutable_command_id_exp_desc_t *desc,
    uint32_t numKernels,
    ze_kernel_handle_t *phKernels,
    uint64_t *pCommandId);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListUpdateMutableCommandKernelsExp(
    ze_command_list_handle_t hCommandList,
    uint32_t numKernels,
    uint64_t *pCommandId,
    ze_kernel_handle_t *phKernels);

ZE_APIEXPORT ze_result_t ZE_APICALL zeCommandListIsMutableExp(
    ze_command_list_handle_t hCommandList,
    ze_bool_t *pIsMutable);

} // extern "C"
