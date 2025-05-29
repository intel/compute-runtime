/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/utilities/wait_util.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/event/user_event.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "test_traits_common.h"

using namespace NEO;

using MultiRootDeviceCommandStreamReceiverBufferTests = MultiRootDeviceFixture;

HWTEST_F(MultiRootDeviceCommandStreamReceiverBufferTests, givenMultipleEventInMultiRootDeviceEnvironmentWhenTheyArePassedToEnqueueWithSubmissionThenCsIsWaitingForEventsFromPreviousDevices) {
    USE_REAL_FILE_SYSTEM();
    REQUIRE_SVM_OR_SKIP(device1);
    REQUIRE_SVM_OR_SKIP(device2);

    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    cl_int retVal = 0;
    size_t offset = 0;
    size_t size = 1;

    auto pCmdQ1 = context->getSpecialQueue(1u);
    auto pCmdQ2 = context->getSpecialQueue(2u);

    std::unique_ptr<MockProgram> program(Program::createBuiltInFromSource<MockProgram>("FillBufferBytes", context.get(), context->getDevices(), &retVal));
    program->build(program->getDevices(), nullptr);
    std::unique_ptr<MockKernel> kernel(Kernel::create<MockKernel>(program.get(), program->getKernelInfoForKernel("FillBufferBytes"), *context->getDevice(0), retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    size_t svmSize = 4096;
    void *svmPtr = alignedMalloc(svmSize, MemoryConstants::pageSize);
    MockGraphicsAllocation svmAlloc(svmPtr, svmSize);

    Event event1(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    auto node1 = event1.getMultiRootTimestampSyncNode();
    Event event2(nullptr, CL_COMMAND_NDRANGE_KERNEL, 6, 16);
    Event event3(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 4, 20);
    auto node3 = event3.getMultiRootTimestampSyncNode();
    Event event4(pCmdQ2, CL_COMMAND_NDRANGE_KERNEL, 3, 4);
    auto node4 = event4.getMultiRootTimestampSyncNode();
    Event event5(pCmdQ2, CL_COMMAND_NDRANGE_KERNEL, 2, 7);
    auto node5 = event5.getMultiRootTimestampSyncNode();
    UserEvent userEvent1(&pCmdQ1->getContext());
    userEvent1.getMultiRootTimestampSyncNode();
    UserEvent userEvent2(&pCmdQ2->getContext());
    userEvent2.getMultiRootTimestampSyncNode();

    userEvent1.setStatus(CL_COMPLETE);
    userEvent2.setStatus(CL_COMPLETE);

    cl_event eventWaitList[] =
        {
            &event1,
            &event2,
            &event3,
            &event4,
            &event5,
            &userEvent1,
            &userEvent2,
        };
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);

    {
        kernel->setSvmKernelExecInfo(&svmAlloc);

        retVal = pCmdQ1->enqueueKernel(
            kernel.get(),
            1,
            &offset,
            &size,
            &size,
            numEventsInWaitList,
            eventWaitList,
            nullptr);

        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ1->getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(2u, semaphores.size());

        auto semaphoreCmd0 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[0]));
        EXPECT_EQ(1u, semaphoreCmd0->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(node4->getContextEndAddress(0u)), semaphoreCmd0->getSemaphoreGraphicsAddress());

        auto semaphoreCmd1 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[1]));
        EXPECT_EQ(1u, semaphoreCmd1->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(node5->getContextEndAddress(0u)), semaphoreCmd1->getSemaphoreGraphicsAddress());
    }

    {
        kernel->setSvmKernelExecInfo(&svmAlloc);

        retVal = pCmdQ2->enqueueKernel(
            kernel.get(),
            1,
            &offset,
            &size,
            &size,
            numEventsInWaitList,
            eventWaitList,
            nullptr);

        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ2->getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(2u, semaphores.size());

        auto semaphoreCmd0 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[0]));
        EXPECT_EQ(1u, semaphoreCmd0->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(node1->getContextEndAddress(0u)), semaphoreCmd0->getSemaphoreGraphicsAddress());

        auto semaphoreCmd1 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[1]));
        EXPECT_EQ(1u, semaphoreCmd1->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(node3->getContextEndAddress(0u)), semaphoreCmd1->getSemaphoreGraphicsAddress());
    }
    alignedFree(svmPtr);
}

using CommandStreamReceiverFlushTaskTests = UltCommandStreamReceiverTest;
using MultiRootDeviceCommandStreamReceiverTests = CommandStreamReceiverFlushTaskTests;

