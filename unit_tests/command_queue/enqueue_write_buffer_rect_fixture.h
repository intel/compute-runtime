/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/aligned_memory.h"
#include "core/helpers/ptr_math.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"
#include "unit_tests/mocks/mock_context.h"

#include "gtest/gtest.h"

namespace NEO {

struct EnqueueWriteBufferRectTest : public CommandEnqueueFixture,
                                    public ::testing::Test {
    EnqueueWriteBufferRectTest(void)
        : buffer(nullptr),
          hostPtr(nullptr) {
    }

    void SetUp() override {
        CommandEnqueueFixture::SetUp();
        context.reset(new MockContext(pClDevice));
        BufferDefaults::context = context.get();

        //For 3D
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
        buffer.reset(nullptr);
        nonZeroCopyBuffer.reset(nullptr);
        ::alignedFree(hostPtr);
        context.reset(nullptr);
        CommandEnqueueFixture::TearDown();
    }

  protected:
    template <typename FamilyType>
    void enqueueWriteBufferRect2D(cl_bool blocking = CL_FALSE) {
        typedef typename FamilyType::WALKER_TYPE GPGPU_WALKER;
        typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;

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

    void *hostPtr;

    static const size_t rowPitch = 100;
    static const size_t slicePitch = 100 * 100;
};
} // namespace NEO
