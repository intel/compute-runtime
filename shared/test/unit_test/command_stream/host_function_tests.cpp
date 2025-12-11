/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/host_function.h"
#include "shared/source/command_stream/tag_allocation_layout.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <cstddef>

using namespace NEO;

using HostFunctionTests = Test<DeviceFixture>;

HWTEST_F(HostFunctionTests, givenHostFunctionDataStoredWhenProgramHostFunctionIsCalledThenMiStoresAndSemaphoreWaitAreProgrammedCorrectlyInCorrectOrder) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    constexpr auto size = 1024u;
    std::byte buff[size] = {};
    LinearStream stream(buff, size);

    uint64_t callbackAddress = 1024;
    uint64_t userDataAddress = 2048;

    HostFunction hostFunction{
        .hostFunctionAddress = callbackAddress,
        .userDataAddress = userDataAddress};

    MockGraphicsAllocation allocation;

    uint64_t hostFunctionId = 1;

    std::function<void(GraphicsAllocation &)> downloadAllocationImpl = [](GraphicsAllocation &) {};
    bool isTbx = false;

    auto hostFunctionStreamer = std::make_unique<HostFunctionStreamer>(&allocation,
                                                                       &hostFunctionId,
                                                                       downloadAllocationImpl,
                                                                       isTbx);

    HostFunctionHelper<FamilyType>::programHostFunction(stream, *hostFunctionStreamer.get(), std::move(hostFunction));

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream, 0);

    auto miStores = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_EQ(1u, miStores.size());

    auto miWait = findAll<MI_SEMAPHORE_WAIT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_EQ(1u, miWait.size());

    // program host function id
    auto expectedHostFunctionId = 1u;
    auto miStoreUserHostFunction = genCmdCast<MI_STORE_DATA_IMM *>(*miStores[0]);
    EXPECT_EQ(reinterpret_cast<uint64_t>(&hostFunctionId), miStoreUserHostFunction->getAddress());
    EXPECT_EQ(getLowPart(expectedHostFunctionId), miStoreUserHostFunction->getDataDword0());
    EXPECT_EQ(getHighPart(expectedHostFunctionId), miStoreUserHostFunction->getDataDword1());
    EXPECT_TRUE(miStoreUserHostFunction->getStoreQword());

    // program wait for host function completion
    auto miWaitTag = genCmdCast<MI_SEMAPHORE_WAIT *>(*miWait[0]);
    EXPECT_EQ(reinterpret_cast<uint64_t>(&hostFunctionId), miWaitTag->getSemaphoreGraphicsAddress());
    EXPECT_EQ(static_cast<uint32_t>(HostFunctionStatus::completed), miWaitTag->getSemaphoreDataDword());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_EQUAL_SDD, miWaitTag->getCompareOperation());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE_POLLING_MODE, miWaitTag->getWaitMode());

    // host function from host function streamer
    auto programmedHostFunction = hostFunctionStreamer->getHostFunction();
    EXPECT_EQ(callbackAddress, programmedHostFunction.hostFunctionAddress);
    EXPECT_EQ(userDataAddress, programmedHostFunction.userDataAddress);
}

