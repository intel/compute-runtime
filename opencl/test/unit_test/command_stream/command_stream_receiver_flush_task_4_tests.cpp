/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/wait_status.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/event/user_event.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "test_traits_common.h"

using namespace NEO;

using MultiRootDeviceCommandStreamReceiverBufferTests = MultiRootDeviceFixture;

HWTEST_F(MultiRootDeviceCommandStreamReceiverBufferTests, givenMultipleEventInMultiRootDeviceEnvironmentWhenTheyArePassedToEnqueueWithSubmissionThenCsIsWaitingForEventsFromPreviousDevices) {
    REQUIRE_SVM_OR_SKIP(device1);
    REQUIRE_SVM_OR_SKIP(device2);

    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    cl_int retVal = 0;
    size_t offset = 0;
    size_t size = 1;

    auto pCmdQ1 = context.get()->getSpecialQueue(1u);
    auto pCmdQ2 = context.get()->getSpecialQueue(2u);

    std::unique_ptr<MockProgram> program(Program::createBuiltInFromSource<MockProgram>("FillBufferBytes", context.get(), context.get()->getDevices(), &retVal));
    program->build(program->getDevices(), nullptr, false);
    std::unique_ptr<MockKernel> kernel(Kernel::create<MockKernel>(program.get(), program->getKernelInfoForKernel("FillBufferBytes"), *context.get()->getDevice(0), &retVal));

    size_t svmSize = 4096;
    void *svmPtr = alignedMalloc(svmSize, MemoryConstants::pageSize);
    MockGraphicsAllocation svmAlloc(svmPtr, svmSize);

    Event event1(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    Event event2(nullptr, CL_COMMAND_NDRANGE_KERNEL, 6, 16);
    Event event3(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 4, 20);
    Event event4(pCmdQ2, CL_COMMAND_NDRANGE_KERNEL, 3, 4);
    Event event5(pCmdQ2, CL_COMMAND_NDRANGE_KERNEL, 2, 7);
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
        EXPECT_EQ(4u, semaphoreCmd0->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(pCmdQ2->getGpgpuCommandStreamReceiver().getTagAddress()), semaphoreCmd0->getSemaphoreGraphicsAddress());

        auto semaphoreCmd1 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[1]));
        EXPECT_EQ(7u, semaphoreCmd1->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(pCmdQ2->getGpgpuCommandStreamReceiver().getTagAddress()), semaphoreCmd1->getSemaphoreGraphicsAddress());
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
        EXPECT_EQ(15u, semaphoreCmd0->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(pCmdQ1->getGpgpuCommandStreamReceiver().getTagAddress()), semaphoreCmd0->getSemaphoreGraphicsAddress());

        auto semaphoreCmd1 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[1]));
        EXPECT_EQ(20u, semaphoreCmd1->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(pCmdQ1->getGpgpuCommandStreamReceiver().getTagAddress()), semaphoreCmd1->getSemaphoreGraphicsAddress());
    }
    alignedFree(svmPtr);
}

using CommandStreamReceiverFlushTaskTests = UltCommandStreamReceiverTest;
using MultiRootDeviceCommandStreamReceiverTests = CommandStreamReceiverFlushTaskTests;

