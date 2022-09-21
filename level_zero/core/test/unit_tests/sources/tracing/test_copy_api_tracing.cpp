/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_api_tracing_common.h"

namespace L0 {
namespace ult {

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListAppendMemoryCopyTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryCopy =
        [](ze_command_list_handle_t hCommandList,
           void *dstptr,
           const void *srcptr,
           size_t size,
           ze_event_handle_t hSignalEvent,
           uint32_t numWaitevents,
           ze_event_handle_t *phWaitEvents) { return ZE_RESULT_SUCCESS; };

    size_t bufferSize = 4096u;
    void *dst = malloc(bufferSize);
    void *src = malloc(bufferSize);

    prologCbs.CommandList.pfnAppendMemoryCopyCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendMemoryCopyCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendMemoryCopyTracing(nullptr, dst, static_cast<const void *>(src), bufferSize, nullptr, 0U, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);

    free(dst);
    free(src);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListAppendMemoryFillTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryFill =
        [](ze_command_list_handle_t hCommandList,
           void *ptr,
           const void *pattern,
           size_t patternSize,
           size_t size,
           ze_event_handle_t hSignalEvent,
           uint32_t numWaitEvents,
           ze_event_handle_t *phWaitEvents) { return ZE_RESULT_SUCCESS; };

    size_t bufferSize = 4096u;
    void *dst = malloc(bufferSize);
    int pattern = 1;

    prologCbs.CommandList.pfnAppendMemoryFillCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendMemoryFillCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendMemoryFillTracing(nullptr, dst, &pattern, sizeof(pattern), bufferSize, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);

    free(dst);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListAppendMemoryCopyRegionTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryCopyRegion =
        [](ze_command_list_handle_t hCommandList,
           void *dstptr,
           const ze_copy_region_t *dstRegion,
           uint32_t dstPitch,
           uint32_t dstSlicePitch,
           const void *srcptr,
           const ze_copy_region_t *srcRegion,
           uint32_t srcPitch,
           uint32_t srcSlicePitch,
           ze_event_handle_t hSignalEvent,
           uint32_t numWaitEvents,
           ze_event_handle_t *phWaitEvents) { return ZE_RESULT_SUCCESS; };

    size_t bufferSize = 4096u;
    void *dst = malloc(bufferSize);
    ze_copy_region_t dstRegion;
    uint32_t dstPitch = 1;
    void *src = malloc(bufferSize);
    ze_copy_region_t srcRegion;
    uint32_t srcPitch = 1;
    uint32_t dstSlicePitch = 0;
    uint32_t srcSlicePitch = 0;

    prologCbs.CommandList.pfnAppendMemoryCopyRegionCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendMemoryCopyRegionCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendMemoryCopyRegionTracing(nullptr, dst,
                                                        &dstRegion, dstPitch,
                                                        dstSlicePitch,
                                                        static_cast<const void *>(src),
                                                        &srcRegion, srcPitch,
                                                        srcSlicePitch,
                                                        nullptr,
                                                        0,
                                                        nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);

    free(dst);
    free(src);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListAppendImageCopyTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopy =
        [](ze_command_list_handle_t hCommandList,
           ze_image_handle_t hDstImage,
           ze_image_handle_t hSrcImage,
           ze_event_handle_t hSignalEvent,
           uint32_t numWaitEvents,
           ze_event_handle_t *phWaitEvents) { return ZE_RESULT_SUCCESS; };

    ze_image_handle_t hDstImage = static_cast<ze_image_handle_t>(malloc(1));
    ze_image_handle_t hSrcImage = static_cast<ze_image_handle_t>(malloc(1));

    prologCbs.CommandList.pfnAppendImageCopyCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendImageCopyCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendImageCopyTracing(nullptr, hDstImage, hSrcImage, nullptr, 0U, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);

    free(hDstImage);
    free(hSrcImage);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListAppendImageCopyRegionTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopyRegion =
        [](ze_command_list_handle_t hCommandList,
           ze_image_handle_t hDstImage,
           ze_image_handle_t hSrcImage,
           const ze_image_region_t *pDstRegion,
           const ze_image_region_t *pSrcRegion,
           ze_event_handle_t hSignalEvent,
           uint32_t numWaitEvents,
           ze_event_handle_t *phWaitEvents) { return ZE_RESULT_SUCCESS; };

