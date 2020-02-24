/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/ptr_math.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "test.h"

using namespace NEO;

struct ReadBufferRectHw
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

typedef ReadBufferRectHw AUBReadBufferRect;
static const size_t width = 10;

HWTEST_P(AUBReadBufferRect, simple3D) {
    MockContext context(this->pClDevice);
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
    auto srcBuffer = std::unique_ptr<Buffer>(Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        bufferSize,
        srcMemory,
        retVal));
    ASSERT_NE(nullptr, srcBuffer);

    cl_bool blockingRead = CL_FALSE;

    createResidentAllocationAndStoreItInCsr(destMemory, bufferSize);

    size_t bufferOrigin[] = {0, 0, zBuffOffs};
    size_t hostOrigin[] = {0, 0, zHostOffs};
    size_t region[] = {rowPitch, rowPitch, 1};

    retVal = pCmdQ->enqueueReadBufferRect(
        srcBuffer.get(),
        blockingRead,
        bufferOrigin,
        hostOrigin,
        region,
        rowPitch,
        slicePitch,
        rowPitch,
        slicePitch,
        destMemory,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pCmdQ->flush();
    EXPECT_EQ(CL_SUCCESS, retVal);

    char *ptr = new char[slicePitch];
    memset(ptr, 0, slicePitch);
    for (unsigned int i = 0; i < rowPitch; i++) {
        //one slice will be copied from src. all others should be zeros
        if (i == zHostOffs) {
            AUBCommandStreamFixture::expectMemory<FamilyType>(destMemory + slicePitch * i, srcMemory + slicePitch * zBuffOffs, slicePitch);
        } else {
            AUBCommandStreamFixture::expectMemory<FamilyType>(destMemory + slicePitch * i, ptr, slicePitch);
        }
    }
    delete[] ptr;

    ::alignedFree(srcMemory);
    ::alignedFree(destMemory);
}
INSTANTIATE_TEST_CASE_P(AUBReadBufferRect_simple,
                        AUBReadBufferRect,
                        ::testing::Combine(
                            ::testing::Values(0, 1, 2, 3, 4),
                            ::testing::Values(0, 1, 2, 3, 4)));

struct AUBReadBufferRectUnaligned
    : public CommandEnqueueAUBFixture,
      public ::testing::Test {

    void SetUp() override {
        CommandEnqueueAUBFixture::SetUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::TearDown();
    }

    template <typename FamilyType>
    void testReadBufferUnaligned(size_t offset, size_t size) {
        MockContext context(pCmdQ->getDevice().getSpecializedDevice<ClDevice>());

        char srcMemory[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        const auto bufferSize = sizeof(srcMemory);
        void *dstMemory = alignedMalloc(bufferSize, MemoryConstants::pageSize);
        memset(dstMemory, 0, bufferSize);
        char referenceMemory[bufferSize] = {0};

        auto retVal = CL_INVALID_VALUE;

        auto buffer = std::unique_ptr<Buffer>(Buffer::create(
            &context,
            CL_MEM_COPY_HOST_PTR,
            bufferSize,
            srcMemory,
            retVal));
        ASSERT_NE(nullptr, buffer);

        buffer->forceDisallowCPUCopy = true;

        // Map destination memory to GPU
        GraphicsAllocation *allocation = createResidentAllocationAndStoreItInCsr(dstMemory, bufferSize);
        auto dstMemoryGPUPtr = reinterpret_cast<char *>(allocation->getGpuAddress());

        cl_bool blockingRead = CL_FALSE;

        size_t rowPitch = bufferSize / 4;
        size_t slicePitch = 4 * rowPitch;
        size_t bufferOrigin[] = {0, 1, 0};
        size_t hostOrigin[] = {0, 0, 0};
        size_t region[] = {size, 1, 1};

        retVal = pCmdQ->enqueueReadBufferRect(
            buffer.get(),
            blockingRead,
            bufferOrigin,
            hostOrigin,
            region,
            rowPitch,
            slicePitch,
            rowPitch,
            slicePitch,
            ptrOffset(dstMemory, offset),
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = pCmdQ->flush();
        EXPECT_EQ(CL_SUCCESS, retVal);

        AUBCommandStreamFixture::expectMemory<FamilyType>(dstMemoryGPUPtr, referenceMemory, offset);
        AUBCommandStreamFixture::expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, offset), &srcMemory[rowPitch * bufferOrigin[1]], size);
        AUBCommandStreamFixture::expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, size + offset), referenceMemory, bufferSize - offset - size);
        pCmdQ->finish();
        alignedFree(dstMemory);
    }
};

HWTEST_F(AUBReadBufferRectUnaligned, misalignedHostPtr) {
    const std::vector<size_t> offsets = {0, 1, 2, 3};
    const std::vector<size_t> sizes = {4, 3, 2, 1};
    for (auto offset : offsets) {
        for (auto size : sizes) {
            testReadBufferUnaligned<FamilyType>(offset, size);
        }
    }
}
