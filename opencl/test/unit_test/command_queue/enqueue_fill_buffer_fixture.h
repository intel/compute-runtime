/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/compiler_product_helper.h"

#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

namespace NEO {

struct EnqueueFillBufferFixture : public CommandEnqueueFixture {

    void setUp() {
        CommandEnqueueFixture::setUp();

        BufferDefaults::context = new MockContext;

        buffer = BufferHelper<>::create();

        auto &compilerProductHelper = this->pDevice->getCompilerProductHelper();
        isHeaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
    }

    void tearDown() {
        delete buffer;
        delete BufferDefaults::context;

        CommandEnqueueFixture::tearDown();
    }

    template <typename FamilyType>
    void enqueueFillBuffer() {
        auto retVal = EnqueueFillBufferHelper<>::enqueueFillBuffer(
            pCmdQ,
            buffer);
        EXPECT_EQ(CL_SUCCESS, retVal);
        parseCommands<FamilyType>(*pCmdQ);
    }

    int32_t adjustBuiltInType(int32_t builtInType) {

        if (this->isHeaplessEnabled) {
            switch (builtInType) {
            case EBuiltInOps::fillBuffer:
            case EBuiltInOps::fillBufferStateless:
                return EBuiltInOps::fillBufferStatelessHeapless;
            }
        }

        return builtInType;
    }

    bool isHeaplessEnabled = false;

    MockContext context;
    Buffer *buffer = nullptr;
};
} // namespace NEO