HWTEST_F(MultiRootDeviceCommandStreamReceiverTests, givenMultipleEventInMultiRootDeviceEnvironmentWhenTheyArePassedToEnqueueWithoutSubmissionThenCsIsWaitingForEventsFromPreviousDevices) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    EnvironmentWithCsrWrapper environment;
    environment.setCsrType<MockCommandStreamReceiver>();

    auto deviceFactory = std::make_unique<UltClDeviceFactory>(4, 0);
    auto device1 = deviceFactory->rootDevices[1];
    auto device2 = deviceFactory->rootDevices[2];
    auto device3 = deviceFactory->rootDevices[3];

    cl_device_id devices[] = {device1, device2, device3};

    auto context = std::make_unique<MockContext>(ClDeviceVector(devices, 3), false);
    auto mockTagAllocator = std::make_unique<MockTagAllocator<>>(context->getRootDeviceIndices(), device1->getExecutionEnvironment()->memoryManager.get(), 10u);
    std::unique_ptr<TagAllocatorBase> uniquePtr(mockTagAllocator.release());
    context->setMultiRootDeviceTimestampPacketAllocator(uniquePtr);
    auto pCmdQ1 = context->getSpecialQueue(1u);
    auto pCmdQ2 = context->getSpecialQueue(2u);
    auto pCmdQ3 = context->getSpecialQueue(3u);

    Event event1(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    auto node1 = event1.getMultiRootTimestampSyncNode();
    Event event2(nullptr, CL_COMMAND_NDRANGE_KERNEL, 6, 16);
    Event event3(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 4, 20);
    auto node3 = event3.getMultiRootTimestampSyncNode();
    Event event4(pCmdQ2, CL_COMMAND_NDRANGE_KERNEL, 3, 4);
    auto node4 = event4.getMultiRootTimestampSyncNode();
    Event event5(pCmdQ3, CL_COMMAND_NDRANGE_KERNEL, 7, 21);
    auto node5 = event5.getMultiRootTimestampSyncNode();
    Event event6(pCmdQ2, CL_COMMAND_NDRANGE_KERNEL, 2, 7);
    auto node6 = event6.getMultiRootTimestampSyncNode();
    UserEvent userEvent1(&pCmdQ1->getContext());
    UserEvent userEvent2(&pCmdQ2->getContext());

    userEvent1.setStatus(CL_COMPLETE);
    userEvent2.setStatus(CL_COMPLETE);

    cl_event eventWaitList[] =
        {
            &event1,
            &event2,
            &event3,
            &event4,
            &event5,
            &event6,
            &userEvent1,
            &userEvent2,
        };
    cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);

    {
        pCmdQ1->enqueueMarkerWithWaitList(
            numEventsInWaitList,
            eventWaitList,
            nullptr);

        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ1->getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(3u, semaphores.size());

        auto semaphoreCmd0 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[0]));
        EXPECT_EQ(1u, semaphoreCmd0->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(node4->getContextEndAddress(0u)), semaphoreCmd0->getSemaphoreGraphicsAddress());

        auto semaphoreCmd1 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[1]));
        EXPECT_EQ(1u, semaphoreCmd1->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(node5->getContextEndAddress(0u)), semaphoreCmd1->getSemaphoreGraphicsAddress());

        auto semaphoreCmd2 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[2]));
        EXPECT_EQ(1u, semaphoreCmd2->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(node6->getContextEndAddress(0u)), semaphoreCmd2->getSemaphoreGraphicsAddress());
    }

    {
        pCmdQ2->enqueueMarkerWithWaitList(
            numEventsInWaitList,
            eventWaitList,
            nullptr);

        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ2->getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(3u, semaphores.size());

        auto semaphoreCmd0 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[0]));
        EXPECT_EQ(1u, semaphoreCmd0->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(node1->getContextEndAddress(0u)), semaphoreCmd0->getSemaphoreGraphicsAddress());

        auto semaphoreCmd1 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[1]));
        EXPECT_EQ(1u, semaphoreCmd1->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(node3->getContextEndAddress(0u)), semaphoreCmd1->getSemaphoreGraphicsAddress());

        auto semaphoreCmd2 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[2]));
        EXPECT_EQ(1u, semaphoreCmd2->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(node5->getContextEndAddress(0u)), semaphoreCmd2->getSemaphoreGraphicsAddress());
    }

    {
        cl_event eventWaitList[] =
            {
                &event1,
                &event2,
                &event5,
                &userEvent1,
            };
        cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);

        pCmdQ3->enqueueMarkerWithWaitList(
            numEventsInWaitList,
            eventWaitList,
            nullptr);

        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ3->getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(1u, semaphores.size());

        auto semaphoreCmd0 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[0]));
        EXPECT_EQ(1u, semaphoreCmd0->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(node1->getContextEndAddress(0u)), semaphoreCmd0->getSemaphoreGraphicsAddress());
    }
}

struct CrossDeviceDependenciesTests : public ::testing::Test {

    void SetUp() override {
        VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
        defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
        deviceFactory = std::make_unique<UltClDeviceFactory>(3, 0);
        auto device1 = deviceFactory->rootDevices[1];

        auto device2 = deviceFactory->rootDevices[2];

        cl_device_id devices[] = {device1, device2};

        context = std::make_unique<MockContext>(ClDeviceVector(devices, 2), false);

        pCmdQ1 = context->getSpecialQueue(1u);
        pCmdQ2 = context->getSpecialQueue(2u);
    }

    void TearDown() override {
    }

    std::unique_ptr<UltClDeviceFactory> deviceFactory;
    std::unique_ptr<MockContext> context;

