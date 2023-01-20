/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/driver_experimental/public/zex_common.h"
#include <level_zero/ze_api.h>

namespace L0 {

ZE_APIEXPORT ze_result_t ZE_APICALL
zexCommandListAppendWaitOnMemory(
    zex_command_list_handle_t hCommandList,
    zex_wait_on_mem_desc_t *desc,
    void *ptr,
    uint32_t data,
    zex_event_handle_t hSignalEvent);
ZE_APIEXPORT ze_result_t ZE_APICALL
zexCommandListAppendWriteToMemory(
    zex_command_list_handle_t hCommandList,
    zex_write_to_mem_desc_t *desc,
    void *ptr,
    uint64_t data);
} // namespace L0
