/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include <cstdint>
#include <memory>
#include <new>
#include <tuple>
#include <vector>

using namespace NEO;

struct ReadBufferRectHw
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

typedef ReadBufferRectHw AUBReadBufferRect;
static const size_t width = 10;

HWTEST_P(AUBReadBufferRect, Given3dWhenReadingBufferThenExpectationsAreMet) {
    MockContext context(this->pClDevice);
    static constexpr size_t rowPitch = width;
    static constexpr size_t slicePitch = rowPitch * rowPitch;
    static constexpr size_t bufferSizeBuff = rowPitch * rowPitch * rowPitch;
    static constexpr size_t bufferSize = alignUp(bufferSizeBuff, size_t{4096});

    size_t zHostOffs;
    size_t zBuffOffs;
    std::tie(zBuffOffs, zHostOffs) = GetParam();

    ASSERT_LT(zBuffOffs, width);
    ASSERT_LT(zHostOffs, width);

    alignas(4096) uint8_t srcRaw[bufferSize];
    alignas(4096) uint8_t destRaw[bufferSize];
    uint8_t *srcMemory = srcRaw;
    uint8_t *destMemory = destRaw;

    for (unsigned int i = 0; i < bufferSize; i++) {
        srcMemory[i] = i;
    }

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
        // one slice will be copied from src. all others should be zeros
        if (i == zHostOffs) {
            expectMemory<FamilyType>(destMemory + slicePitch * i, srcMemory + slicePitch * zBuffOffs, slicePitch);
        } else {
            expectMemory<FamilyType>(destMemory + slicePitch * i, ptr, slicePitch);
        }
    }
    delete[] ptr;
}
INSTANTIATE_TEST_SUITE_P(AUBReadBufferRect_simple,
                         AUBReadBufferRect,
                         ::testing::Combine(
                             ::testing::Values(0, 1, 2, 3, 4),
                             ::testing::Values(0, 1, 2, 3, 4)));

struct AUBReadBufferRectUnaligned
    : public CommandEnqueueAUBFixture,
      public ::testing::Test {

    void SetUp() override {
        CommandEnqueueAUBFixture::setUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::tearDown();
    }

    template <typename FamilyType>
    void testReadBufferUnaligned(MockContext &context, size_t offset, size_t size) {
        char srcMemory[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        constexpr auto bufferSize = sizeof(srcMemory);
        char referenceMemory[bufferSize] = {0};

        cl_int retVal = CL_INVALID_VALUE;
        auto srcBuffer = std::unique_ptr<Buffer>(Buffer::create(
            &context,
            CL_MEM_COPY_HOST_PTR,
            bufferSize,
            srcMemory,
            retVal));
        ASSERT_NE(nullptr, srcBuffer);
        srcBuffer->forceDisallowCPUCopy = true;

        std::vector<uint8_t> rawDstMemory(bufferSize + MemoryConstants::pageSize - 1, 0);
        void *dstMemory = alignUp(rawDstMemory.data(), MemoryConstants::pageSize);
        GraphicsAllocation *allocation = createResidentAllocationAndStoreItInCsr(dstMemory, bufferSize);
        auto dstMemoryGPUPtr = reinterpret_cast<char *>(allocation->getGpuAddress());

        cl_bool blockingRead = CL_FALSE;

        size_t rowPitch = bufferSize / 4;
        size_t slicePitch = 4 * rowPitch;
        size_t bufferOrigin[] = {0, 1, 0};
        size_t hostOrigin[] = {0, 0, 0};
        size_t region[] = {size, 1, 1};

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
            ptrOffset(dstMemory, offset),
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = pCmdQ->flush();
        EXPECT_EQ(CL_SUCCESS, retVal);

        expectMemory<FamilyType>(dstMemoryGPUPtr, referenceMemory, offset);
        expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, offset), &srcMemory[rowPitch * bufferOrigin[1]], size);
        expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, size + offset), referenceMemory, bufferSize - offset - size);
        pCmdQ->finish(false);
    }
};

HWTEST_F(AUBReadBufferRectUnaligned, GivenMisalignedHostPtrWhenReadingBufferThenExpectationAreMet) {
    MockContext context(pCmdQ->getDevice().getSpecializedDevice<ClDevice>());
    static constexpr size_t offsets[] = {0, 1, 2, 3};
    static constexpr size_t sizes[] = {4, 3, 2, 1};
    for (auto offset : offsets) {
        for (auto size : sizes) {
            testReadBufferUnaligned<FamilyType>(context, offset, size);
        }
    }
}
