/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "test.h"

using namespace NEO;

struct TimestampPacketSimpleTests : public ::testing::Test {
    class MockTimestampPacketStorage : public TimestampPackets<uint32_t> {
      public:
        using TimestampPackets<uint32_t>::implicitGpuDependenciesCount;
        using TimestampPackets<uint32_t>::packets;
    };

    void setTagToReadyState(TagNodeBase *tagNode) {
        auto packetsUsed = tagNode->getPacketsUsed();
        tagNode->initialize();

        uint32_t zeros[4] = {};

        for (uint32_t i = 0; i < TimestampPacketSizeControl::preferredPacketCount; i++) {
            tagNode->assignDataToAllTimestamps(i, zeros);
        }
        tagNode->setPacketsUsed(packetsUsed);
    }

    const size_t gws[3] = {1, 1, 1};
};

struct TimestampPacketTests : public TimestampPacketSimpleTests {
    struct MockTagNode : public TagNode<TimestampPackets<uint32_t>> {
        using TagNode<TimestampPackets<uint32_t>>::gpuAddress;
    };

    void SetUp() override {
        DebugManager.flags.EnableTimestampPacket.set(1);

        executionEnvironment = platform()->peekExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(2);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        }
        device = std::make_unique<MockClDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
        context = new MockContext(device.get());
        kernel = std::make_unique<MockKernelWithInternals>(*device, context);
        mockCmdQ = new MockCommandQueue(context, device.get(), nullptr);
    }

    void TearDown() override {
        mockCmdQ->release();
        context->release();
    }

    template <typename MI_SEMAPHORE_WAIT>
    void verifySemaphore(MI_SEMAPHORE_WAIT *semaphoreCmd, TagNodeBase *timestampPacketNode, uint32_t packetId) {
        EXPECT_NE(nullptr, semaphoreCmd);
        EXPECT_EQ(semaphoreCmd->getCompareOperation(), MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
        EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());

        uint64_t compareOffset = packetId * TimestampPackets<uint32_t>::getSinglePacketSize();
        auto dataAddress = TimestampPacketHelper::getContextEndGpuAddress(*timestampPacketNode) + compareOffset;

        EXPECT_EQ(dataAddress, semaphoreCmd->getSemaphoreGraphicsAddress());
    };

    template <typename GfxFamily>
    void verifyMiAtomic(typename GfxFamily::MI_ATOMIC *miAtomicCmd, TagNodeBase *timestampPacketNode) {
        using MI_ATOMIC = typename GfxFamily::MI_ATOMIC;
        EXPECT_NE(nullptr, miAtomicCmd);
        auto writeAddress = TimestampPacketHelper::getGpuDependenciesCountGpuAddress(*timestampPacketNode);

        EXPECT_EQ(MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, miAtomicCmd->getAtomicOpcode());
        EXPECT_EQ(writeAddress, UnitTestHelper<GfxFamily>::getAtomicMemoryAddress(*miAtomicCmd));
    };

    void verifyDependencyCounterValues(TimestampPacketContainer *timestampPacketContainer, uint32_t expectedValue) {
        auto &nodes = timestampPacketContainer->peekNodes();
        EXPECT_NE(0u, nodes.size());
        for (auto &node : nodes) {
            EXPECT_EQ(expectedValue, node->getImplicitCpuDependenciesCount());
        }
    }

    ExecutionEnvironment *executionEnvironment;
    std::unique_ptr<MockClDevice> device;
    MockContext *context;
    std::unique_ptr<MockKernelWithInternals> kernel;
    MockCommandQueue *mockCmdQ;
    DebugManagerStateRestore restorer;
};
