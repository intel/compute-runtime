/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "test.h"
#include <algorithm>

using namespace OCLRT;

struct CopyBufferRectHw
    : public CommandEnqueueAUBFixture,
      public ::testing::TestWithParam<std::tuple<size_t, size_t, size_t, size_t, size_t, size_t, bool>> {

    void SetUp() override {
        CommandEnqueueAUBFixture::SetUp();
        std::tie(srcOrigin0, srcOrigin1, srcOrigin2, dstOrigin0, dstOrigin1, dstOrigin2, copy3D) = GetParam();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::TearDown();
    }

    size_t srcOrigin0;
    size_t srcOrigin1;
    size_t srcOrigin2;
    size_t dstOrigin0;
    size_t dstOrigin1;
    size_t dstOrigin2;
    bool copy3D;
};

typedef CopyBufferRectHw AUBCopyBufferRect;

HWTEST_P(AUBCopyBufferRect, simple) {
    //3D UINT8 buffer 20x20x20
    static const size_t rowPitch = 20;
    static const size_t slicePitch = rowPitch * rowPitch;
    static const size_t elementCount = slicePitch * rowPitch;
    MockContext context(this->pDevice);

    cl_uchar *srcMemory = new uint8_t[elementCount + 8];
    cl_uchar *dstMemory = new uint8_t[elementCount + 8];

    for (size_t i = 0; i < elementCount; i++) {
        srcMemory[i] = static_cast<cl_uchar>(i + 1);
        dstMemory[i] = 0;
    }

    auto retVal = CL_INVALID_VALUE;

    auto srcBuffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        elementCount * sizeof(uint8_t),
        srcMemory,
        retVal);
    ASSERT_NE(nullptr, srcBuffer);

    auto dstBuffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        elementCount * sizeof(uint8_t),
        dstMemory,
        retVal);
    ASSERT_NE(nullptr, dstBuffer);

    auto pSrcMemory = &srcMemory[0];

    auto pDestMemory = (cl_uchar *)(dstBuffer->getGraphicsAllocation()->getGpuAddress());

    size_t regionX = std::min(rowPitch / 2, rowPitch - std::max(srcOrigin0, dstOrigin0));
    size_t regionY = std::min(rowPitch / 2, rowPitch - std::max(srcOrigin1, dstOrigin1));
    size_t regionZ = copy3D ? std::min(rowPitch / 2, rowPitch - std::max(srcOrigin2, dstOrigin2)) : 1;

    size_t srcOrigin[] = {srcOrigin0, srcOrigin1, srcOrigin2};
    size_t dstOrigin[] = {dstOrigin0, dstOrigin1, dstOrigin2};
    size_t region[] = {regionX, regionY, regionZ};

    retVal = pCmdQ->enqueueCopyBufferRect(
        srcBuffer,
        dstBuffer,
        srcOrigin,
        dstOrigin,
        region,
        rowPitch,
        slicePitch,
        rowPitch,
        slicePitch,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pCmdQ->flush();

    // Verify Output, line by line
    uint8_t src[rowPitch * slicePitch];
    memset(src, 0, sizeof(src));

    auto tDst = pDestMemory;
    auto tSrc = ptrOffset(pSrcMemory, srcOrigin[0] + srcOrigin[1] * rowPitch + srcOrigin[2] * slicePitch);
    auto tRef = ptrOffset(src, dstOrigin[0] + dstOrigin[1] * rowPitch + dstOrigin[2] * slicePitch);

    for (unsigned int z = 0; z < regionZ; z++) {
        auto pDst = tDst;
        auto pSrc = tSrc;
        auto pRef = tRef;

        for (unsigned int y = 0; y < regionY; y++) {
            memcpy(pRef, pSrc, region[0]);

            pDst += rowPitch;
            pSrc += rowPitch;
            pRef += rowPitch;
        }
        tDst += slicePitch;
        tSrc += slicePitch;
        tRef += slicePitch;
    }
    AUBCommandStreamFixture::expectMemory<FamilyType>(pDestMemory, src, rowPitch * slicePitch);

    delete srcBuffer;
    delete dstBuffer;

    delete[] srcMemory;
    delete[] dstMemory;
}

static size_t zero[] = {0};

INSTANTIATE_TEST_CASE_P(AUBCopyBufferRect,
                        AUBCopyBufferRect,
                        ::testing::Combine(
                            ::testing::Values(0, 3), //srcOrigin
                            ::testing::ValuesIn(zero),
                            ::testing::Values(0, 7),
                            ::testing::Values(0, 3), //dstPrigin
                            ::testing::ValuesIn(zero),
                            ::testing::Values(0, 7),
                            ::testing::Values(true, false)));
