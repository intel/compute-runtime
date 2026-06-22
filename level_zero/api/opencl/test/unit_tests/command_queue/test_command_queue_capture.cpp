/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/event/event.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/mem_obj/buffer.h"
#include "level_zero/api/opencl/test/common/fixtures/leo_capture_fixture.h"

#include "CL/cl.h"

#include <array>
#include <cstring>

namespace NEO {
namespace LEO {
namespace ult {

using LeoCommandQueueCaptureTest = Test<LeoCaptureFixture>;

TEST_F(LeoCommandQueueCaptureTest, givenNoEnqueueWhenInspectingCaptureThenNoCommandsRecorded) {
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
    EXPECT_FALSE(capturingCmdList.appendMemoryCopyArgs.wasCalled());
    EXPECT_TRUE(capturingCmdList.sequence.empty());
}

TEST_F(LeoCommandQueueCaptureTest, givenWriteBufferWhenEnqueuedThenSingleMemoryCopyCapturedWithMatchingArguments) {
    constexpr size_t bufferSize = 64u;
    constexpr size_t offset = 16u;
    constexpr size_t copySize = 32u;
    auto buffer = createBuffer(bufferSize);
    auto pBuffer = castToObject<Buffer>(buffer);
    ASSERT_NE(nullptr, pBuffer);

    std::array<uint8_t, copySize> hostData{};

    auto retVal = clEnqueueWriteBuffer(getCommandQueue(), buffer, CL_FALSE, offset, copySize, hostData.data(), 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyArgs.count());
    EXPECT_EQ(1u, capturingCmdList.totalCalls());

    const auto &params = capturingCmdList.appendMemoryCopyArgs[0];
    EXPECT_EQ(ptrOffset(pBuffer->getUsmPtr(), offset), params.dstptr);
    EXPECT_EQ(hostData.data(), params.srcptr);
    EXPECT_EQ(copySize, params.size);
    EXPECT_EQ(nullptr, params.signalEvent);
    EXPECT_TRUE(params.waitEvents.empty());

    clReleaseMemObject(buffer);
}

TEST_F(LeoCommandQueueCaptureTest, givenReadBufferWhenEnqueuedThenMemoryCopyCapturedWithReversedDirection) {
    constexpr size_t bufferSize = 64u;
    constexpr size_t offset = 8u;
    constexpr size_t copySize = 24u;
    auto buffer = createBuffer(bufferSize);
    auto pBuffer = castToObject<Buffer>(buffer);
    ASSERT_NE(nullptr, pBuffer);

    std::array<uint8_t, copySize> hostData{};

    auto retVal = clEnqueueReadBuffer(getCommandQueue(), buffer, CL_FALSE, offset, copySize, hostData.data(), 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyArgs.count());
    const auto &params = capturingCmdList.appendMemoryCopyArgs[0];
    EXPECT_EQ(hostData.data(), params.dstptr);
    EXPECT_EQ(ptrOffset(pBuffer->getUsmPtr(), offset), params.srcptr);
    EXPECT_EQ(copySize, params.size);

    clReleaseMemObject(buffer);
}

TEST_F(LeoCommandQueueCaptureTest, givenCopyBufferWhenEnqueuedThenMemoryCopyCapturedBetweenUsmPointers) {
    constexpr size_t bufferSize = 128u;
    constexpr size_t srcOffset = 16u;
    constexpr size_t dstOffset = 48u;
    constexpr size_t copySize = 32u;
    auto srcBuffer = createBuffer(bufferSize);
    auto dstBuffer = createBuffer(bufferSize);
    auto pSrc = castToObject<Buffer>(srcBuffer);
    auto pDst = castToObject<Buffer>(dstBuffer);
    ASSERT_NE(nullptr, pSrc);
    ASSERT_NE(nullptr, pDst);

    auto retVal = clEnqueueCopyBuffer(getCommandQueue(), srcBuffer, dstBuffer, srcOffset, dstOffset, copySize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyArgs.count());
    const auto &params = capturingCmdList.appendMemoryCopyArgs[0];
    EXPECT_EQ(ptrOffset(pDst->getUsmPtr(), dstOffset), params.dstptr);
    EXPECT_EQ(ptrOffset(pSrc->getUsmPtr(), srcOffset), params.srcptr);
    EXPECT_EQ(copySize, params.size);

    clReleaseMemObject(srcBuffer);
    clReleaseMemObject(dstBuffer);
}

TEST_F(LeoCommandQueueCaptureTest, givenFillBufferWhenEnqueuedThenMemoryFillCapturedWithPatternBytes) {
    constexpr size_t bufferSize = 64u;
    constexpr size_t offset = 8u;
    constexpr size_t fillSize = 32u;
    auto buffer = createBuffer(bufferSize);
    auto pBuffer = castToObject<Buffer>(buffer);
    ASSERT_NE(nullptr, pBuffer);

    const uint32_t patternValue = 0xABCDEF01u;

    auto retVal = clEnqueueFillBuffer(getCommandQueue(), buffer, &patternValue, sizeof(patternValue), offset, fillSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    ASSERT_EQ(1u, capturingCmdList.appendMemoryFillArgs.count());
    const auto &params = capturingCmdList.appendMemoryFillArgs[0];
    EXPECT_EQ(ptrOffset(pBuffer->getUsmPtr(), offset), params.ptr);
    EXPECT_EQ(sizeof(patternValue), params.patternSize);
    EXPECT_EQ(fillSize, params.size);
    ASSERT_EQ(sizeof(patternValue), params.pattern.size());
    EXPECT_EQ(0, memcmp(params.pattern.data(), &patternValue, sizeof(patternValue)));

    clReleaseMemObject(buffer);
}

TEST_F(LeoCommandQueueCaptureTest, givenBlockingWriteBufferWhenEnqueuedThenMemoryCopyAndHostSynchronizeCapturedInOrder) {
    constexpr size_t bufferSize = 64u;
    auto buffer = createBuffer(bufferSize);
    std::array<uint8_t, bufferSize> hostData{};

    auto retVal = clEnqueueWriteBuffer(getCommandQueue(), buffer, CL_TRUE, 0, bufferSize, hostData.data(), 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, capturingCmdList.appendMemoryCopyArgs.count());
    EXPECT_EQ(1u, capturingCmdList.hostSynchronizeArgs.count());

    ASSERT_EQ(2u, capturingCmdList.sequence.size());
    EXPECT_EQ(ApiId::appendMemoryCopy, capturingCmdList.sequence[0]);
    EXPECT_EQ(ApiId::hostSynchronize, capturingCmdList.sequence[1]);

    clReleaseMemObject(buffer);
}

TEST_F(LeoCommandQueueCaptureTest, givenBarrierWithWaitListWhenEnqueuedThenAppendBarrierCapturedWithWaitEventHandles) {
    cl_int errcode = CL_SUCCESS;
    auto userEvent = clCreateUserEvent(clContext, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    auto pUserEvent = castToObject<Event>(userEvent);
    ASSERT_NE(nullptr, pUserEvent);

    cl_event outEvent = nullptr;
    auto retVal = clEnqueueBarrierWithWaitList(getCommandQueue(), 1, &userEvent, &outEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    ASSERT_EQ(1u, capturingCmdList.appendBarrierArgs.count());
    const auto &params = capturingCmdList.appendBarrierArgs[0];
    ASSERT_EQ(1u, params.waitEvents.size());
    EXPECT_EQ(pUserEvent->getL0Handle(), params.waitEvents[0]);
    EXPECT_NE(nullptr, params.signalEvent);
    EXPECT_EQ(castToObject<Event>(outEvent)->getL0Handle(), params.signalEvent);

    clReleaseEvent(outEvent);
    clReleaseEvent(userEvent);
}

TEST_F(LeoCommandQueueCaptureTest, givenMarkerWithoutWaitListWhenEnqueuedThenAppendBarrierCapturedWithoutWaitEvents) {
    auto retVal = clEnqueueMarkerWithWaitList(getCommandQueue(), 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    ASSERT_EQ(1u, capturingCmdList.appendBarrierArgs.count());
    const auto &params = capturingCmdList.appendBarrierArgs[0];
    EXPECT_TRUE(params.waitEvents.empty());
    EXPECT_EQ(nullptr, params.signalEvent);
}

TEST_F(LeoCommandQueueCaptureTest, givenMultipleEnqueuesWhenInspectingSequenceThenAllCallsCapturedInIssueOrder) {
    constexpr size_t bufferSize = 64u;
    auto buffer = createBuffer(bufferSize);
    std::array<uint8_t, bufferSize> hostData{};
    const uint32_t patternValue = 0x5Au;

    EXPECT_EQ(CL_SUCCESS, clEnqueueWriteBuffer(getCommandQueue(), buffer, CL_FALSE, 0, bufferSize, hostData.data(), 0, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, clEnqueueFillBuffer(getCommandQueue(), buffer, &patternValue, sizeof(patternValue), 0, bufferSize, 0, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, clEnqueueBarrierWithWaitList(getCommandQueue(), 0, nullptr, nullptr));

    ASSERT_EQ(3u, capturingCmdList.sequence.size());
    EXPECT_EQ(ApiId::appendMemoryCopy, capturingCmdList.sequence[0]);
    EXPECT_EQ(ApiId::appendMemoryFill, capturingCmdList.sequence[1]);
    EXPECT_EQ(ApiId::appendBarrier, capturingCmdList.sequence[2]);

    clReleaseMemObject(buffer);
}

TEST_F(LeoCommandQueueCaptureTest, givenRepeatedEnqueueWhenInspectingMemberThenEachInvocationAccessibleByIndex) {
    constexpr size_t bufferSize = 64u;
    auto buffer = createBuffer(bufferSize);
    std::array<uint8_t, bufferSize> hostData{};

    EXPECT_EQ(CL_SUCCESS, clEnqueueWriteBuffer(getCommandQueue(), buffer, CL_FALSE, 0, bufferSize, hostData.data(), 0, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, clEnqueueReadBuffer(getCommandQueue(), buffer, CL_FALSE, 0, bufferSize, hostData.data(), 0, nullptr, nullptr));

    auto &memoryCopies = capturingCmdList.appendMemoryCopyArgs;
    ASSERT_EQ(2u, memoryCopies.count());
    EXPECT_EQ(hostData.data(), memoryCopies[0].srcptr); // write: host is source
    EXPECT_EQ(hostData.data(), memoryCopies[1].dstptr); // read: host is destination
    EXPECT_FALSE(capturingCmdList.appendMemoryFillArgs.wasCalled());

    clReleaseMemObject(buffer);
}

TEST_F(LeoCommandQueueCaptureTest, givenClearCapturesWhenCalledThenPreviouslyRecordedCommandsDiscarded) {
    constexpr size_t bufferSize = 64u;
    auto buffer = createBuffer(bufferSize);
    std::array<uint8_t, bufferSize> hostData{};

    EXPECT_EQ(CL_SUCCESS, clEnqueueWriteBuffer(getCommandQueue(), buffer, CL_FALSE, 0, bufferSize, hostData.data(), 0, nullptr, nullptr));
    ASSERT_EQ(1u, capturingCmdList.totalCalls());

    capturingCmdList.clearCaptures();
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
    EXPECT_FALSE(capturingCmdList.appendMemoryCopyArgs.wasCalled());

    EXPECT_EQ(CL_SUCCESS, clEnqueueReadBuffer(getCommandQueue(), buffer, CL_FALSE, 0, bufferSize, hostData.data(), 0, nullptr, nullptr));
    EXPECT_EQ(1u, capturingCmdList.appendMemoryCopyArgs.count());

    clReleaseMemObject(buffer);
}

TEST_F(LeoCommandQueueCaptureTest, givenCopyBufferRectWithZeroPitchesWhenEnqueuedThenSrcAndDstPitchesDefaultToTightlyPacked) {
    constexpr size_t rowBytes = 16u;
    constexpr size_t numRows = 4u;
    constexpr size_t numSlices = 4u;
    constexpr size_t bufferSize = rowBytes * numRows * numSlices;
    auto srcBuffer = createBuffer(bufferSize);
    auto dstBuffer = createBuffer(bufferSize);

    const size_t origin[3] = {0u, 0u, 0u};
    const size_t region[3] = {rowBytes, numRows, numSlices};

    EXPECT_EQ(CL_SUCCESS, clEnqueueCopyBufferRect(getCommandQueue(), srcBuffer, dstBuffer, origin, origin, region,
                                                  0u, 0u, 0u, 0u, 0, nullptr, nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyRegionArgs.count());
    const auto &copy = capturingCmdList.appendMemoryCopyRegionArgs[0];
    EXPECT_EQ(rowBytes, copy.srcPitch);
    EXPECT_EQ(rowBytes * numRows, copy.srcSlicePitch);
    EXPECT_EQ(rowBytes, copy.dstPitch);
    EXPECT_EQ(rowBytes * numRows, copy.dstSlicePitch);

    clReleaseMemObject(srcBuffer);
    clReleaseMemObject(dstBuffer);
}

TEST_F(LeoCommandQueueCaptureTest, givenReadBufferRectWithNonZeroRowPitchAndZeroSlicePitchWhenEnqueuedThenSlicePitchDerivesFromGivenRowPitch) {
    constexpr size_t rowBytes = 16u;
    constexpr size_t numRows = 4u;
    constexpr size_t hostRowPitch = rowBytes + 8u;
    auto buffer = createBuffer(rowBytes * numRows);
    std::array<uint8_t, hostRowPitch * numRows> hostData{};

    const size_t origin[3] = {0u, 0u, 0u};
    const size_t region[3] = {rowBytes, numRows, 1u};

    EXPECT_EQ(CL_SUCCESS, clEnqueueReadBufferRect(getCommandQueue(), buffer, CL_FALSE, origin, origin, region,
                                                  0u, 0u, hostRowPitch, 0u, hostData.data(), 0, nullptr, nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyRegionArgs.count());
    const auto &copy = capturingCmdList.appendMemoryCopyRegionArgs[0];
    EXPECT_EQ(rowBytes, copy.srcPitch);
    EXPECT_EQ(rowBytes * numRows, copy.srcSlicePitch);
    EXPECT_EQ(hostRowPitch, copy.dstPitch);
    EXPECT_EQ(hostRowPitch * numRows, copy.dstSlicePitch);

    clReleaseMemObject(buffer);
}

TEST_F(LeoCommandQueueCaptureTest, givenCommandListReturningErrorWhenWriteBufferEnqueuedThenErrorMappedAndCallStillCaptured) {
    constexpr size_t bufferSize = 64u;
    auto buffer = createBuffer(bufferSize);
    std::array<uint8_t, bufferSize> hostData{};

    capturingCmdList.appendMemoryCopyResult = ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;

    auto retVal = clEnqueueWriteBuffer(getCommandQueue(), buffer, CL_FALSE, 0, bufferSize, hostData.data(), 0, nullptr, nullptr);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
    EXPECT_EQ(1u, capturingCmdList.appendMemoryCopyArgs.count());

    clReleaseMemObject(buffer);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
