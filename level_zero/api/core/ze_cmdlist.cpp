/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist.h"
#include <level_zero/ze_api.h>

extern "C" {

__zedllexport ze_result_t __zecall
zeCommandListCreate(
    ze_device_handle_t hDevice,
    const ze_command_list_desc_t *desc,
    ze_command_list_handle_t *phCommandList) {
    return L0::Device::fromHandle(hDevice)->createCommandList(desc, phCommandList);
}

__zedllexport ze_result_t __zecall
zeCommandListCreateImmediate(
    ze_device_handle_t hDevice,
    const ze_command_queue_desc_t *altdesc,
    ze_command_list_handle_t *phCommandList) {
    return L0::Device::fromHandle(hDevice)->createCommandListImmediate(altdesc, phCommandList);
}

__zedllexport ze_result_t __zecall
zeCommandListDestroy(
    ze_command_list_handle_t hCommandList) {
    return L0::CommandList::fromHandle(hCommandList)->destroy();
}

__zedllexport ze_result_t __zecall
zeCommandListClose(
    ze_command_list_handle_t hCommandList) {
    return L0::CommandList::fromHandle(hCommandList)->close();
}

__zedllexport ze_result_t __zecall
zeCommandListReset(
    ze_command_list_handle_t hCommandList) {
    return L0::CommandList::fromHandle(hCommandList)->reset();
}

} // extern "C"