    CommandQueue *pCmdQ1 = nullptr;
    CommandQueue *pCmdQ2 = nullptr;
};

HWTEST_F(CrossDeviceDependenciesTests, givenMultipleEventInMultiRootDeviceEnvironmentWhenTheyArePassedToMarkerThenMiSemaphoreWaitCommandSizeIsIncluded) {
    Event event1(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    event1.getMultiRootTimestampSyncNode();
    Event event2(nullptr, CL_COMMAND_NDRANGE_KERNEL, 6, 16);
    Event event3(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 1, 6);
    event3.getMultiRootTimestampSyncNode();
    Event event4(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 4, 20);
    event4.getMultiRootTimestampSyncNode();
    Event event5(pCmdQ2, CL_COMMAND_NDRANGE_KERNEL, 3, 4);
    event5.getMultiRootTimestampSyncNode();
    Event event6(pCmdQ2, CL_COMMAND_NDRANGE_KERNEL, 2, 7);
    event6.getMultiRootTimestampSyncNode();
    UserEvent userEvent1(&pCmdQ1->getContext());
    UserEvent userEvent2(&pCmdQ2->getContext());

    userEvent1.setStatus(CL_COMPLETE);
    userEvent2.setStatus(CL_COMPLETE);

    {
        cl_event eventWaitList[] =
            {
                &event1,
                &event2,
                &event3,
                &event4,
                &userEvent1,
                &userEvent2,
            };
        cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);

        pCmdQ1->enqueueMarkerWithWaitList(
            numEventsInWaitList,
            eventWaitList,
            nullptr);

        EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, nullptr);
        CsrDependencies csrDeps;
        eventsRequest.fillCsrDependenciesForRootDevices(csrDeps, pCmdQ1->getGpgpuCommandStreamReceiver());

        EXPECT_EQ(0u, csrDeps.multiRootTimeStampSyncContainer.size());
        EXPECT_EQ(0u, TimestampPacketHelper::getRequiredCmdStreamSizeForMultiRootDeviceSyncNodesContainer<FamilyType>(csrDeps));
    }

    {
        cl_event eventWaitList[] =
            {
                &event1,
                &event2,
                &event3,
                &event4,
                &event5,
                &event6,
                &userEvent1,
            };
        cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);

        pCmdQ2->enqueueMarkerWithWaitList(
            numEventsInWaitList,
            eventWaitList,
            nullptr);

        EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, nullptr);
        CsrDependencies csrDeps;
        eventsRequest.fillCsrDependenciesForRootDevices(csrDeps, pCmdQ2->getGpgpuCommandStreamReceiver());

        EXPECT_EQ(3u, csrDeps.multiRootTimeStampSyncContainer.size());
        EXPECT_EQ(3u * NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait(), TimestampPacketHelper::getRequiredCmdStreamSizeForMultiRootDeviceSyncNodesContainer<FamilyType>(csrDeps));
    }
}

HWTEST_F(CrossDeviceDependenciesTests, givenWaitListWithEventBlockedByUserEventWhenProgrammingCrossDeviceDependenciesForGpgpuCsrThenProgramSemaphoreWaitOnUnblockingEvent) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    UserEvent userEvent1(&pCmdQ1->getContext());

    cl_event outputEvent1{};
    cl_event inputEvent1 = &userEvent1;

    pCmdQ1->enqueueMarkerWithWaitList(
        1,
        &inputEvent1,
        &outputEvent1);

    auto event1 = castToObject<Event>(outputEvent1);

    ASSERT_NE(nullptr, event1);
    EXPECT_EQ(CompletionStamp::notReady, event1->peekTaskCount());

    cl_int retVal = CL_INVALID_PLATFORM;
    auto buffer = Buffer::create(context.get(), 0, MemoryConstants::pageSize, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    char hostPtr[MemoryConstants::pageSize]{};

    cl_event outputEvent2{};

    auto currentCsUsedCmdq1 = pCmdQ1->getCS(0).getUsed();
    pCmdQ2->enqueueReadBuffer(buffer, CL_FALSE, 0, MemoryConstants::pageSize, hostPtr, nullptr,
                              1,
                              &outputEvent1,
                              &outputEvent2);
    {
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ2->getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(0u, semaphores.size());
    }

    auto event2 = castToObject<Event>(outputEvent2);

    ASSERT_NE(nullptr, event2);
    EXPECT_EQ(CompletionStamp::notReady, event2->peekTaskCount());

    pCmdQ1->enqueueMarkerWithWaitList(
        1,
        &outputEvent2,
        nullptr);
    {
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ1->getCS(0), currentCsUsedCmdq1);
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(0u, semaphores.size());
    }
    userEvent1.setStatus(CL_COMPLETE);
    pCmdQ1->finish();
    pCmdQ2->finish();
    {
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ1->getGpgpuCommandStreamReceiver().getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(1u, semaphores.size());
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[0]));
        EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(event2->getMultiRootDeviceTimestampPacketNodes()->peekNodes().at(0)->getContextEndAddress(0u)), semaphoreCmd->getSemaphoreGraphicsAddress());
    }
    {
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ2->getGpgpuCommandStreamReceiver().getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(1u, semaphores.size());
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[0]));
        EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(event1->getMultiRootDeviceTimestampPacketNodes()->peekNodes().at(0)->getContextEndAddress(0u)), semaphoreCmd->getSemaphoreGraphicsAddress());
    }
    event1->release();
    event2->release();
    buffer->release();
}