HWTEST_F(HostFunctionTests, givenCommandBufferPassedWhenProgramHostFunctionsAreCalledThenMiStoresAndSemaphoreWaitAreProgrammedCorrectlyInCorrectOrder) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    MockGraphicsAllocation allocation;

    uint64_t hostFunctionId = 1;

    std::function<void(GraphicsAllocation &)> downloadAllocationImpl = [](GraphicsAllocation &) {};
    bool isTbx = false;

    auto hostFunctionStreamer = std::make_unique<HostFunctionStreamer>(&allocation,
                                                                       &hostFunctionId,
                                                                       downloadAllocationImpl,
                                                                       isTbx);

    constexpr auto size = 1024u;
    std::byte buff[size] = {};

    uint64_t callbackAddress = 1024;
    uint64_t userDataAddress = 2048;

    HostFunction hostFunction{
        .hostFunctionAddress = callbackAddress,
        .userDataAddress = userDataAddress};

    LinearStream commandStream(buff, size);

    auto miStoreDataImmBuffer1 = commandStream.getSpaceForCmd<MI_STORE_DATA_IMM>();
    HostFunctionHelper<FamilyType>::programHostFunctionId(nullptr, miStoreDataImmBuffer1, *hostFunctionStreamer.get(), std::move(hostFunction));

    auto semaphoreCommand = commandStream.getSpaceForCmd<MI_SEMAPHORE_WAIT>();
    HostFunctionHelper<FamilyType>::programHostFunctionWaitForCompletion(nullptr, semaphoreCommand, *hostFunctionStreamer.get());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream, 0);

    auto miStores = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_EQ(1u, miStores.size());

    auto miWait = findAll<MI_SEMAPHORE_WAIT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_EQ(1u, miWait.size());

    // program host function id
    auto expectedHostFunctionId = 1u;
    auto miStoreUserHostFunction = genCmdCast<MI_STORE_DATA_IMM *>(*miStores[0]);
    EXPECT_EQ(reinterpret_cast<uint64_t>(&hostFunctionId), miStoreUserHostFunction->getAddress());
    EXPECT_EQ(getLowPart(expectedHostFunctionId), miStoreUserHostFunction->getDataDword0());
    EXPECT_EQ(getHighPart(expectedHostFunctionId), miStoreUserHostFunction->getDataDword1());
    EXPECT_TRUE(miStoreUserHostFunction->getStoreQword());

    // program wait for host function completion
    auto miWaitTag = genCmdCast<MI_SEMAPHORE_WAIT *>(*miWait[0]);
    EXPECT_EQ(reinterpret_cast<uint64_t>(&hostFunctionId), miWaitTag->getSemaphoreGraphicsAddress());
    EXPECT_EQ(static_cast<uint32_t>(HostFunctionStatus::completed), miWaitTag->getSemaphoreDataDword());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_EQUAL_SDD, miWaitTag->getCompareOperation());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE_POLLING_MODE, miWaitTag->getWaitMode());

    // host function from host function streamer
    auto programmedHostFunction = hostFunctionStreamer->getHostFunction();
    EXPECT_EQ(callbackAddress, programmedHostFunction.hostFunctionAddress);
    EXPECT_EQ(userDataAddress, programmedHostFunction.userDataAddress);
}

HWTEST_F(HostFunctionTests, givenHostFunctionStreamerWhenProgramHostFunctionIsCalledThenHostFunctionStreamerWasUpdatedWithHostFunction) {

    uint64_t callbackAddress1 = 1024;
    uint64_t userDataAddress1 = 2048;
    uint64_t callbackAddress2 = 4096;
    uint64_t userDataAddress2 = 8192;

    constexpr auto size = 4096u;
    std::byte buff[size] = {};
    LinearStream stream(buff, size);

    for (bool isTbx : ::testing::Bool()) {

        HostFunction hostFunction1{
            .hostFunctionAddress = callbackAddress1,
            .userDataAddress = userDataAddress1};

        HostFunction hostFunction2{
            .hostFunctionAddress = callbackAddress2,
            .userDataAddress = userDataAddress2};

        uint64_t hostFunctionId = HostFunctionStatus::completed;
        uint64_t hostFunctionIdAddress = reinterpret_cast<uint64_t>(&hostFunctionId);
        MockGraphicsAllocation mockAllocation;
        bool downloadAllocationCalled = false;
        std::function<void(GraphicsAllocation &)> downloadAllocationImpl = [&](GraphicsAllocation &) { downloadAllocationCalled = true; };

        auto hostFunctionStreamer = std::make_unique<HostFunctionStreamer>(&mockAllocation,
                                                                           &hostFunctionId,
                                                                           downloadAllocationImpl,
                                                                           isTbx);

        EXPECT_FALSE(hostFunctionStreamer->isHostFunctionReadyToExecute());

        {
            // 1st host function
            HostFunctionHelper<FamilyType>::programHostFunction(stream, *hostFunctionStreamer.get(), std::move(hostFunction1));
            hostFunctionId = 1u; // simulate function being processed

            auto programmedHostFunction1 = hostFunctionStreamer->getHostFunction();

            EXPECT_EQ(&mockAllocation, hostFunctionStreamer->getHostFunctionIdAllocation());
            EXPECT_EQ(hostFunctionIdAddress, hostFunctionStreamer->getHostFunctionIdGpuAddress());

            hostFunctionId = HostFunctionStatus::completed;
            EXPECT_FALSE(hostFunctionStreamer->isHostFunctionReadyToExecute());
            hostFunctionId = 1u;
            EXPECT_TRUE(hostFunctionStreamer->isHostFunctionReadyToExecute());
            EXPECT_EQ(isTbx, downloadAllocationCalled);

            hostFunctionStreamer->prepareForExecution(programmedHostFunction1);

            // next host function must wait, streamer busy until host function is completed
            EXPECT_FALSE(hostFunctionStreamer->isHostFunctionReadyToExecute());
            hostFunctionStreamer->signalHostFunctionCompletion(programmedHostFunction1);
            EXPECT_EQ(HostFunctionStatus::completed, hostFunctionId); // host function ID should be marked as completed

            EXPECT_EQ(callbackAddress1, programmedHostFunction1.hostFunctionAddress);
            EXPECT_EQ(userDataAddress1, programmedHostFunction1.userDataAddress);
        }
        {
            hostFunctionId = HostFunctionStatus::completed;

            // 2nd host function
            HostFunctionHelper<FamilyType>::programHostFunction(stream, *hostFunctionStreamer.get(), std::move(hostFunction2));

            hostFunctionId = 3u; // simulate function being processed

            auto programmedHostFunction2 = hostFunctionStreamer->getHostFunction();

            EXPECT_EQ(&mockAllocation, hostFunctionStreamer->getHostFunctionIdAllocation());
            EXPECT_EQ(hostFunctionIdAddress, hostFunctionStreamer->getHostFunctionIdGpuAddress());

            hostFunctionId = HostFunctionStatus::completed;
            EXPECT_FALSE(hostFunctionStreamer->isHostFunctionReadyToExecute());

            hostFunctionId = hostFunctionStreamer->getNextHostFunctionIdAndIncrement();
            EXPECT_TRUE(hostFunctionStreamer->isHostFunctionReadyToExecute());
            EXPECT_EQ(isTbx, downloadAllocationCalled);

            hostFunctionStreamer->prepareForExecution(programmedHostFunction2);
            hostFunctionStreamer->signalHostFunctionCompletion(programmedHostFunction2);
            EXPECT_EQ(HostFunctionStatus::completed, hostFunctionId); // host function ID should be marked as completed

            EXPECT_EQ(callbackAddress2, programmedHostFunction2.hostFunctionAddress);
            EXPECT_EQ(userDataAddress2, programmedHostFunction2.userDataAddress);
        }
        {
            // no more programmed Host Functions
            EXPECT_FALSE(hostFunctionStreamer->isHostFunctionReadyToExecute());
        }
    }
}

