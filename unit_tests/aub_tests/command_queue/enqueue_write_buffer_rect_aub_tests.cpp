/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/ptr_math.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/options.h"
#include "runtime/mem_obj/buffer.h"
#include "test.h"
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/mocks/mock_context.h"

using namespace NEO;

struct WriteBufferRectHw
    : public CommandEnqueueAUBFixture,
      public ::testing::WithParamInterface<std::tuple<size_t, size_t>>,
      public ::testing::Test {

    void SetUp() override {
        CommandEnqueueAUBFixture::SetUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::TearDown();
    }
};

typedef WriteBufferRectHw AUBWriteBufferRect;
static const size_t width = 10;

HWTEST_P(AUBWriteBufferRect, simple3D) {
    MockContext context(this->pDevice);
    size_t rowPitch = width;
    size_t slicePitch = rowPitch * rowPitch;

    size_t bufferSizeBuff = rowPitch * rowPitch * rowPitch;
    size_t bufferSize = alignUp(bufferSizeBuff, 4096);

    size_t zHostOffs;
    size_t zBuffOffs;
    std::tie(zBuffOffs, zHostOffs) = GetParam();

    ASSERT_LT(zBuffOffs, width);
    ASSERT_LT(zHostOffs, width);

    uint8_t *srcMemory = (uint8_t *)::alignedMalloc(bufferSize, 4096);
    uint8_t *destMemory = (uint8_t *)::alignedMalloc(bufferSize, 4096);

    for (unsigned int i = 0; i < bufferSize; i++)
        srcMemory[i] = i;

    memset(destMemory, 0x00, bufferSize);

    auto retVal = CL_INVALID_VALUE;
    auto dstBuffer = std::unique_ptr<Buffer>(Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        bufferSize,
        destMemory,
        retVal));
    ASSERT_NE(nullptr, dstBuffer);

    uint8_t *pDestMemory = (uint8_t *)dstBuffer->getGraphicsAllocation()->getGpuAddress();

    cl_bool blockingWrite = CL_TRUE;

    size_t bufferOrigin[] = {0, 0, zBuffOffs};
    size_t hostOrigin[] = {0, 0, zHostOffs};
    size_t region[] = {rowPitch, rowPitch, 1};

    retVal = pCmdQ->enqueueWriteBufferRect(
        dstBuffer.get(),
        blockingWrite,
        bufferOrigin,
        hostOrigin,
        region,
        rowPitch,
        slicePitch,
        rowPitch,
        slicePitch,
        srcMemory,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    char *ptr = new char[slicePitch];
    memset(ptr, 0, slicePitch);
    for (unsigned int i = 0; i < rowPitch; i++) {
        //one slice will be copied from src. all others should be zeros
        if (i == zBuffOffs) {
            AUBCommandStreamFixture::expectMemory<FamilyType>(pDestMemory + slicePitch * i, srcMemory + slicePitch * zHostOffs, slicePitch);
        } else {
            AUBCommandStreamFixture::expectMemory<FamilyType>(pDestMemory + slicePitch * i, ptr, slicePitch);
        }
    }
    delete[] ptr;

    ::alignedFree(srcMemory);
    ::alignedFree(destMemory);
}
INSTANTIATE_TEST_CASE_P(AUBWriteBufferRect_simple,
                        AUBWriteBufferRect,
                        ::testing::Combine(
                            ::testing::Values(0, 1, 2, 3, 4),
                            ::testing::Values(0, 1, 2, 3, 4)));

struct AUBWriteBufferRectUnaligned
    : public CommandEnqueueAUBFixture,
      public ::testing::Test {

    void SetUp() override {
        CommandEnqueueAUBFixture::SetUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::TearDown();
    }

    template <typename FamilyType>
    void testWriteBufferUnaligned(size_t offset, size_t size) {
        MockContext context(&pCmdQ->getDevice());

        char srcMemory[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        const auto bufferSize = sizeof(srcMemory);
        char dstMemory[bufferSize] = {0};
        char referenceMemory[bufferSize] = {0};

        auto retVal = CL_INVALID_VALUE;

        auto buffer = std::unique_ptr<Buffer>(Buffer::create(
            &context,
            CL_MEM_COPY_HOST_PTR,
            bufferSize,
            dstMemory,
            retVal));
        ASSERT_NE(nullptr, buffer);

        buffer->forceDisallowCPUCopy = true;

        uint8_t *pDestMemory = (uint8_t *)buffer->getGraphicsAllocation()->getGpuAddress();

        cl_bool blockingWrite = CL_TRUE;

        size_t rowPitch = bufferSize / 4;
        size_t slicePitch = 4 * rowPitch;
        size_t bufferOrigin[] = {0, 1, 0};
        size_t hostOrigin[] = {0, 0, 0};
        size_t region[] = {size, 1, 1};

        retVal = pCmdQ->enqueueWriteBufferRect(
            buffer.get(),
            blockingWrite,
            bufferOrigin,
            hostOrigin,
            region,
            rowPitch,
            slicePitch,
            rowPitch,
            slicePitch,
            ptrOffset(srcMemory, offset),
            0,
            nullptr,
            nullptr);

        EXPECT_EQ(CL_SUCCESS, retVal);
        pCmdQ->finish(true);

        AUBCommandStreamFixture::expectMemory<FamilyType>(pDestMemory, referenceMemory, rowPitch);
        AUBCommandStreamFixture::expectMemory<FamilyType>(pDestMemory + rowPitch * bufferOrigin[1], ptrOffset(srcMemory, offset), size);
        AUBCommandStreamFixture::expectMemory<FamilyType>(pDestMemory + rowPitch * bufferOrigin[1] + size, referenceMemory, bufferSize - size - rowPitch);
    }
};

HWTEST_F(AUBWriteBufferRectUnaligned, misalignedHostPtr) {
    const std::vector<size_t> offsets = {0, 1, 2, 3};
    const std::vector<size_t> sizes = {4, 3, 2, 1};
    for (auto offset : offsets) {
        for (auto size : sizes) {
            testWriteBufferUnaligned<FamilyType>(offset, size);
        }
    }
}