HWTEST_F(CrossDeviceDependenciesTests, givenWaitListWithEventBlockedByUserEventWhenProgrammingSingleDeviceDependenciesForGpgpuCsrThenNoSemaphoreWaitIsProgrammed) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBlitterForEnqueueOperations.set(false);

    UserEvent userEvent1(&pCmdQ1->getContext());

    cl_event outputEvent1{};
    cl_event inputEvent1 = &userEvent1;
    pCmdQ1->enqueueMarkerWithWaitList(
        1,
        &inputEvent1,
        &outputEvent1);

    auto event1 = castToObject<Event>(outputEvent1);

    ASSERT_NE(nullptr, event1);
    EXPECT_EQ(CompletionStamp::notReady, event1->peekTaskCount());

    cl_int retVal = CL_INVALID_PLATFORM;
    auto buffer = Buffer::create(context.get(), 0, MemoryConstants::pageSize, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    char hostPtr[MemoryConstants::pageSize]{};

    cl_event outputEvent2{};
    auto currentCsUsed = pCmdQ1->getCS(0).getUsed();
    pCmdQ1->enqueueReadBuffer(buffer, CL_FALSE, 0, MemoryConstants::pageSize, hostPtr, nullptr,
                              1,
                              &outputEvent1,
                              &outputEvent2);
    {
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ1->getCS(0), currentCsUsed);
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(0u, semaphores.size());
    }

    auto event2 = castToObject<Event>(outputEvent2);

    ASSERT_NE(nullptr, event2);
    EXPECT_EQ(CompletionStamp::notReady, event2->peekTaskCount());

    pCmdQ1->enqueueMarkerWithWaitList(
        1,
        &outputEvent2,
        nullptr);
    {
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ1->getCS(0), currentCsUsed);
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(0u, semaphores.size());
    }
    userEvent1.setStatus(CL_COMPLETE);
    event1->release();
    event2->release();
    pCmdQ1->finish();
    {
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ1->getGpgpuCommandStreamReceiver().getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(0u, semaphores.size());
    }
    buffer->release();
}

HWTEST_F(CrossDeviceDependenciesTests, givenWaitListWithEventBlockedByUserEventWhenProgrammingCrossDeviceDependenciesForBlitCsrThenProgramSemaphoreWaitOnUnblockingEvent) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBlitterForEnqueueOperations.set(true);

    for (auto &rootDeviceEnvironment : deviceFactory->rootDevices[0]->getExecutionEnvironment()->rootDeviceEnvironments) {
        REQUIRE_FULL_BLITTER_OR_SKIP(*rootDeviceEnvironment);
    }

    auto clCmdQ1 = clCreateCommandQueue(context.get(), deviceFactory->rootDevices[1], {}, nullptr);
    auto clCmdQ2 = clCreateCommandQueue(context.get(), deviceFactory->rootDevices[2], {}, nullptr);

    pCmdQ1 = castToObject<CommandQueue>(clCmdQ1);
    pCmdQ2 = castToObject<CommandQueue>(clCmdQ2);
    ASSERT_NE(nullptr, pCmdQ1);
    ASSERT_NE(nullptr, pCmdQ2);

    UserEvent userEvent1(&pCmdQ1->getContext());

    cl_event outputEvent1{};
    cl_event inputEvent1 = &userEvent1;

    pCmdQ1->enqueueMarkerWithWaitList(
        1,
        &inputEvent1,
        &outputEvent1);

    auto event1 = castToObject<Event>(outputEvent1);

    ASSERT_NE(nullptr, event1);
    EXPECT_EQ(CompletionStamp::notReady, event1->peekTaskCount());

    cl_int retVal = CL_INVALID_PLATFORM;
    auto buffer = Buffer::create(context.get(), 0, MemoryConstants::pageSize, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    char hostPtr[MemoryConstants::pageSize]{};

    cl_event outputEvent2{};

    pCmdQ2->enqueueReadBuffer(buffer, CL_FALSE, 0, MemoryConstants::pageSize, hostPtr, nullptr,
                              1,
                              &outputEvent1,
                              &outputEvent2);

    auto event2 = castToObject<Event>(outputEvent2);

    ASSERT_NE(nullptr, event2);
    EXPECT_EQ(CompletionStamp::notReady, event2->peekTaskCount());
    {
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ2->getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(0u, semaphores.size());
    }

    cl_event outputEvent3{};
    pCmdQ1->enqueueReadBuffer(buffer, CL_FALSE, 0, MemoryConstants::pageSize, hostPtr, nullptr,
                              1,
                              &outputEvent2,
                              &outputEvent3);

    auto event3 = castToObject<Event>(outputEvent3);

    ASSERT_NE(nullptr, event3);
    EXPECT_EQ(CompletionStamp::notReady, event3->peekTaskCount());
    {
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ2->getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(0u, semaphores.size());
    }

    pCmdQ2->enqueueMarkerWithWaitList(
        1,
        &outputEvent3,
        nullptr);
    {
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ2->getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(0u, semaphores.size());
    }
    userEvent1.setStatus(CL_COMPLETE);
    pCmdQ1->finish();
    pCmdQ2->finish();

    {
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ1->getGpgpuCommandStreamReceiver().getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(1u, semaphores.size());
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[0]));
        EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());
        auto node = event2->getMultiRootDeviceTimestampPacketNodes()->peekNodes().at(0);
        EXPECT_EQ(node->getGpuAddress() + node->getContextEndOffset(), semaphoreCmd->getSemaphoreGraphicsAddress());
    }
    {
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ1->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_LE(1u, semaphores.size());
    }
    {
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ2->getGpgpuCommandStreamReceiver().getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(2u, semaphores.size());
        auto semaphoreCmd0 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[0]));
        EXPECT_EQ(1u, semaphoreCmd0->getSemaphoreDataDword());
        auto node = event1->getMultiRootDeviceTimestampPacketNodes()->peekNodes().at(0);
        EXPECT_EQ(node->getGpuAddress() + node->getContextEndOffset(), semaphoreCmd0->getSemaphoreGraphicsAddress());
    }
    {
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ2->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_LE(1u, semaphores.size());
    }
    event1->release();
    event2->release();
    event3->release();
    buffer->release();
    pCmdQ1->release();
    pCmdQ2->release();
}

