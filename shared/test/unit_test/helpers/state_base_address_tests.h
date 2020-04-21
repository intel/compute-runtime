/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "test.h"

struct SBATest : ::testing::Test {
    void SetUp() override {
        size_t sizeStream = 512;
        size_t alignmentStream = 0x1000;
        sshBuffer = alignedMalloc(sizeStream, alignmentStream);

        ASSERT_NE(nullptr, sshBuffer);

        ssh.replaceBuffer(sshBuffer, sizeStream);
        auto graphicsAllocation = new MockGraphicsAllocation(sshBuffer, sizeStream);
        ssh.replaceGraphicsAllocation(graphicsAllocation);
    }

    void TearDown() override {
        delete ssh.getGraphicsAllocation();
        alignedFree(sshBuffer);
    }
    IndirectHeap ssh = {nullptr};
    void *sshBuffer = nullptr;
    DebugManagerStateRestore restorer;
};
