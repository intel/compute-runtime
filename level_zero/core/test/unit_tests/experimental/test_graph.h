/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_image.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/experimental/source/graph/graph.h"
#include "level_zero/include/level_zero/driver_experimental/zex_graph.h"

using namespace NEO;

namespace L0 {
ze_result_t zeCommandListAppendWriteGlobalTimestamp(ze_command_list_handle_t hCommandList, uint64_t *dstptr, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendBarrier(ze_command_list_handle_t hCommandList, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendMemoryRangesBarrier(ze_command_list_handle_t hCommandList, uint32_t numRanges, const size_t *pRangeSizes, const void **pRanges, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendMemoryCopy(ze_command_list_handle_t hCommandList, void *dstptr, const void *srcptr, size_t size, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendMemoryFill(ze_command_list_handle_t hCommandList, void *ptr, const void *pattern, size_t patternSize, size_t size, ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendMemoryCopyRegion(ze_command_list_handle_t hCommandList, void *dstptr, const ze_copy_region_t *dstRegion, uint32_t dstPitch, uint32_t dstSlicePitch, const void *srcptr, const ze_copy_region_t *srcRegion, uint32_t srcPitch, uint32_t srcSlicePitch, ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendMemoryCopyFromContext(ze_command_list_handle_t hCommandList, void *dstptr, ze_context_handle_t hContextSrc, const void *srcptr, size_t size, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendImageCopy(ze_command_list_handle_t hCommandList, ze_image_handle_t hDstImage, ze_image_handle_t hSrcImage, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendImageCopyRegion(ze_command_list_handle_t hCommandList, ze_image_handle_t hDstImage, ze_image_handle_t hSrcImage, const ze_image_region_t *pDstRegion, const ze_image_region_t *pSrcRegion, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendImageCopyToMemory(ze_command_list_handle_t hCommandList, void *dstptr, ze_image_handle_t hSrcImage, const ze_image_region_t *pSrcRegion, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendImageCopyFromMemory(ze_command_list_handle_t hCommandList, ze_image_handle_t hDstImage, const void *srcptr, const ze_image_region_t *pDstRegion, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendMemoryPrefetch(ze_command_list_handle_t hCommandList, const void *ptr, size_t size);
ze_result_t zeCommandListAppendMemAdvise(ze_command_list_handle_t hCommandList, ze_device_handle_t hDevice, const void *ptr, size_t size, ze_memory_advice_t advice);
ze_result_t zeCommandListAppendSignalEvent(ze_command_list_handle_t hCommandList, ze_event_handle_t hEvent);
ze_result_t zeCommandListAppendWaitOnEvents(ze_command_list_handle_t hCommandList, uint32_t numEvents, ze_event_handle_t *phEvents);
ze_result_t zeCommandListAppendEventReset(ze_command_list_handle_t hCommandList, ze_event_handle_t hEvent);
ze_result_t zeCommandListAppendQueryKernelTimestamps(ze_command_list_handle_t hCommandList, uint32_t numEvents, ze_event_handle_t *phEvents, void *dstptr, const size_t *pOffsets, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendLaunchKernel(ze_command_list_handle_t hCommandList, ze_kernel_handle_t kernelHandle, const ze_group_count_t *launchKernelArgs, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendLaunchCooperativeKernel(ze_command_list_handle_t hCommandList, ze_kernel_handle_t kernelHandle, const ze_group_count_t *launchKernelArgs, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendLaunchKernelIndirect(ze_command_list_handle_t hCommandList, ze_kernel_handle_t kernelHandle, const ze_group_count_t *pLaunchArgumentsBuffer, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendLaunchMultipleKernelsIndirect(ze_command_list_handle_t hCommandList, uint32_t numKernels, ze_kernel_handle_t *kernelHandles, const uint32_t *pCountBuffer, const ze_group_count_t *pLaunchArgumentsBuffer, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendSignalExternalSemaphoreExt(ze_command_list_handle_t hCommandList, uint32_t numSemaphores, ze_external_semaphore_ext_handle_t *phSemaphores, ze_external_semaphore_signal_params_ext_t *signalParams, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendWaitExternalSemaphoreExt(ze_command_list_handle_t hCommandList, uint32_t numSemaphores, ze_external_semaphore_ext_handle_t *phSemaphores, ze_external_semaphore_wait_params_ext_t *waitParams, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendImageCopyToMemoryExt(ze_command_list_handle_t hCommandList, void *dstptr, ze_image_handle_t hSrcImage, const ze_image_region_t *pSrcRegion, uint32_t destRowPitch, uint32_t destSlicePitch, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendImageCopyFromMemoryExt(ze_command_list_handle_t hCommandList, ze_image_handle_t hDstImage, const void *srcptr, const ze_image_region_t *pDstRegion, uint32_t srcRowPitch, uint32_t srcSlicePitch, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendLaunchKernelWithParameters(ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel, const ze_group_count_t *pGroupCounts, const void *pNext, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);
ze_result_t zeCommandListAppendLaunchKernelWithArguments(ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel, const ze_group_count_t groupCounts, const ze_group_size_t groupSizes, void **pArguments, const void *pNext, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents);

namespace ult {

struct GraphsCleanupGuard {
    ~GraphsCleanupGuard() {
        processUsesGraphs.store(false);
    }
};

struct MockGraphCmdListWithContext : Mock<CommandList> {
    MockGraphCmdListWithContext(L0::Context *ctx) : ctx(ctx) {}
    ze_result_t getContextHandle(ze_context_handle_t *phContext) override {
        *phContext = ctx;
        return ZE_RESULT_SUCCESS;
    }

    L0::Context *ctx = nullptr;
};

struct MockGraphContextReturningSpecificCmdList : Mock<Context> {
    Mock<CommandList> *cmdListToReturn = nullptr;
    ze_result_t createCommandList(ze_device_handle_t hDevice, const ze_command_list_desc_t *desc, ze_command_list_handle_t *commandList) override {
        *commandList = cmdListToReturn;
        cmdListToReturn = nullptr;
        return ZE_RESULT_SUCCESS;
    }

    MockGraphContextReturningSpecificCmdList() = default;
    MockGraphContextReturningSpecificCmdList(const MockGraphContextReturningSpecificCmdList &) = delete;
    MockGraphContextReturningSpecificCmdList &operator=(const MockGraphContextReturningSpecificCmdList &) = delete;
    MockGraphContextReturningSpecificCmdList(MockGraphContextReturningSpecificCmdList &&) = delete;
    MockGraphContextReturningSpecificCmdList &operator=(MockGraphContextReturningSpecificCmdList &&) = delete;
    ~MockGraphContextReturningSpecificCmdList() override {
        if (cmdListToReturn) {
            delete static_cast<L0::CommandList *>(cmdListToReturn);
        }
    }
};

struct MockGraphContextReturningNewCmdList : Mock<Context> {
    ze_result_t createCommandList(ze_device_handle_t hDevice, const ze_command_list_desc_t *desc, ze_command_list_handle_t *commandList) override {
        *commandList = new Mock<CommandList>;
        return ZE_RESULT_SUCCESS;
    }
};

} // namespace ult
} // namespace L0