HWTEST_F(MultiRootDeviceCommandStreamReceiverTests, givenUnflushedQueueAndEventInMultiRootDeviceEnvironmentWhenTheyArePassedToSecondQueueThenFlushSubmissions) {
    auto deviceFactory = std::make_unique<UltClDeviceFactory>(3, 0);
    deviceFactory->rootDevices[1]->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;
    deviceFactory->rootDevices[1]->getUltCommandStreamReceiver<FamilyType>().useNewResourceImplicitFlush = false;

    cl_device_id devices[] = {deviceFactory->rootDevices[1], deviceFactory->rootDevices[2]};

    auto context = std::make_unique<MockContext>(ClDeviceVector(devices, 2), false);
    auto pCmdQ1 = context.get()->getSpecialQueue(1u);
    auto pCmdQ2 = context.get()->getSpecialQueue(2u);

    pCmdQ1->getGpgpuCommandStreamReceiver().overrideDispatchPolicy(DispatchMode::batchedDispatch);
    cl_event outputEvent{};
    cl_event inputEvent;

    pCmdQ1->enqueueMarkerWithWaitList(
        0,
        nullptr,
        &inputEvent);
    pCmdQ1->enqueueMarkerWithWaitList(
        1,
        &inputEvent,
        &outputEvent);

    EXPECT_FALSE(pCmdQ1->getGpgpuCommandStreamReceiver().isLatestTaskCountFlushed());

    pCmdQ2->enqueueMarkerWithWaitList(
        1,
        &outputEvent,
        nullptr);
    EXPECT_TRUE(pCmdQ1->getGpgpuCommandStreamReceiver().isLatestTaskCountFlushed());
    castToObject<Event>(inputEvent)->release();
    castToObject<Event>(outputEvent)->release();
    pCmdQ1->finish();
    pCmdQ2->finish();
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenStaticPartitioningEnabledWhenFlushingTaskThenWorkPartitionAllocationIsMadeResident) {
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableStaticPartitioning.set(1);
    debugManager.flags.EnableImplicitScaling.set(1);
    debugManager.flags.ForcePreemptionMode.set(PreemptionMode::Disabled);
    UltDeviceFactory deviceFactory{1, 2};
    MockDevice *device = deviceFactory.rootDevices[0];
    auto &mockCsr = device->getUltCommandStreamReceiver<FamilyType>();
    ASSERT_NE(nullptr, mockCsr.getWorkPartitionAllocation());

    mockCsr.overrideDispatchPolicy(DispatchMode::batchedDispatch);
    mockCsr.storeMakeResidentAllocations = true;

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    cleanupHeaps();
    initHeaps();
    mockCsr.flushTask(commandStream,
                      0,
                      &dsh,
                      &ioh,
                      &ssh,
                      taskLevel,
                      dispatchFlags,
                      *device);

    bool found = false;
    for (auto allocation : mockCsr.makeResidentAllocations) {
        if (allocation.first == mockCsr.getWorkPartitionAllocation()) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenEnqueueWithoutArbitrationPolicyWhenPolicyIsAlreadyProgrammedThenReuse) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.set(1);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    commandStreamReceiver.streamProperties.initSupport(pDevice->getRootDeviceEnvironment());
    auto &csrThreadArbitrationPolicy = commandStreamReceiver.streamProperties.stateComputeMode.threadArbitrationPolicy.value;

    int32_t sentThreadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobinAfterDependency;

    flushTaskFlags.threadArbitrationPolicy = sentThreadArbitrationPolicy;

    flushTask(commandStreamReceiver);

    EXPECT_EQ(csrThreadArbitrationPolicy, sentThreadArbitrationPolicy);

    flushTaskFlags.threadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;

    flushTask(commandStreamReceiver);

    EXPECT_EQ(csrThreadArbitrationPolicy, sentThreadArbitrationPolicy);
}

struct PreambleThreadArbitrationMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (HwMapper<productFamily>::GfxProduct::supportsCmdSet(IGFX_GEN12LP_CORE)) {
            return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::implementsPreambleThreadArbitration;
        }
        return false;
    }
};

HWTEST2_F(CommandStreamReceiverFlushTaskTests, givenPolicyValueChangedWhenFlushingTaskThenProgramThreadArbitrationPolicy, PreambleThreadArbitrationMatcher) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;

    flushTask(commandStreamReceiver);
    size_t parsingOffset = commandStreamReceiver.commandStream.getUsed();
    for (auto arbitrationChanged : ::testing::Bool()) {
        commandStreamReceiver.streamProperties.stateComputeMode.threadArbitrationPolicy.value =
            arbitrationChanged ? -1 : gfxCoreHelper.getDefaultThreadArbitrationPolicy();

        flushTask(commandStreamReceiver);
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(commandStreamReceiver.commandStream, parsingOffset);
        auto miLoadRegisterCommandsCount = findAll<MI_LOAD_REGISTER_IMM *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end()).size();
        if (arbitrationChanged) {
            EXPECT_GE(miLoadRegisterCommandsCount, 1u);
        } else {
            EXPECT_EQ(0u, miLoadRegisterCommandsCount);
        }
        parsingOffset = commandStreamReceiver.commandStream.getUsed();
    }
}

namespace CpuIntrinsicsTests {
extern volatile TagAddressType *pauseAddress;
extern TaskCountType pauseValue;
} // namespace CpuIntrinsicsTests

using CommandStreamReceiverFlushTaskTestsWithMockCsrHw2 = UltCommandStreamReceiverTestWithCsrT<MockCsrHw2>;

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenTagValueNotMeetingTaskCountToWaitWhenTagValueSwitchesThenWaitFunctionReturnsTrue) {
    VariableBackup<volatile TagAddressType *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<TaskCountType> backupPauseValue(&CpuIntrinsicsTests::pauseValue);
    VariableBackup<WaitUtils::WaitpkgUse> backupWaitpkgUse(&WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::noUse);
    VariableBackup<uint32_t> backupWaitCount(&WaitUtils::waitCount, 1);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());

    TaskCountType taskCountToWait = 2u;

    *mockCsr->tagAddress = 1u;

    CpuIntrinsicsTests::pauseAddress = mockCsr->tagAddress;
    CpuIntrinsicsTests::pauseValue = taskCountToWait;

    const auto ret = mockCsr->waitForCompletionWithTimeout(WaitParams{false, false, false, 1}, taskCountToWait);
    EXPECT_EQ(NEO::WaitStatus::ready, ret);
}

HWTEST_TEMPLATED_F(CommandStreamReceiverFlushTaskTestsWithMockCsrHw2, givenTagValueNotMeetingTaskCountToWaitAndIndefinitelyPollWhenWaitForCompletionThenDoNotCallWaitUtils) {
    VariableBackup<volatile TagAddressType *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<TaskCountType> backupPauseValue(&CpuIntrinsicsTests::pauseValue);
    VariableBackup<WaitUtils::WaitpkgUse> backupWaitpkgUse(&WaitUtils::waitpkgUse, WaitUtils::WaitpkgUse::noUse);
    VariableBackup<uint32_t> backupWaitCount(&WaitUtils::waitCount, 1);

    auto mockCsr = static_cast<MockCsrHw2<FamilyType> *>(&pDevice->getGpgpuCommandStreamReceiver());

    TaskCountType taskCountToWait = 2u;

    *mockCsr->tagAddress = 1u;

    CpuIntrinsicsTests::pauseAddress = mockCsr->tagAddress;
    CpuIntrinsicsTests::pauseValue = taskCountToWait;

    const auto ret = mockCsr->waitForCompletionWithTimeout(WaitParams{true, true, false, 10}, taskCountToWait);
    EXPECT_EQ(NEO::WaitStatus::notReady, ret);
}