TEST(CommandStreamReceiverHostFunctionsTest, givenCommandStreamReceiverWhenEnsureHostFunctionDataInitializationCalledThenHostFunctionAllocationIsBeingAllocatedOnlyOnce) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    DeviceBitfield devices(0b11);
    auto csr = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, devices);
    executionEnvironment.memoryManager.reset(new OsAgnosticMemoryManager(executionEnvironment));

    csr->initializeTagAllocation();
    csr->ensureHostFunctionWorkerStarted();
    auto *streamer = &csr->getHostFunctionStreamer();
    EXPECT_NE(nullptr, streamer);
    EXPECT_EQ(1u, csr->startHostFunctionWorkerCalledTimes);

    csr->ensureHostFunctionWorkerStarted();
    EXPECT_EQ(streamer, &csr->getHostFunctionStreamer());
    EXPECT_EQ(1u, csr->startHostFunctionWorkerCalledTimes);

    csr->startHostFunctionWorker();
    EXPECT_EQ(2u, csr->startHostFunctionWorkerCalledTimes); // direct call -> the counter updated but due to an early return allocation didn't change
    EXPECT_EQ(streamer, &csr->getHostFunctionStreamer());

    EXPECT_EQ(AllocationType::tagBuffer, streamer->getHostFunctionIdAllocation()->getAllocationType());

    auto expectedHostFunctionIdAddress = reinterpret_cast<uint64_t>(ptrOffset(streamer->getHostFunctionIdAllocation()->getUnderlyingBuffer(),
                                                                              TagAllocationLayout::hostFunctionDataOffset));

    EXPECT_EQ(expectedHostFunctionIdAddress, streamer->getHostFunctionIdGpuAddress());
}

TEST(CommandStreamReceiverHostFunctionsTest, givenDestructedCommandStreamReceiverWhenEnsureHostFunctionDataInitializationCalledThenHostFunctionAllocationsDeallocated) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    DeviceBitfield devices(0b11);

    auto csr = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, devices);
    executionEnvironment.memoryManager.reset(new OsAgnosticMemoryManager(executionEnvironment));
    csr->initializeTagAllocation();

    csr->ensureHostFunctionWorkerStarted();
    EXPECT_NE(nullptr, csr->getHostFunctionStreamer().getHostFunctionIdAllocation());
    EXPECT_EQ(1u, csr->createHostFunctionWorkerCounter);
}