HWTEST_F(MultiRootDeviceCommandStreamReceiverTests, givenMultipleEventInMultiRootDeviceEnvironmentWhenTheyArePassedToEnqueueWithoutSubmissionThenCsIsWaitingForEventsFromPreviousDevices) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto deviceFactory = std::make_unique<UltClDeviceFactory>(4, 0);
    auto device1 = deviceFactory->rootDevices[1];
    auto device2 = deviceFactory->rootDevices[2];
    auto device3 = deviceFactory->rootDevices[3];

    auto mockCsr1 = new MockCommandStreamReceiver(*device1->executionEnvironment, device1->getRootDeviceIndex(), device1->getDeviceBitfield());
    auto mockCsr2 = new MockCommandStreamReceiver(*device2->executionEnvironment, device2->getRootDeviceIndex(), device2->getDeviceBitfield());
    auto mockCsr3 = new MockCommandStreamReceiver(*device3->executionEnvironment, device3->getRootDeviceIndex(), device3->getDeviceBitfield());

    device1->resetCommandStreamReceiver(mockCsr1);
    device2->resetCommandStreamReceiver(mockCsr2);
    device3->resetCommandStreamReceiver(mockCsr3);

    cl_device_id devices[] = {device1, device2, device3};

    auto context = std::make_unique<MockContext>(ClDeviceVector(devices, 3), false);

    auto pCmdQ1 = context.get()->getSpecialQueue(1u);
    auto pCmdQ2 = context.get()->getSpecialQueue(2u);
    auto pCmdQ3 = context.get()->getSpecialQueue(3u);

    Event event1(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 5, 15);
    Event event2(nullptr, CL_COMMAND_NDRANGE_KERNEL, 6, 16);
    Event event3(pCmdQ1, CL_COMMAND_NDRANGE_KERNEL, 4, 20);
    Event event4(pCmdQ2, CL_COMMAND_NDRANGE_KERNEL, 3, 4);
    Event event5(pCmdQ3, CL_COMMAND_NDRANGE_KERNEL, 7, 21);
    Event event6(pCmdQ2, CL_COMMAND_NDRANGE_KERNEL, 2, 7);
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
        EXPECT_EQ(4u, semaphoreCmd0->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(pCmdQ2->getGpgpuCommandStreamReceiver().getTagAddress()), semaphoreCmd0->getSemaphoreGraphicsAddress());

        auto semaphoreCmd1 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[1]));
        EXPECT_EQ(21u, semaphoreCmd1->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(pCmdQ3->getGpgpuCommandStreamReceiver().getTagAddress()), semaphoreCmd1->getSemaphoreGraphicsAddress());

        auto semaphoreCmd2 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[2]));
        EXPECT_EQ(7u, semaphoreCmd2->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(pCmdQ2->getGpgpuCommandStreamReceiver().getTagAddress()), semaphoreCmd2->getSemaphoreGraphicsAddress());
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
        EXPECT_EQ(15u, semaphoreCmd0->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(pCmdQ1->getGpgpuCommandStreamReceiver().getTagAddress()), semaphoreCmd0->getSemaphoreGraphicsAddress());

        auto semaphoreCmd1 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[1]));
        EXPECT_EQ(20u, semaphoreCmd1->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(pCmdQ1->getGpgpuCommandStreamReceiver().getTagAddress()), semaphoreCmd1->getSemaphoreGraphicsAddress());

        auto semaphoreCmd2 = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[2]));
        EXPECT_EQ(21u, semaphoreCmd2->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(pCmdQ3->getGpgpuCommandStreamReceiver().getTagAddress()), semaphoreCmd2->getSemaphoreGraphicsAddress());
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
        EXPECT_EQ(15u, semaphoreCmd0->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(pCmdQ1->getGpgpuCommandStreamReceiver().getTagAddress()), semaphoreCmd0->getSemaphoreGraphicsAddress());
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

        pCmdQ1 = context.get()->getSpecialQueue(1u);
        pCmdQ2 = context.get()->getSpecialQueue(2u);
    }

    void TearDown() override {
    }

    std::unique_ptr<UltClDeviceFactory> deviceFactory;
    std::unique_ptr<MockContext> context;

    CommandQueue *pCmdQ1 = nullptr;
    CommandQueue *pCmdQ2 = nullptr;
};

HWTEST_F(CrossDeviceDependenciesTests, givenMultipleEventInMultiRootDeviceEnvironmentWhenTheyArePassedToMarkerThenMiSemaphoreWaitCommandSizeIsIncluded) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

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
        eventsRequest.fillCsrDependenciesForTaskCountContainer(csrDeps, pCmdQ1->getGpgpuCommandStreamReceiver());

        EXPECT_EQ(0u, csrDeps.taskCountContainer.size());
        EXPECT_EQ(0u, TimestampPacketHelper::getRequiredCmdStreamSizeForTaskCountContainer<FamilyType>(csrDeps));
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
        eventsRequest.fillCsrDependenciesForTaskCountContainer(csrDeps, pCmdQ2->getGpgpuCommandStreamReceiver());

        EXPECT_EQ(3u, csrDeps.taskCountContainer.size());
        EXPECT_EQ(3u * sizeof(MI_SEMAPHORE_WAIT), TimestampPacketHelper::getRequiredCmdStreamSizeForTaskCountContainer<FamilyType>(csrDeps));
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
        csHwParser.parseCommands<FamilyType>(pCmdQ1->getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(0u, semaphores.size());
    }
    userEvent1.setStatus(CL_COMPLETE);
    event1->release();
    event2->release();
    pCmdQ1->finish();
    pCmdQ2->finish();
    {
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ1->getGpgpuCommandStreamReceiver().getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(1u, semaphores.size());
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[0]));
        EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(pCmdQ2->getGpgpuCommandStreamReceiver().getTagAddress()), semaphoreCmd->getSemaphoreGraphicsAddress());
    }
    {
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ2->getGpgpuCommandStreamReceiver().getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(1u, semaphores.size());
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[0]));
        EXPECT_EQ(0u, semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(pCmdQ1->getGpgpuCommandStreamReceiver().getTagAddress()), semaphoreCmd->getSemaphoreGraphicsAddress());
    }
    buffer->release();
}

