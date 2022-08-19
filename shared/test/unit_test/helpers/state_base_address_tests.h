/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "gtest/gtest.h"

struct SbaTest : public NEO::DeviceFixture, public ::testing::Test {
    void SetUp() override {
        NEO::DeviceFixture::setUp();
        size_t sizeStream = 512;
        size_t alignmentStream = 0x1000;

        sshBuffer = alignedMalloc(sizeStream, alignmentStream);
        ASSERT_NE(nullptr, sshBuffer);
        ssh.replaceBuffer(sshBuffer, sizeStream);
        auto graphicsAllocation = new MockGraphicsAllocation(sshBuffer, sizeStream);
        ssh.replaceGraphicsAllocation(graphicsAllocation);

        dshBuffer = alignedMalloc(sizeStream, alignmentStream);
        ASSERT_NE(nullptr, dshBuffer);
        dsh.replaceBuffer(dshBuffer, sizeStream);
        graphicsAllocation = new MockGraphicsAllocation(dshBuffer, sizeStream);
        dsh.replaceGraphicsAllocation(graphicsAllocation);

        iohBuffer = alignedMalloc(sizeStream, alignmentStream);
        ASSERT_NE(nullptr, iohBuffer);
        ioh.replaceBuffer(iohBuffer, sizeStream);
        graphicsAllocation = new MockGraphicsAllocation(iohBuffer, sizeStream);
        ioh.replaceGraphicsAllocation(graphicsAllocation);

        linearStreamBuffer = alignedMalloc(sizeStream, alignmentStream);
        commandStream.replaceBuffer(linearStreamBuffer, alignmentStream);
    }

    void TearDown() override {
        alignedFree(linearStreamBuffer);

        delete ssh.getGraphicsAllocation();
        alignedFree(sshBuffer);

        delete dsh.getGraphicsAllocation();
        alignedFree(dshBuffer);

        delete ioh.getGraphicsAllocation();
        alignedFree(iohBuffer);

        NEO::DeviceFixture::tearDown();
    }
    IndirectHeap ssh = {nullptr};
    IndirectHeap dsh = {nullptr};
    IndirectHeap ioh = {nullptr};

    DebugManagerStateRestore restorer;
    LinearStream commandStream;

    void *sshBuffer = nullptr;
    void *dshBuffer = nullptr;
    void *iohBuffer = nullptr;
    void *linearStreamBuffer = nullptr;
};
