/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/test/common/fixtures/leo_capture_fixture.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <array>

namespace NEO {
namespace LEO {
namespace ult {

struct EnqueueRectFixture : public Test<LeoCaptureFixture> {
    void SetUp() override {
        Test<LeoCaptureFixture>::SetUp();
        buffer = createBuffer(bufferSize);
        ASSERT_NE(nullptr, buffer);
        pBuffer = castToObject<Buffer>(buffer);
        ASSERT_NE(nullptr, pBuffer);
    }

    void TearDown() override {
        if (secondBuffer != nullptr) {
            clReleaseMemObject(secondBuffer);
        }
        if (buffer != nullptr) {
            clReleaseMemObject(buffer);
        }
        Test<LeoCaptureFixture>::TearDown();
    }

    cl_mem createSecondBuffer(cl_mem_flags flags = CL_MEM_READ_WRITE) {
        secondBuffer = createBuffer(bufferSize, flags);
        return secondBuffer;
    }

    static void expectRegion(const ze_copy_region_t &region, uint32_t originX, uint32_t originY, uint32_t originZ,
                             uint32_t width, uint32_t height, uint32_t depth) {
        EXPECT_EQ(originX, region.originX);
        EXPECT_EQ(originY, region.originY);
        EXPECT_EQ(originZ, region.originZ);
        EXPECT_EQ(width, region.width);
        EXPECT_EQ(height, region.height);
        EXPECT_EQ(depth, region.depth);
    }

    static constexpr size_t bufferSize = 4096u;

    size_t bufferOrigin[3] = {4u, 5u, 6u};
    size_t hostOrigin[3] = {1u, 2u, 3u};
    size_t region[3] = {8u, 4u, 2u};

