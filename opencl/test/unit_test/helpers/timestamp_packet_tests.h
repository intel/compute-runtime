/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_timestamp_packet.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

using namespace NEO;

struct TimestampPacketTests : public ::testing::Test {
    struct MockTagNode : public TagNode<TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>> {
        using TagNode<TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>>::gpuAddress;
    };

    void SetUp() override {
        debugManager.flags.EnableTimestampPacket.set(1);

        executionEnvironment = platform()->peekExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(2);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        }
        executionEnvironment->calculateMaxOsContextCount();
        device = std::make_unique<MockClDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
        context = new MockContext(device.get());
        kernel = std::make_unique<MockKernelWithInternals>(*device, context);
        mockCmdQ = new MockCommandQueue(context, device.get(), nullptr, false);
    }

    void TearDown() override {
        mockCmdQ->release();
        context->release();
    }

    template <typename FamilyType>
    void verifySemaphore(typename FamilyType::MI_SEMAPHORE_WAIT *semaphoreCmd, TagNodeBase *timestampPacketNode, uint32_t packetId) {
        using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
        ASSERT_NE(nullptr, semaphoreCmd);
        EXPECT_EQ(semaphoreCmd->getCompareOperation(), MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
        EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());

        uint64_t compareOffset = packetId * TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>::getSinglePacketSize();
        auto dataAddress = TimestampPacketHelper::getContextEndGpuAddress(*timestampPacketNode) + compareOffset;

        EXPECT_EQ(dataAddress, semaphoreCmd->getSemaphoreGraphicsAddress());
    };

    template <typename FamilyType>
    void setTagToReadyState(TagNodeBase *tagNode) {
        auto packetsUsed = tagNode->getPacketsUsed();
        tagNode->initialize();

        typename FamilyType::TimestampPacketType zeros[4] = {};

        for (uint32_t i = 0; i < TimestampPacketConstants::preferredPacketCount; i++) {
            tagNode->assignDataToAllTimestamps(i, zeros);
        }
        tagNode->setPacketsUsed(packetsUsed);
    }

    const size_t gws[3] = {1, 1, 1};

    ExecutionEnvironment *executionEnvironment;
    std::unique_ptr<MockClDevice> device;
    MockContext *context;
    std::unique_ptr<MockKernelWithInternals> kernel;
    MockCommandQueue *mockCmdQ;
    DebugManagerStateRestore restorer;
};

template <template <typename> class CsrType>
struct TimestampPacketTestsWithMockCsrT : public TimestampPacketTests {
    void SetUp() override {}
    void TearDown() override {}

    template <typename FamilyType>
    void setUpT() {
        EnvironmentWithCsrWrapper environment;
        environment.setCsrType<CsrType<FamilyType>>();

        TimestampPacketTests::SetUp();
    }

    template <typename FamilyType>
    void tearDownT() {
        TimestampPacketTests::TearDown();
    }
};
