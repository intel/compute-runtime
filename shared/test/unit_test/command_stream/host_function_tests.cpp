/*
 * Copyright (C) 2025-2026 Intel Corporation
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
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <cstddef>

using namespace NEO;

using HostFunctionTests = Test<DeviceFixture>;

HWTEST_F(HostFunctionTests, givenHostFunctionDataStoredWhenProgramHostFunctionIsCalledThenMiStoresAndSemaphoreWaitAreProgrammedCorrectlyInCorrectOrder) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto partitionOffset = static_cast<uint32_t>(sizeof(uint64_t));
    for (auto nPartitions : {1u, 2u}) {

        constexpr auto size = 1024u;
        std::byte buff[size] = {};
        LinearStream stream(buff, size);

        uint64_t callbackAddress = 1024;
        uint64_t userDataAddress = 2048;

        HostFunction hostFunction{
            .hostFunctionAddress = callbackAddress,
            .userDataAddress = userDataAddress};

        MockGraphicsAllocation allocation;
        std::vector<uint64_t> hostFunctionId(nPartitions, 0u);
        auto hostFunctionIdBaseAddress = reinterpret_cast<uint64_t>(hostFunctionId.data());

        std::function<void(GraphicsAllocation &)> downloadAllocationImpl = [](GraphicsAllocation &) {};
        bool isTbx = false;

        auto hostFunctionStreamer = std::make_unique<HostFunctionStreamer>(nullptr,
                                                                           &allocation,
                                                                           hostFunctionId.data(),
                                                                           downloadAllocationImpl,
                                                                           nPartitions,
                                                                           partitionOffset,
                                                                           isTbx);

        HostFunctionHelper<FamilyType>::programHostFunction(stream, *hostFunctionStreamer.get(), std::move(hostFunction));

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(stream, 0);

        auto miStores = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        EXPECT_EQ(1u, miStores.size());

        auto miWait = findAll<MI_SEMAPHORE_WAIT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        EXPECT_EQ(nPartitions, miWait.size());

        // program host function id
        auto expectedHostFunctionId = 1u;
        auto miStoreUserHostFunction = genCmdCast<MI_STORE_DATA_IMM *>(*miStores[0]);
        EXPECT_EQ(hostFunctionIdBaseAddress, miStoreUserHostFunction->getAddress());
        EXPECT_EQ(getLowPart(expectedHostFunctionId), miStoreUserHostFunction->getDataDword0());
        EXPECT_EQ(getHighPart(expectedHostFunctionId), miStoreUserHostFunction->getDataDword1());
        EXPECT_TRUE(miStoreUserHostFunction->getStoreQword());

        // program wait for host function completion
        for (auto partitionId = 0u; partitionId < nPartitions; partitionId++) {
            auto miWaitTag = genCmdCast<MI_SEMAPHORE_WAIT *>(*miWait[partitionId]);
            auto expectedAddress = hostFunctionIdBaseAddress + partitionId * partitionOffset;
            EXPECT_EQ(expectedAddress, miWaitTag->getSemaphoreGraphicsAddress());
            EXPECT_EQ(static_cast<uint32_t>(HostFunctionStatus::completed), miWaitTag->getSemaphoreDataDword());
            EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_EQUAL_SDD, miWaitTag->getCompareOperation());
            EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE_POLLING_MODE, miWaitTag->getWaitMode());

            if constexpr (requires { &miWaitTag->getWorkloadPartitionIdOffsetEnable; }) {
                EXPECT_FALSE(miWaitTag->getWorkloadPartitionIdOffsetEnable());
            }
        }

        // host function from host function streamer
        auto programmedHostFunction = hostFunctionStreamer->getHostFunction(expectedHostFunctionId);
        EXPECT_EQ(callbackAddress, programmedHostFunction.hostFunctionAddress);
        EXPECT_EQ(userDataAddress, programmedHostFunction.userDataAddress);
    }
}

HWTEST_F(HostFunctionTests, givenCommandBufferPassedWhenProgramHostFunctionsAreCalledThenMiStoresAndSemaphoreWaitAreProgrammedCorrectlyInCorrectOrder) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto partitionOffset = static_cast<uint32_t>(sizeof(uint64_t));

    for (auto nPartitions : {1u, 2u}) {

        MockGraphicsAllocation allocation;
        std::vector<uint64_t> hostFunctionId(nPartitions, 0u);
        auto hostFunctionIdBaseAddress = reinterpret_cast<uint64_t>(hostFunctionId.data());
        std::function<void(GraphicsAllocation &)> downloadAllocationImpl = [](GraphicsAllocation &) {};
        bool isTbx = false;

        auto hostFunctionStreamer = std::make_unique<HostFunctionStreamer>(nullptr,
                                                                           &allocation,
                                                                           hostFunctionId.data(),
                                                                           downloadAllocationImpl,
                                                                           nPartitions,
                                                                           partitionOffset,
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

        for (auto partitionId = 0u; partitionId < nPartitions; partitionId++) {
            auto semaphoreCommand = commandStream.getSpaceForCmd<MI_SEMAPHORE_WAIT>();
            HostFunctionHelper<FamilyType>::programHostFunctionWaitForCompletion(nullptr, semaphoreCommand, *hostFunctionStreamer.get(), partitionId);
        }

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(commandStream, 0);

        auto miStores = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        EXPECT_EQ(1u, miStores.size());

        auto miWait = findAll<MI_SEMAPHORE_WAIT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        EXPECT_EQ(nPartitions, miWait.size());

        // program host function id
        auto expectedHostFunctionId = 1u;
        auto miStoreUserHostFunction = genCmdCast<MI_STORE_DATA_IMM *>(*miStores[0]);
        EXPECT_EQ(hostFunctionIdBaseAddress, miStoreUserHostFunction->getAddress());
        EXPECT_EQ(getLowPart(expectedHostFunctionId), miStoreUserHostFunction->getDataDword0());
        EXPECT_EQ(getHighPart(expectedHostFunctionId), miStoreUserHostFunction->getDataDword1());
        EXPECT_TRUE(miStoreUserHostFunction->getStoreQword());

        if constexpr (requires { &miStoreUserHostFunction->getWorkloadPartitionIdOffsetEnable; }) {
            bool expectedWorkloadPartitionIdOffset = nPartitions > 1U;
            EXPECT_EQ(expectedWorkloadPartitionIdOffset, miStoreUserHostFunction->getWorkloadPartitionIdOffsetEnable());
        }

        // program wait for host function completion
        for (auto partitionId = 0u; partitionId < nPartitions; partitionId++) {
            auto miWaitTag = genCmdCast<MI_SEMAPHORE_WAIT *>(*miWait[partitionId]);
            auto expectedAddress = hostFunctionIdBaseAddress + partitionId * partitionOffset;

            EXPECT_EQ(expectedAddress, miWaitTag->getSemaphoreGraphicsAddress());
            EXPECT_EQ(static_cast<uint32_t>(HostFunctionStatus::completed), miWaitTag->getSemaphoreDataDword());
            EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_EQUAL_SDD, miWaitTag->getCompareOperation());
            EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE_POLLING_MODE, miWaitTag->getWaitMode());

            if constexpr (requires { &miWaitTag->getWorkloadPartitionIdOffsetEnable; }) {
                EXPECT_FALSE(miWaitTag->getWorkloadPartitionIdOffsetEnable());
            }
        }

        // host function from host function streamer
        auto programmedHostFunction = hostFunctionStreamer->getHostFunction(expectedHostFunctionId);
        EXPECT_EQ(callbackAddress, programmedHostFunction.hostFunctionAddress);
        EXPECT_EQ(userDataAddress, programmedHostFunction.userDataAddress);
    }
}

HWTEST_F(HostFunctionTests, givenHostFunctionStreamerWhenProgramHostFunctionIsCalledThenHostFunctionStreamerWasUpdatedWithHostFunction) {

    uint64_t callbackAddress1 = 1024;
    uint64_t userDataAddress1 = 2048;
    uint64_t callbackAddress2 = 4096;
    uint64_t userDataAddress2 = 8192;

    auto partitionOffset = static_cast<uint32_t>(sizeof(uint64_t));
    auto &csr = pDevice->getGpgpuCommandStreamReceiver();
    auto &ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> &>(csr);

    for (auto nPartitions : {1u, 2u}) {

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

            std::vector<uint64_t> hostFunctionData(nPartitions, 0u);

            uint64_t hostFunctionIdAddress = reinterpret_cast<uint64_t>(hostFunctionData.data());
            MockGraphicsAllocation mockAllocation;
            bool downloadAllocationCalled = false;
            std::function<void(GraphicsAllocation &)> downloadAllocationImpl = [&](GraphicsAllocation &) { downloadAllocationCalled = true; };

            ultCsr.writeMemoryParams.totalCallCount = 0;

            auto hostFunctionStreamer = std::make_unique<HostFunctionStreamer>(&csr,
                                                                               &mockAllocation,
                                                                               hostFunctionData.data(),
                                                                               downloadAllocationImpl,
                                                                               nPartitions,
                                                                               partitionOffset,
                                                                               isTbx);

            EXPECT_FALSE(hostFunctionStreamer->getHostFunctionReadyToExecute());

            {
                // 1st host function in order
                HostFunctionHelper<FamilyType>::programHostFunction(stream, *hostFunctionStreamer.get(), std::move(hostFunction1));
                for (auto partitionId = 0u; partitionId < nPartitions; partitionId++) {
                    hostFunctionData[partitionId] = 1u;
                }

                auto programmedHostFunction1 = hostFunctionStreamer->getHostFunction(1u);

                EXPECT_EQ(&mockAllocation, hostFunctionStreamer->getHostFunctionIdAllocation());
                for (auto partitionId = 0u; partitionId < nPartitions; partitionId++) {
                    EXPECT_EQ(hostFunctionIdAddress + partitionId * partitionOffset, hostFunctionStreamer->getHostFunctionIdGpuAddress(partitionId));
                }

                for (auto partitionId = 0u; partitionId < nPartitions; partitionId++) {
                    hostFunctionData[partitionId] = HostFunctionStatus::completed;
                }

                EXPECT_EQ(HostFunctionStatus::completed, hostFunctionStreamer->getHostFunctionReadyToExecute());

                for (auto partitionId = 0u; partitionId < nPartitions; partitionId++) {
                    hostFunctionData[partitionId] = 1u;
                }

                EXPECT_NE(HostFunctionStatus::completed, hostFunctionStreamer->getHostFunctionReadyToExecute());
                EXPECT_EQ(isTbx, downloadAllocationCalled);

                hostFunctionStreamer->prepareForExecution(programmedHostFunction1);

                if (isTbx) {
                    EXPECT_EQ(0u, ultCsr.writeMemoryParams.totalCallCount);
                }
                // next host function must wait, streamer busy until host function is completed
                EXPECT_EQ(HostFunctionStatus::completed, hostFunctionStreamer->getHostFunctionReadyToExecute());
                hostFunctionStreamer->signalHostFunctionCompletion(programmedHostFunction1);

                for (auto partitionId = 0u; partitionId < nPartitions; partitionId++) {
                    EXPECT_EQ(HostFunctionStatus::completed, hostFunctionData[partitionId]); // host function ID should be marked as completed
                }

                EXPECT_EQ(callbackAddress1, programmedHostFunction1.hostFunctionAddress);
                EXPECT_EQ(userDataAddress1, programmedHostFunction1.userDataAddress);

                if (isTbx) {
                    EXPECT_EQ(nPartitions, ultCsr.writeMemoryParams.totalCallCount); // 1st update
                }
            }
            {
                for (auto partitionId = 0u; partitionId < nPartitions; partitionId++) {
                    hostFunctionData[partitionId] = HostFunctionStatus::completed;
                }

                // 2nd host function
                HostFunctionHelper<FamilyType>::programHostFunction(stream, *hostFunctionStreamer.get(), std::move(hostFunction2));

                // simulate function being processed
                uint64_t hostFunctionId = 3u;
                for (auto partitionId = 0u; partitionId < nPartitions; partitionId++) {
                    hostFunctionData[partitionId] = hostFunctionId;
                }

                auto programmedHostFunction2 = hostFunctionStreamer->getHostFunction(hostFunctionId);

                EXPECT_EQ(&mockAllocation, hostFunctionStreamer->getHostFunctionIdAllocation());
                for (auto partitionId = 0u; partitionId < nPartitions; partitionId++) {
                    EXPECT_EQ(hostFunctionIdAddress + partitionId * partitionOffset, hostFunctionStreamer->getHostFunctionIdGpuAddress(partitionId));
                }

                for (auto partitionId = 0u; partitionId < nPartitions; partitionId++) {
                    hostFunctionData[partitionId] = HostFunctionStatus::completed;
                }
                EXPECT_EQ(HostFunctionStatus::completed, hostFunctionStreamer->getHostFunctionReadyToExecute());

                hostFunctionId = hostFunctionStreamer->getNextHostFunctionIdAndIncrement();
                for (auto partitionId = 0u; partitionId < nPartitions; partitionId++) {
                    hostFunctionData[partitionId] = hostFunctionId;
                }

                if (isTbx) {
                    EXPECT_EQ(nPartitions, ultCsr.writeMemoryParams.totalCallCount);
                }

                EXPECT_NE(HostFunctionStatus::completed, hostFunctionStreamer->getHostFunctionReadyToExecute());
                EXPECT_EQ(isTbx, downloadAllocationCalled);

                hostFunctionStreamer->prepareForExecution(programmedHostFunction2);
                hostFunctionStreamer->signalHostFunctionCompletion(programmedHostFunction2);

                for (auto partitionId = 0u; partitionId < nPartitions; partitionId++) {
                    EXPECT_EQ(HostFunctionStatus::completed, hostFunctionData[partitionId]); // host function ID should be marked as completed
                }

                if (isTbx) {
                    EXPECT_EQ(2 * nPartitions, ultCsr.writeMemoryParams.totalCallCount); // 2nd update
                }

                EXPECT_EQ(callbackAddress2, programmedHostFunction2.hostFunctionAddress);
                EXPECT_EQ(userDataAddress2, programmedHostFunction2.userDataAddress);
            }
            {
                // no more programmed Host Functions
                EXPECT_EQ(HostFunctionStatus::completed, hostFunctionStreamer->getHostFunctionReadyToExecute());
            }
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

    EXPECT_EQ(expectedHostFunctionIdAddress, streamer->getHostFunctionIdGpuAddress(0u));

    EXPECT_EQ(expectedHostFunctionIdAddress + csr->immWritePostSyncWriteOffset, streamer->getHostFunctionIdGpuAddress(1u));
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