    cl_mem buffer = nullptr;
    cl_mem secondBuffer = nullptr;
    Buffer *pBuffer = nullptr;
    std::array<uint8_t, bufferSize> hostData{};
};

TEST_F(EnqueueRectFixture, givenReadBufferRectWhenEnqueuedThenBufferIsSourceAndHostIsDestination) {
    ASSERT_EQ(CL_SUCCESS, clEnqueueReadBufferRect(getCommandQueue(), buffer, CL_FALSE, bufferOrigin, hostOrigin, region,
                                                  0, 0, 0, 0, hostData.data(), 0, nullptr, nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyRegionArgs.count());
    const auto &params = capturingCmdList.appendMemoryCopyRegionArgs[0];

    EXPECT_EQ(hostData.data(), params.dstptr);
    EXPECT_EQ(pBuffer->getUsmPtr(), params.srcptr);

    ASSERT_TRUE(params.dstRegion.has_value());
    ASSERT_TRUE(params.srcRegion.has_value());
    expectRegion(*params.dstRegion, 1u, 2u, 3u, 8u, 4u, 2u);
    expectRegion(*params.srcRegion, 4u, 5u, 6u, 8u, 4u, 2u);
}

TEST_F(EnqueueRectFixture, givenWriteBufferRectWhenEnqueuedThenBufferIsDestinationAndHostIsSource) {
    ASSERT_EQ(CL_SUCCESS, clEnqueueWriteBufferRect(getCommandQueue(), buffer, CL_FALSE, bufferOrigin, hostOrigin, region,
                                                   0, 0, 0, 0, hostData.data(), 0, nullptr, nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyRegionArgs.count());
    const auto &params = capturingCmdList.appendMemoryCopyRegionArgs[0];

    EXPECT_EQ(pBuffer->getUsmPtr(), params.dstptr);
    EXPECT_EQ(hostData.data(), params.srcptr);

    ASSERT_TRUE(params.dstRegion.has_value());
    ASSERT_TRUE(params.srcRegion.has_value());
    expectRegion(*params.dstRegion, 4u, 5u, 6u, 8u, 4u, 2u);
    expectRegion(*params.srcRegion, 1u, 2u, 3u, 8u, 4u, 2u);
}

TEST_F(EnqueueRectFixture, givenReadBufferRectWithExplicitPitchesWhenEnqueuedThenHostPitchesGoToDestinationSide) {
    constexpr size_t bufferRowPitch = 64u;
    constexpr size_t bufferSlicePitch = 512u;
    constexpr size_t hostRowPitch = 32u;
    constexpr size_t hostSlicePitch = 256u;

    ASSERT_EQ(CL_SUCCESS, clEnqueueReadBufferRect(getCommandQueue(), buffer, CL_FALSE, bufferOrigin, hostOrigin, region,
                                                  bufferRowPitch, bufferSlicePitch, hostRowPitch, hostSlicePitch,
                                                  hostData.data(), 0, nullptr, nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyRegionArgs.count());
    const auto &params = capturingCmdList.appendMemoryCopyRegionArgs[0];

    EXPECT_EQ(static_cast<uint32_t>(hostRowPitch), params.dstPitch);
    EXPECT_EQ(static_cast<uint32_t>(hostSlicePitch), params.dstSlicePitch);
    EXPECT_EQ(static_cast<uint32_t>(bufferRowPitch), params.srcPitch);
    EXPECT_EQ(static_cast<uint32_t>(bufferSlicePitch), params.srcSlicePitch);
}

TEST_F(EnqueueRectFixture, givenWriteBufferRectWithExplicitPitchesWhenEnqueuedThenBufferPitchesGoToDestinationSide) {
    constexpr size_t bufferRowPitch = 64u;
    constexpr size_t bufferSlicePitch = 512u;
    constexpr size_t hostRowPitch = 32u;
    constexpr size_t hostSlicePitch = 256u;

    ASSERT_EQ(CL_SUCCESS, clEnqueueWriteBufferRect(getCommandQueue(), buffer, CL_FALSE, bufferOrigin, hostOrigin, region,
                                                   bufferRowPitch, bufferSlicePitch, hostRowPitch, hostSlicePitch,
                                                   hostData.data(), 0, nullptr, nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyRegionArgs.count());
    const auto &params = capturingCmdList.appendMemoryCopyRegionArgs[0];

    EXPECT_EQ(static_cast<uint32_t>(bufferRowPitch), params.dstPitch);
    EXPECT_EQ(static_cast<uint32_t>(bufferSlicePitch), params.dstSlicePitch);
    EXPECT_EQ(static_cast<uint32_t>(hostRowPitch), params.srcPitch);
    EXPECT_EQ(static_cast<uint32_t>(hostSlicePitch), params.srcSlicePitch);
}

TEST_F(EnqueueRectFixture, givenReadBufferRectWithOnlyBufferPitchesWhenEnqueuedThenHostPitchesDefaultIndependently) {
    constexpr size_t bufferRowPitch = 64u;
    constexpr size_t bufferSlicePitch = 512u;

    ASSERT_EQ(CL_SUCCESS, clEnqueueReadBufferRect(getCommandQueue(), buffer, CL_FALSE, bufferOrigin, hostOrigin, region,
                                                  bufferRowPitch, bufferSlicePitch, 0, 0,
                                                  hostData.data(), 0, nullptr, nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyRegionArgs.count());
    const auto &params = capturingCmdList.appendMemoryCopyRegionArgs[0];

    EXPECT_EQ(static_cast<uint32_t>(region[0]), params.dstPitch);
    EXPECT_EQ(static_cast<uint32_t>(region[0] * region[1]), params.dstSlicePitch);
    EXPECT_EQ(static_cast<uint32_t>(bufferRowPitch), params.srcPitch);
    EXPECT_EQ(static_cast<uint32_t>(bufferSlicePitch), params.srcSlicePitch);
}

TEST_F(EnqueueRectFixture, givenReadBufferRectWithRowPitchOnlyWhenEnqueuedThenSlicePitchDerivesFromThatRowPitch) {
    constexpr size_t bufferRowPitch = 64u;
    constexpr size_t hostRowPitch = 40u;

    ASSERT_EQ(CL_SUCCESS, clEnqueueReadBufferRect(getCommandQueue(), buffer, CL_FALSE, bufferOrigin, hostOrigin, region,
                                                  bufferRowPitch, 0, hostRowPitch, 0,
                                                  hostData.data(), 0, nullptr, nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyRegionArgs.count());
    const auto &params = capturingCmdList.appendMemoryCopyRegionArgs[0];

    EXPECT_EQ(static_cast<uint32_t>(hostRowPitch), params.dstPitch);
    EXPECT_EQ(static_cast<uint32_t>(region[1] * hostRowPitch), params.dstSlicePitch);
    EXPECT_EQ(static_cast<uint32_t>(bufferRowPitch), params.srcPitch);
    EXPECT_EQ(static_cast<uint32_t>(region[1] * bufferRowPitch), params.srcSlicePitch);
}

TEST_F(EnqueueRectFixture, givenCopyBufferRectWhenEnqueuedThenBothSidesUseBufferUsmPointers) {
    auto dstBuffer = createSecondBuffer();
    auto pDstBuffer = castToObject<Buffer>(dstBuffer);
    ASSERT_NE(nullptr, pDstBuffer);

    size_t srcOrigin[3] = {4u, 5u, 6u};
    size_t dstOrigin[3] = {7u, 8u, 9u};

    ASSERT_EQ(CL_SUCCESS, clEnqueueCopyBufferRect(getCommandQueue(), buffer, dstBuffer, srcOrigin, dstOrigin, region,
                                                  0, 0, 0, 0, 0, nullptr, nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyRegionArgs.count());
    const auto &params = capturingCmdList.appendMemoryCopyRegionArgs[0];

    EXPECT_EQ(pDstBuffer->getUsmPtr(), params.dstptr);
    EXPECT_EQ(pBuffer->getUsmPtr(), params.srcptr);

    ASSERT_TRUE(params.dstRegion.has_value());
    ASSERT_TRUE(params.srcRegion.has_value());
    expectRegion(*params.dstRegion, 7u, 8u, 9u, 8u, 4u, 2u);
    expectRegion(*params.srcRegion, 4u, 5u, 6u, 8u, 4u, 2u);
}

TEST_F(EnqueueRectFixture, givenCopyBufferRectWithDistinctPitchesWhenEnqueuedThenPitchesLandOnMatchingSides) {
    auto dstBuffer = createSecondBuffer();
    constexpr size_t srcRowPitch = 64u;
    constexpr size_t srcSlicePitch = 1024u;
    constexpr size_t dstRowPitch = 48u;
    constexpr size_t dstSlicePitch = 768u;

    ASSERT_EQ(CL_SUCCESS, clEnqueueCopyBufferRect(getCommandQueue(), buffer, dstBuffer, bufferOrigin, hostOrigin, region,
                                                  srcRowPitch, srcSlicePitch, dstRowPitch, dstSlicePitch,
                                                  0, nullptr, nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyRegionArgs.count());
    const auto &params = capturingCmdList.appendMemoryCopyRegionArgs[0];

    EXPECT_EQ(static_cast<uint32_t>(dstRowPitch), params.dstPitch);
    EXPECT_EQ(static_cast<uint32_t>(dstSlicePitch), params.dstSlicePitch);
    EXPECT_EQ(static_cast<uint32_t>(srcRowPitch), params.srcPitch);
    EXPECT_EQ(static_cast<uint32_t>(srcSlicePitch), params.srcSlicePitch);
}

TEST_F(EnqueueRectFixture, givenCopyBufferRectWhenEnqueuedThenNoHostSynchronizeIsIssued) {
    auto dstBuffer = createSecondBuffer();

    ASSERT_EQ(CL_SUCCESS, clEnqueueCopyBufferRect(getCommandQueue(), buffer, dstBuffer, bufferOrigin, hostOrigin, region,
                                                  0, 0, 0, 0, 0, nullptr, nullptr));

    EXPECT_FALSE(capturingCmdList.hostSynchronizeArgs.wasCalled());
    ASSERT_EQ(1u, capturingCmdList.sequence.size());
    EXPECT_EQ(ApiId::appendMemoryCopyRegion, capturingCmdList.sequence[0]);
}

TEST_F(EnqueueRectFixture, givenBlockingWriteBufferRectWhenEnqueuedThenHostSynchronizeFollowsTheCopy) {
    ASSERT_EQ(CL_SUCCESS, clEnqueueWriteBufferRect(getCommandQueue(), buffer, CL_TRUE, bufferOrigin, hostOrigin, region,
                                                   0, 0, 0, 0, hostData.data(), 0, nullptr, nullptr));

    ASSERT_EQ(2u, capturingCmdList.sequence.size());
    EXPECT_EQ(ApiId::appendMemoryCopyRegion, capturingCmdList.sequence[0]);
    EXPECT_EQ(ApiId::hostSynchronize, capturingCmdList.sequence[1]);
}

TEST_F(EnqueueRectFixture, givenBlockingReadBufferRectWhenEnqueuedThenHostSynchronizeFollowsTheCopy) {
    ASSERT_EQ(CL_SUCCESS, clEnqueueReadBufferRect(getCommandQueue(), buffer, CL_TRUE, bufferOrigin, hostOrigin, region,
                                                  0, 0, 0, 0, hostData.data(), 0, nullptr, nullptr));

    ASSERT_EQ(2u, capturingCmdList.sequence.size());
    EXPECT_EQ(ApiId::appendMemoryCopyRegion, capturingCmdList.sequence[0]);
    EXPECT_EQ(ApiId::hostSynchronize, capturingCmdList.sequence[1]);
}

TEST_F(EnqueueRectFixture, givenZeroRegionWhenReadBufferRectEnqueuedThenPitchesCollapseToZero) {
    size_t emptyRegion[3] = {0u, 0u, 0u};

    ASSERT_EQ(CL_SUCCESS, clEnqueueReadBufferRect(getCommandQueue(), buffer, CL_FALSE, bufferOrigin, hostOrigin,
                                                  emptyRegion, 0, 0, 0, 0, hostData.data(), 0, nullptr, nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendMemoryCopyRegionArgs.count());
    const auto &params = capturingCmdList.appendMemoryCopyRegionArgs[0];

    EXPECT_EQ(0u, params.dstPitch);
    EXPECT_EQ(0u, params.dstSlicePitch);
    EXPECT_EQ(0u, params.srcPitch);
    EXPECT_EQ(0u, params.srcSlicePitch);
    expectRegion(*params.dstRegion, 1u, 2u, 3u, 0u, 0u, 0u);
}

TEST_F(EnqueueRectFixture, givenHostWriteOnlyBufferWhenReadBufferRectEnqueuedThenInvalidOperationAndNothingCaptured) {
    auto restrictedBuffer = createSecondBuffer(CL_MEM_READ_WRITE | CL_MEM_HOST_WRITE_ONLY);

    EXPECT_EQ(CL_INVALID_OPERATION, clEnqueueReadBufferRect(getCommandQueue(), restrictedBuffer, CL_FALSE, bufferOrigin,
                                                            hostOrigin, region, 0, 0, 0, 0, hostData.data(), 0, nullptr, nullptr));

    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueRectFixture, givenHostNoAccessBufferWhenReadBufferRectEnqueuedThenInvalidOperationAndNothingCaptured) {
    auto restrictedBuffer = createSecondBuffer(CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS);

    EXPECT_EQ(CL_INVALID_OPERATION, clEnqueueReadBufferRect(getCommandQueue(), restrictedBuffer, CL_FALSE, bufferOrigin,
                                                            hostOrigin, region, 0, 0, 0, 0, hostData.data(), 0, nullptr, nullptr));

    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueRectFixture, givenHostReadOnlyBufferWhenWriteBufferRectEnqueuedThenInvalidOperationAndNothingCaptured) {
    auto restrictedBuffer = createSecondBuffer(CL_MEM_READ_WRITE | CL_MEM_HOST_READ_ONLY);

    EXPECT_EQ(CL_INVALID_OPERATION, clEnqueueWriteBufferRect(getCommandQueue(), restrictedBuffer, CL_FALSE, bufferOrigin,
                                                             hostOrigin, region, 0, 0, 0, 0, hostData.data(), 0, nullptr, nullptr));

    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueRectFixture, givenHostNoAccessBufferWhenWriteBufferRectEnqueuedThenInvalidOperationAndNothingCaptured) {
    auto restrictedBuffer = createSecondBuffer(CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS);

    EXPECT_EQ(CL_INVALID_OPERATION, clEnqueueWriteBufferRect(getCommandQueue(), restrictedBuffer, CL_FALSE, bufferOrigin,
                                                             hostOrigin, region, 0, 0, 0, 0, hostData.data(), 0, nullptr, nullptr));

    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueRectFixture, givenNullHostPointerWhenReadBufferRectEnqueuedThenInvalidValueAndNothingCaptured) {
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueReadBufferRect(getCommandQueue(), buffer, CL_FALSE, bufferOrigin, hostOrigin,
                                                        region, 0, 0, 0, 0, nullptr, 0, nullptr, nullptr));

    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueRectFixture, givenNullBufferWhenCopyBufferRectEnqueuedThenInvalidMemObjectAndNothingCaptured) {
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, clEnqueueCopyBufferRect(getCommandQueue(), buffer, nullptr, bufferOrigin,
                                                             hostOrigin, region, 0, 0, 0, 0, 0, nullptr, nullptr));

    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

} // namespace ult
} // namespace LEO
} // namespace NEO