HWTEST_F(CrossDeviceDependenciesTests, givenWaitListWithEventBlockedByUserEventWhenProgrammingSingleDeviceDependenciesForGpgpuCsrThenNoSemaphoreWaitIsProgrammed) {
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

    pCmdQ1->enqueueReadBuffer(buffer, CL_FALSE, 0, MemoryConstants::pageSize, hostPtr, nullptr,
                              1,
                              &outputEvent1,
                              &outputEvent2);
    {
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ1->getCS(0));
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
        csHwParser.parseCommands<FamilyType>(pCmdQ1->getCS(0));
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
    DebugManager.flags.EnableBlitterForEnqueueOperations.set(true);

    for (auto &rootDeviceEnvironment : deviceFactory->rootDevices[0]->getExecutionEnvironment()->rootDeviceEnvironments) {
        REQUIRE_FULL_BLITTER_OR_SKIP(rootDeviceEnvironment->getHardwareInfo());
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
    event1->release();
    event2->release();
    event3->release();
    pCmdQ1->finish();
    pCmdQ2->finish();

    {
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ1->getGpgpuCommandStreamReceiver().getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_EQ(1u, semaphores.size());
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*(semaphores[0]));
        EXPECT_EQ(1u, semaphoreCmd->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(pCmdQ2->getGpgpuCommandStreamReceiver().getTagAddress()), semaphoreCmd->getSemaphoreGraphicsAddress());
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
        EXPECT_EQ(0u, semaphoreCmd0->getSemaphoreDataDword());
        EXPECT_EQ(reinterpret_cast<uint64_t>(pCmdQ1->getGpgpuCommandStreamReceiver().getTagAddress()), semaphoreCmd0->getSemaphoreGraphicsAddress());
    }
    {
        HardwareParse csHwParser;
        csHwParser.parseCommands<FamilyType>(pCmdQ2->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS)->getCS(0));
        auto semaphores = findAll<MI_SEMAPHORE_WAIT *>(csHwParser.cmdList.begin(), csHwParser.cmdList.end());

        EXPECT_LE(1u, semaphores.size());
    }
    buffer->release();
    pCmdQ1->release();
    pCmdQ2->release();
}

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenStaticPartitioningEnabledWhenFlushingTaskThenWorkPartitionAllocationIsMadeResident) {
    DebugManagerStateRestore restore{};
    DebugManager.flags.EnableStaticPartitioning.set(1);
    DebugManager.flags.EnableImplicitScaling.set(1);
    DebugManager.flags.ForcePreemptionMode.set(PreemptionMode::Disabled);
    UltDeviceFactory deviceFactory{1, 2};
    MockDevice *device = deviceFactory.rootDevices[0];
    auto &mockCsr = device->getUltCommandStreamReceiver<FamilyType>();
    ASSERT_NE(nullptr, mockCsr.getWorkPartitionAllocation());

    mockCsr.overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr.storeMakeResidentAllocations = true;

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    mockCsr.flushTask(commandStream,
                      0,
                      dsh,
                      ioh,
                      ssh,
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
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
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
        if constexpr (HwMapper<productFamily>::GfxProduct::supportsCmdSet(IGFX_GEN8_CORE)) {
            return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::implementsPreambleThreadArbitration;
        }
        return false;
    }
};

HWTEST2_F(CommandStreamReceiverFlushTaskTests, givenPolicyValueChangedWhenFlushingTaskThenProgramThreadArbitrationPolicy, PreambleThreadArbitrationMatcher) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    auto &hwHelper = HwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;

    flushTask(commandStreamReceiver);
    size_t parsingOffset = commandStreamReceiver.commandStream.getUsed();
    for (auto arbitrationChanged : ::testing::Bool()) {
        commandStreamReceiver.streamProperties.stateComputeMode.threadArbitrationPolicy.value =
            arbitrationChanged ? -1 : hwHelper.getDefaultThreadArbitrationPolicy();

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
extern volatile uint32_t *pauseAddress;
extern uint32_t pauseValue;
} // namespace CpuIntrinsicsTests

HWTEST_F(CommandStreamReceiverFlushTaskTests, givenTagValueNotMeetingTaskCountToWaitWhenTagValueSwitchesThenWaitFunctionReturnsTrue) {
    VariableBackup<volatile uint32_t *> backupPauseAddress(&CpuIntrinsicsTests::pauseAddress);
    VariableBackup<uint32_t> backupPauseValue(&CpuIntrinsicsTests::pauseValue);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCsr);

    uint32_t taskCountToWait = 2u;

    *mockCsr->tagAddress = 1u;

    CpuIntrinsicsTests::pauseAddress = mockCsr->tagAddress;
    CpuIntrinsicsTests::pauseValue = taskCountToWait;

    const auto ret = mockCsr->waitForCompletionWithTimeout(false, 1, taskCountToWait);
    EXPECT_EQ(NEO::WaitStatus::Ready, ret);
}