HWTEST_F(UltCommandStreamReceiverTest, WhenFlushingAllCachesThenPipeControlIsAdded) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.FlushAllCaches.set(true);

    char buff[sizeof(PIPE_CONTROL) * 3];
    LinearStream stream(buff, sizeof(PIPE_CONTROL) * 3);

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(stream, args);

    parseCommands<FamilyType>(stream, 0);

    PIPE_CONTROL *pipeControl = getCommand<PIPE_CONTROL>();

    ASSERT_NE(nullptr, pipeControl);

    // WA pipeControl added
    if (cmdList.size() == 2) {
        pipeControl++;
    }

    EXPECT_TRUE(pipeControl->getDcFlushEnable());
    EXPECT_TRUE(pipeControl->getRenderTargetCacheFlushEnable());
    EXPECT_TRUE(pipeControl->getInstructionCacheInvalidateEnable());
    EXPECT_TRUE(pipeControl->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getPipeControlFlushEnable());
    EXPECT_TRUE(pipeControl->getVfCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getConstantCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getStateCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getTlbInvalidate());
}

HWTEST_F(UltCommandStreamReceiverTest, givenDebugDisablingCacheFlushWhenAddingPipeControlWithCacheFlushThenOverrideRequestAndDisableCacheFlushFlags) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.DoNotFlushCaches.set(true);

    char buff[sizeof(PIPE_CONTROL) * 3];
    LinearStream stream(buff, sizeof(PIPE_CONTROL) * 3);

    PipeControlArgs args;
    args.dcFlushEnable = true;
    args.constantCacheInvalidationEnable = true;
    args.instructionCacheInvalidateEnable = true;
    args.pipeControlFlushEnable = true;
    args.renderTargetCacheFlushEnable = true;
    args.stateCacheInvalidationEnable = true;
    args.textureCacheInvalidationEnable = true;
    args.vfCacheInvalidationEnable = true;

    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(stream, args);

    parseCommands<FamilyType>(stream, 0);

    PIPE_CONTROL *pipeControl = getCommand<PIPE_CONTROL>();

    ASSERT_NE(nullptr, pipeControl);

    // WA pipeControl added
    if (cmdList.size() == 2) {
        pipeControl++;
    }

    EXPECT_FALSE(pipeControl->getDcFlushEnable());
    EXPECT_FALSE(pipeControl->getRenderTargetCacheFlushEnable());
    EXPECT_FALSE(pipeControl->getInstructionCacheInvalidateEnable());
    EXPECT_FALSE(pipeControl->getTextureCacheInvalidationEnable());
    EXPECT_FALSE(pipeControl->getPipeControlFlushEnable());
    EXPECT_FALSE(pipeControl->getVfCacheInvalidationEnable());
    EXPECT_FALSE(pipeControl->getConstantCacheInvalidationEnable());
    EXPECT_FALSE(pipeControl->getStateCacheInvalidationEnable());
}

struct BcsCrossDeviceMigrationTests : public ::testing::Test {

    template <typename FamilyType>
    class MockCmdQToTestMigration : public CommandQueueHw<FamilyType> {
      public:
        MockCmdQToTestMigration(Context *context, ClDevice *device) : CommandQueueHw<FamilyType>(context, device, nullptr, false) {}

        bool migrateMultiGraphicsAllocationsIfRequired(const BuiltinOpParams &operationParams, CommandStreamReceiver &csr) override {
            migrateMultiGraphicsAllocationsIfRequiredCalled = true;
            migrateMultiGraphicsAllocationsReceivedOperationParams = operationParams;
            migrateMultiGraphicsAllocationsReceivedCsr = &csr;
            return CommandQueueHw<FamilyType>::migrateMultiGraphicsAllocationsIfRequired(operationParams, csr);
        }

        bool migrateMultiGraphicsAllocationsIfRequiredCalled = false;
        BuiltinOpParams migrateMultiGraphicsAllocationsReceivedOperationParams{};
        CommandStreamReceiver *migrateMultiGraphicsAllocationsReceivedCsr = nullptr;
    };

    void SetUp() override {
        VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
        defaultHwInfo->capabilityTable.blitterOperationsSupported = true;

        debugManager.flags.EnableBlitterForEnqueueOperations.set(true);

        deviceFactory = std::make_unique<UltClDeviceFactory>(2, 0);
        auto device1 = deviceFactory->rootDevices[0];
        REQUIRE_FULL_BLITTER_OR_SKIP(device1->getRootDeviceEnvironment());
        auto device2 = deviceFactory->rootDevices[1];
        REQUIRE_FULL_BLITTER_OR_SKIP(device2->getRootDeviceEnvironment());
        cl_device_id devices[] = {device1, device2};

        context = std::make_unique<MockContext>(ClDeviceVector(devices, 2), false);
        auto memoryManager = static_cast<MockMemoryManager *>(context->getMemoryManager());
        for (auto &rootDeviceIndex : context->getRootDeviceIndices()) {
            memoryManager->localMemorySupported[rootDeviceIndex] = true;
        }
    }

    void TearDown() override {
    }

    std::unique_ptr<UltClDeviceFactory> deviceFactory;
    std::unique_ptr<MockContext> context;
    DebugManagerStateRestore restorer;

    template <typename FamilyType>
    std::unique_ptr<MockCmdQToTestMigration<FamilyType>> createCommandQueue(uint32_t rooDeviceIndex) {
        if (rooDeviceIndex < 2) {
            return std::make_unique<MockCmdQToTestMigration<FamilyType>>(context.get(), deviceFactory->rootDevices[rooDeviceIndex]);
        }
        return nullptr;
    }
};

