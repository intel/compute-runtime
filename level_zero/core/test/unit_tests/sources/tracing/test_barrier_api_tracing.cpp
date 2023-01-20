/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListAppendBarrierTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendBarrier =
        [](ze_command_list_handle_t hCommandList, ze_event_handle_t hSignalEvent,
           uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) { return ZE_RESULT_SUCCESS; };
    ze_event_handle_t hSignalEvent = nullptr;
    uint32_t numWaitEvents = 0;

    prologCbs.CommandList.pfnAppendBarrierCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendBarrierCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendBarrierTracing(nullptr, hSignalEvent, numWaitEvents, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListAppendMemoryRangesBarrierTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryRangesBarrier =
        [](ze_command_list_handle_t hCommandList, uint32_t numRanges, const size_t *pRangeSizes, const void **pRanges,
           ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) { return ZE_RESULT_SUCCESS; };
    uint32_t numRanges = 1;
    const size_t pRangeSizes[] = {1};
    const void **pRanges = new const void *[1];
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryRangesBarrier =
        [](ze_command_list_handle_t hCommandList, uint32_t numRanges, const size_t *pRangeSizes, const void **pRanges,
           ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) { return ZE_RESULT_SUCCESS; };
    prologCbs.CommandList.pfnAppendMemoryRangesBarrierCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendMemoryRangesBarrierCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendMemoryRangesBarrierTracing(nullptr, numRanges, pRangeSizes, pRanges, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);

    delete[] pRanges;
}

} // namespace ult
} // namespace L0