    ze_image_handle_t hDstImage = static_cast<ze_image_handle_t>(malloc(1));
    ze_image_handle_t hSrcImage = static_cast<ze_image_handle_t>(malloc(1));

    prologCbs.CommandList.pfnAppendImageCopyRegionCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendImageCopyRegionCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendImageCopyRegionTracing(nullptr, hDstImage, hSrcImage, nullptr, nullptr, nullptr, 0U, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);

    free(hDstImage);
    free(hSrcImage);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListAppendImageCopyToMemoryTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopyToMemory =
        [](ze_command_list_handle_t hCommandList,
           void *dstptr,
           ze_image_handle_t hSrcImage,
           const ze_image_region_t *pSrcRegion,
           ze_event_handle_t hSignalEvent,
           uint32_t numWaitEvents,
           ze_event_handle_t *phWaitEvents) { return ZE_RESULT_SUCCESS; };

    ze_image_handle_t hSrcImage = static_cast<ze_image_handle_t>(malloc(1));
    void *dstptr = malloc(1);

    prologCbs.CommandList.pfnAppendImageCopyToMemoryCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendImageCopyToMemoryCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendImageCopyToMemoryTracing(nullptr, dstptr, hSrcImage, nullptr, nullptr, 0U, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);

    free(hSrcImage);
    free(dstptr);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListAppendImageCopyFromMemoryTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendImageCopyFromMemory =
        [](ze_command_list_handle_t hCommandList,
           ze_image_handle_t hDstImage,
           const void *srcptr,
           const ze_image_region_t *pDstRegion,
           ze_event_handle_t hSignalEvent,
           uint32_t numWaitEvents,
           ze_event_handle_t *phWaitEvents) { return ZE_RESULT_SUCCESS; };

    ze_image_handle_t hDstImage = static_cast<ze_image_handle_t>(malloc(1));
    void *srcptr = malloc(1);

    prologCbs.CommandList.pfnAppendImageCopyFromMemoryCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendImageCopyFromMemoryCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendImageCopyFromMemoryTracing(nullptr, hDstImage, srcptr, nullptr, nullptr, 0U, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);

    free(hDstImage);
    free(srcptr);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListAppendMemAdviseTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemAdvise =
        [](ze_command_list_handle_t hCommandList,
           ze_device_handle_t hDevice,
           const void *ptr,
           size_t size,
           ze_memory_advice_t advice) { return ZE_RESULT_SUCCESS; };
    size_t bufferSize = 4096u;
    void *ptr = malloc(bufferSize);

    ze_memory_advice_t advice = ZE_MEMORY_ADVICE_SET_READ_MOSTLY;

    prologCbs.CommandList.pfnAppendMemAdviseCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendMemAdviseCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendMemAdviseTracing(nullptr, nullptr, ptr, bufferSize, advice);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);

    free(ptr);
}

TEST_F(ZeApiTracingRuntimeTests, WhenCallingCommandListAppendMemoryCopyFromContextTracingWrapperWithOneSetOfPrologEpilogsThenReturnSuccess) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    driver_ddiTable.core_ddiTable.CommandList.pfnAppendMemoryCopyFromContext =
        [](ze_command_list_handle_t hCommandList,
           void *dstptr,
           ze_context_handle_t hContextSrc,
           const void *srcptr,
           size_t size,
           ze_event_handle_t hSignalEvent,
           uint32_t numWaitEvents,
           ze_event_handle_t *phWaitEvents) { return ZE_RESULT_SUCCESS; };

    prologCbs.CommandList.pfnAppendMemoryCopyFromContextCb = genericPrologCallbackPtr;
    epilogCbs.CommandList.pfnAppendMemoryCopyFromContextCb = genericEpilogCallbackPtr;

    setTracerCallbacksAndEnableTracer();

    result = zeCommandListAppendMemoryCopyFromContextTracing(nullptr, nullptr, nullptr, nullptr, 0U, nullptr, 1u, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(defaultUserData, 1);
}

} // namespace ult
} // namespace L0