HWTEST_F(BcsCrossDeviceMigrationTests, givenBufferWithMultiStorageWhenEnqueueReadBufferIsCalledThenMigrateBufferToRootDeviceAssociatedWithCommandQueue) {
    uint32_t targetRootDeviceIndex = 1;
    auto cmdQueue = createCommandQueue<FamilyType>(targetRootDeviceIndex);
    ASSERT_NE(nullptr, cmdQueue);

    cl_int retVal = CL_INVALID_VALUE;
    constexpr size_t size = MemoryConstants::pageSize;

    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), 0, size, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    EXPECT_TRUE(buffer->getMultiGraphicsAllocation().requiresMigrations());

    char hostPtr[size]{};

    auto bcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(cmdQueue->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS));
    bcsCsr->flushTagUpdateCalled = false;
    retVal = cmdQueue->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, size, hostPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cmdQueue->finish();

    EXPECT_TRUE(cmdQueue->migrateMultiGraphicsAllocationsIfRequiredCalled);

    EXPECT_EQ(bcsCsr, cmdQueue->migrateMultiGraphicsAllocationsReceivedCsr);
    EXPECT_EQ(targetRootDeviceIndex, bcsCsr->getRootDeviceIndex());
    EXPECT_TRUE(bcsCsr->flushTagUpdateCalled);

    EXPECT_EQ(buffer.get(), cmdQueue->migrateMultiGraphicsAllocationsReceivedOperationParams.srcMemObj);
}

HWTEST_F(CrossDeviceDependenciesTests, givenMultipleEventInMultiRootDeviceEnvironmentWhenTheyDoNotHaveMultiRootSyncNodeThenCsrDepsDoesNotHaveAnyMultiRootSyncContainer) {
    Event event1(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    Event event2(nullptr, CL_COMMAND_NDRANGE_KERNEL, 6, 16);
    Event event3(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 1, 6);
    Event event4(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 4, 20);
    Event event5(pCmdQ2, CL_COMMAND_NDRANGE_KERNEL, 3, 4);
    Event event6(pCmdQ2, CL_COMMAND_NDRANGE_KERNEL, 2, 7);
    UserEvent userEvent1(&pCmdQ1->getContext());
    UserEvent userEvent2(&pCmdQ2->getContext());

    userEvent1.setStatus(CL_COMPLETE);
    userEvent2.setStatus(CL_COMPLETE);
    {
        cl_event eventWaitList[] =
            {
                &event1,
                &event2,
                &event3,
                &event4,
                &event5,
                &event6,
                &userEvent1,
            };
        cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);

        EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, nullptr);
        CsrDependencies csrDeps;
        eventsRequest.fillCsrDependenciesForRootDevices(csrDeps, pCmdQ2->getGpgpuCommandStreamReceiver());

        EXPECT_EQ(0u, csrDeps.multiRootTimeStampSyncContainer.size());
    }
}
HWTEST_F(CrossDeviceDependenciesTests, givenMultipleEventInMultiRootDeviceEnvironmentWhenTheyDoNotHaveMultiRootSyncNodeContainersThenCsrDepsDoesNotHaveAnyMultiRootSyncContainer) {

    MockEvent<Event> event1(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    event1.multiRootDeviceTimestampPacketContainer.reset(new TimestampPacketContainer());
    MockEvent<Event> event2(nullptr, CL_COMMAND_NDRANGE_KERNEL, 6, 16);
    MockEvent<Event> event3(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 4, 20);
    event3.multiRootDeviceTimestampPacketContainer.reset(new TimestampPacketContainer());
    MockEvent<Event> event4(pCmdQ2, CL_COMMAND_NDRANGE_KERNEL, 3, 4);
    event4.multiRootDeviceTimestampPacketContainer.reset(new TimestampPacketContainer());
    MockEvent<Event> event5(pCmdQ2, CL_COMMAND_NDRANGE_KERNEL, 3, 4);
    event5.multiRootDeviceTimestampPacketContainer.reset(new TimestampPacketContainer());
    MockEvent<Event> event6(pCmdQ2, CL_COMMAND_NDRANGE_KERNEL, 2, 7);
    event6.multiRootDeviceTimestampPacketContainer.reset(new TimestampPacketContainer());
    UserEvent userEvent1(&pCmdQ1->getContext());

    userEvent1.setStatus(CL_COMPLETE);

    {
        cl_event eventWaitList[] =
            {
                &event1,
                &event2,
                &event3,
                &event4,
                &event5,
                &event6,
                &userEvent1,
            };
        cl_uint numEventsInWaitList = sizeof(eventWaitList) / sizeof(eventWaitList[0]);

        EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, nullptr);
        CsrDependencies csrDeps;
        eventsRequest.fillCsrDependenciesForRootDevices(csrDeps, pCmdQ2->getGpgpuCommandStreamReceiver());

        EXPECT_EQ(0u, csrDeps.multiRootTimeStampSyncContainer.size());
    }
}
