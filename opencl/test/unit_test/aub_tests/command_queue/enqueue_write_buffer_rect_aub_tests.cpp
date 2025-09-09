/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

struct WriteBufferRectHw
    : public CommandEnqueueAUBFixture,
      public ::testing::WithParamInterface<std::tuple<size_t, size_t>>,
      public ::testing::Test {

    void SetUp() override {
        CommandEnqueueAUBFixture::setUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::tearDown();
    }
};

using AUBWriteBufferRect = WriteBufferRectHw;

HWTEST_P(AUBWriteBufferRect, Given3dWhenWritingBufferThenExpectationsAreMet) {

    MockContext context(this->pClDevice);
    const size_t width = 10;
    size_t rowPitch = width;
    size_t slicePitch = rowPitch * rowPitch;

    size_t bufferSizeBuff = rowPitch * rowPitch * rowPitch;
    size_t bufferSize = alignUp(bufferSizeBuff, 4096);

    auto [zBuffOffs, zHostOffs] = GetParam();

    ASSERT_LT(zBuffOffs, width);
    ASSERT_LT(zHostOffs, width);

    auto srcMemory = static_cast<uint8_t *>(::alignedMalloc(bufferSize, 4096));
    auto destMemory = static_cast<uint8_t *>(::alignedMalloc(bufferSize, 4096));

    for (unsigned int i = 0; i < bufferSize; i++) {
        auto oneBytePattern = static_cast<uint8_t>(i & 0xff);
        srcMemory[i] = oneBytePattern;
    }

    memset(destMemory, 0x00, bufferSize);

    auto retVal = CL_INVALID_VALUE;
    auto dstBuffer = std::unique_ptr<Buffer>(Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        bufferSize,
        destMemory,
        retVal));
    ASSERT_NE(nullptr, dstBuffer);

    uint8_t *pDestMemory = reinterpret_cast<uint8_t *>(ptrOffset(dstBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress(), dstBuffer->getOffset()));

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

    std::vector<uint8_t> ptr(slicePitch, 0);
    for (unsigned int i = 0; i < rowPitch; i++) {
        // one slice will be copied from src. all others should be zeros
        if (i == zBuffOffs) {
            expectMemory<FamilyType>(pDestMemory + slicePitch * i, srcMemory + slicePitch * zHostOffs, slicePitch);
        } else {
            expectMemory<FamilyType>(pDestMemory + slicePitch * i, ptr.data(), slicePitch);
        }
    }

    ::alignedFree(srcMemory);
    ::alignedFree(destMemory);
}
INSTANTIATE_TEST_SUITE_P(AUBWriteBufferRect_simple,
                         AUBWriteBufferRect,
                         ::testing::Combine(
                             ::testing::Values(0u, 1u, 2u, 3u, 4u),
                             ::testing::Values(0u, 1u, 2u, 3u, 4u)));

struct AUBWriteBufferRectUnaligned
    : public CommandEnqueueAUBFixture,
      public ::testing::Test {

    void SetUp() override {
        CommandEnqueueAUBFixture::setUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::tearDown();
    }

    template <typename FamilyType>
    void testWriteBufferUnaligned(size_t offset, size_t size) {
        MockContext context(pCmdQ->getDevice().getSpecializedDevice<ClDevice>());

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

        uint8_t *pDestMemory = reinterpret_cast<uint8_t *>(ptrOffset(buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress(), buffer->getOffset()));

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
        pCmdQ->finish(false);

        expectMemory<FamilyType>(pDestMemory, referenceMemory, rowPitch);
        expectMemory<FamilyType>(pDestMemory + rowPitch * bufferOrigin[1], ptrOffset(srcMemory, offset), size);
        expectMemory<FamilyType>(pDestMemory + rowPitch * bufferOrigin[1] + size, referenceMemory, bufferSize - size - rowPitch);
    }
};

HWTEST_F(AUBWriteBufferRectUnaligned, GivenMisalignedHostPtrWhenWritingBufferThenExpectationsAreMet) {
    const std::vector<size_t> offsets = {0, 1, 2, 3};
    const std::vector<size_t> sizes = {4, 3, 2, 1};
    for (auto offset : offsets) {
        for (auto size : sizes) {
            testWriteBufferUnaligned<FamilyType>(offset, size);
        }
    }
}
