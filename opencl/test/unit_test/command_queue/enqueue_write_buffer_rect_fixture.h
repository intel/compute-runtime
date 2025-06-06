/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/aligned_memory.h"

#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

namespace NEO {

struct EnqueueWriteBufferRectTest : public CommandEnqueueFixture,
                                    public ::testing::Test {
    void SetUp() override {
        CommandEnqueueFixture::setUp();
        context.reset(new MockContext(pClDevice));
        BufferDefaults::context = context.get();

        // For 3D
        hostPtr = ::alignedMalloc(slicePitch * rowPitch, 4096);

        auto retVal = CL_INVALID_VALUE;
        buffer.reset(Buffer::create(
            context.get(),
            CL_MEM_READ_WRITE,
            slicePitch * rowPitch,
            nullptr,
            retVal));

        nonZeroCopyBuffer.reset(BufferHelper<BufferUseHostPtr<>>::create());

        ASSERT_NE(nullptr, buffer.get());
    }

    void TearDown() override {
        buffer.reset();
        nonZeroCopyBuffer.reset();
        ::alignedFree(hostPtr);
        context.reset();
        CommandEnqueueFixture::tearDown();
    }

  protected:
    template <typename FamilyType>
    void enqueueWriteBufferRect2D(cl_bool blocking = CL_FALSE) {
        size_t bufferOrigin[] = {0, 0, 0};
        size_t hostOrigin[] = {0, 0, 0};
        size_t region[] = {50, 50, 1};
        auto retVal = pCmdQ->enqueueWriteBufferRect(
            buffer.get(),
            blocking,
            bufferOrigin,
            hostOrigin,
            region,
            rowPitch,
            slicePitch,
            rowPitch,
            slicePitch,
            hostPtr,
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        parseCommands<FamilyType>(*pCmdQ);
    }

    std::unique_ptr<MockContext> context;
    std::unique_ptr<Buffer> buffer;
    std::unique_ptr<Buffer> nonZeroCopyBuffer;

    void *hostPtr = nullptr;

    static const size_t rowPitch = 100;
    static const size_t slicePitch = 100 * 100;
};
} // namespace NEO
