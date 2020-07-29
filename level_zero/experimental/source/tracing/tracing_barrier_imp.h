/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendBarrier_Tracing(ze_command_list_handle_t hCommandList,
                                   ze_event_handle_t hSignalEvent,
                                   uint32_t numWaitEvents,
                                   ze_event_handle_t *phWaitEvents);

ZE_APIEXPORT ze_result_t ZE_APICALL
zeCommandListAppendMemoryRangesBarrier_Tracing(ze_command_list_handle_t hCommandList,
                                               uint32_t numRanges,
                                               const size_t *pRangeSizes,
                                               const void **pRanges,
                                               ze_event_handle_t hSignalEvent,
                                               uint32_t numWaitEvents,
                                               ze_event_handle_t *phWaitEvents);
}
