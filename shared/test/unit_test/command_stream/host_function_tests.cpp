/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/host_function.h"
#include "shared/source/command_stream/host_function_allocator.h"
#include "shared/source/command_stream/tag_allocation_layout.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_host_function_allocator.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <cstddef>
#include <limits>

using namespace NEO;

using HostFunctionTests = Test<DeviceFixture>;

HWTEST_F(HostFunctionTests, givenHostFunctionDataStoredWhenProgramHostFunctionIsCalledThenMiStoresAndSemaphoreWaitAreProgrammedCorrectlyInCorrectOrder) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto dcFlushRequired = pDevice->getGpgpuCommandStreamReceiver().getDcFlushSupport();
    auto partitionOffset = static_cast<uint32_t>(sizeof(uint64_t));

    for (bool isMemorySynchronizationRequired : ::testing::Bool()) {
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
                                                                               isTbx,
                                                                               dcFlushRequired,
                                                                               HasSemaphore64bCmd<FamilyType>);

            HostFunctionHelper<FamilyType>::programHostFunction(stream, *hostFunctionStreamer.get(), std::move(hostFunction), isMemorySynchronizationRequired);

            HardwareParse hwParser;
            hwParser.parseCommands<FamilyType>(stream, 0);

            bool expectPipeControl = dcFlushRequired && isMemorySynchronizationRequired;
            auto hostFunctionProgrammingHwCmd = expectPipeControl ? findAll<PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end())
                                                                  : findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
            EXPECT_EQ(1u, hostFunctionProgrammingHwCmd.size());

            auto miWait = findAll<MI_SEMAPHORE_WAIT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
            EXPECT_EQ(nPartitions, miWait.size());

            // program host function id
            auto expectedHostFunctionId = 1u;
            if (expectPipeControl) {
                auto pipeControl = genCmdCast<PIPE_CONTROL *>(*hostFunctionProgrammingHwCmd[0]);
                EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControl->getPostSyncOperation());
                EXPECT_EQ(getLowPart(hostFunctionIdBaseAddress), pipeControl->getAddress());
                EXPECT_EQ(getHighPart(hostFunctionIdBaseAddress), pipeControl->getAddressHigh());
                EXPECT_EQ(expectedHostFunctionId, pipeControl->getImmediateData());
                EXPECT_TRUE(pipeControl->getDcFlushEnable());
            } else {
                auto miStoreUserHostFunction = genCmdCast<MI_STORE_DATA_IMM *>(*hostFunctionProgrammingHwCmd[0]);
                EXPECT_EQ(hostFunctionIdBaseAddress, miStoreUserHostFunction->getAddress());
                EXPECT_EQ(getLowPart(expectedHostFunctionId), miStoreUserHostFunction->getDataDword0());
                EXPECT_EQ(getHighPart(expectedHostFunctionId), miStoreUserHostFunction->getDataDword1());
                EXPECT_TRUE(miStoreUserHostFunction->getStoreQword());
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
}

HWTEST_F(HostFunctionTests, givenCommandBufferPassedWhenProgramHostFunctionsAreCalledThenMiStoresAndSemaphoreWaitAreProgrammedCorrectlyInCorrectOrder) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto partitionOffset = static_cast<uint32_t>(sizeof(uint64_t));
    auto dcFlushRequired = pDevice->getGpgpuCommandStreamReceiver().getDcFlushSupport();

    for (bool isMemorySynchronizationRequired : ::testing::Bool()) {
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
                                                                               isTbx,
                                                                               dcFlushRequired,
                                                                               HasSemaphore64bCmd<FamilyType>);

            constexpr auto size = 1024u;
            std::byte buff[size] = {};

            uint64_t callbackAddress = 1024;
            uint64_t userDataAddress = 2048;

            HostFunction hostFunction{
                .hostFunctionAddress = callbackAddress,
                .userDataAddress = userDataAddress};

            LinearStream commandStream(buff, size);

            if (dcFlushRequired && isMemorySynchronizationRequired) {
                auto pcBuffer1 = commandStream.getSpaceForCmd<PIPE_CONTROL>();
                HostFunctionHelper<FamilyType>::programHostFunctionId(nullptr, pcBuffer1, *hostFunctionStreamer.get(), std::move(hostFunction), isMemorySynchronizationRequired);
            } else {
                auto miStoreDataImmBuffer1 = commandStream.getSpaceForCmd<MI_STORE_DATA_IMM>();
                HostFunctionHelper<FamilyType>::programHostFunctionId(nullptr, miStoreDataImmBuffer1, *hostFunctionStreamer.get(), std::move(hostFunction), isMemorySynchronizationRequired);
            }

            for (auto partitionId = 0u; partitionId < nPartitions; partitionId++) {
                auto semaphoreCommand = commandStream.getSpaceForCmd<MI_SEMAPHORE_WAIT>();
                HostFunctionHelper<FamilyType>::programHostFunctionWaitForCompletion(nullptr, semaphoreCommand, *hostFunctionStreamer.get(), partitionId);
            }

            HardwareParse hwParser;
            hwParser.parseCommands<FamilyType>(commandStream, 0);

            auto miWait = findAll<MI_SEMAPHORE_WAIT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
            EXPECT_EQ(nPartitions, miWait.size());

            // program host function id
            auto expectedHostFunctionId = 1u;

            if (dcFlushRequired && isMemorySynchronizationRequired) {
                auto pipeControls = findAll<PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
                EXPECT_EQ(1u, pipeControls.size());

                auto pc = genCmdCast<PIPE_CONTROL *>(*pipeControls[0]);
                EXPECT_EQ(getLowPart(hostFunctionIdBaseAddress), pc->getAddress());
                EXPECT_EQ(getHighPart(hostFunctionIdBaseAddress), pc->getAddressHigh());
                EXPECT_EQ(expectedHostFunctionId, pc->getImmediateData());

                if constexpr (requires { &pc->getWorkloadPartitionIdOffsetEnable; }) {
                    bool expectedWorkloadPartitionIdOffset = nPartitions > 1U;
                    EXPECT_EQ(expectedWorkloadPartitionIdOffset, pc->getWorkloadPartitionIdOffsetEnable());
                }

            } else {
                auto miStores = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
                EXPECT_EQ(1u, miStores.size());

                auto miStoreUserHostFunction = genCmdCast<MI_STORE_DATA_IMM *>(*miStores[0]);
                EXPECT_EQ(hostFunctionIdBaseAddress, miStoreUserHostFunction->getAddress());
                EXPECT_EQ(getLowPart(expectedHostFunctionId), miStoreUserHostFunction->getDataDword0());
                EXPECT_EQ(getHighPart(expectedHostFunctionId), miStoreUserHostFunction->getDataDword1());
                EXPECT_TRUE(miStoreUserHostFunction->getStoreQword());

                if constexpr (requires { &miStoreUserHostFunction->getWorkloadPartitionIdOffsetEnable; }) {
                    bool expectedWorkloadPartitionIdOffset = nPartitions > 1U;
                    EXPECT_EQ(expectedWorkloadPartitionIdOffset, miStoreUserHostFunction->getWorkloadPartitionIdOffsetEnable());
                }
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
}

HWTEST_F(HostFunctionTests, givenHostFunctionStreamerWhenProgramHostFunctionIsCalledThenHostFunctionStreamerWasUpdatedWithHostFunction) {

    uint64_t callbackAddress1 = 1024;
    uint64_t userDataAddress1 = 2048;
    uint64_t callbackAddress2 = 4096;
    uint64_t userDataAddress2 = 8192;

    auto partitionOffset = static_cast<uint32_t>(sizeof(uint64_t));
    auto &csr = pDevice->getGpgpuCommandStreamReceiver();
    auto &ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> &>(csr);
    auto dcFlushRequired = csr.getDcFlushSupport();

    for (auto isMemorySynchronizationRequired : ::testing::Bool()) {

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
                                                                                   isTbx,
                                                                                   dcFlushRequired,
                                                                                   HasSemaphore64bCmd<FamilyType>);

                EXPECT_FALSE(hostFunctionStreamer->getHostFunctionReadyToExecute());

                {
                    // 1st host function in order
                    HostFunctionHelper<FamilyType>::programHostFunction(stream, *hostFunctionStreamer.get(), std::move(hostFunction1), isMemorySynchronizationRequired);
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
                    HostFunctionHelper<FamilyType>::programHostFunction(stream, *hostFunctionStreamer.get(), std::move(hostFunction2), isMemorySynchronizationRequired);

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
}

TEST(CommandStreamReceiverHostFunctionsTest, givenCommandStreamReceiverWhenEnsureHostFunctionDataInitializationCalledThenHostFunctionAllocationIsBeingAllocatedOnlyOnce) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    DeviceBitfield devices(0b11);
    auto csr = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, devices);
    executionEnvironment.memoryManager.reset(new OsAgnosticMemoryManager(executionEnvironment));
    MockMemoryManager mockMemoryManager(executionEnvironment);
    uint32_t rootDeviceIndex = csr->getRootDeviceIndex();
    uint32_t maxRootDeviceIndex = static_cast<uint32_t>(executionEnvironment.rootDeviceEnvironments.size() - 1);

    MockHostFunctionAllocator hostFunctionAllocator(&mockMemoryManager,
                                                    csr.get(),
                                                    MemoryConstants::pageSize64k,
                                                    rootDeviceIndex,
                                                    maxRootDeviceIndex,
                                                    csr->getActivePartitions(),
                                                    false);

    csr->ensureHostFunctionWorkerStarted(&hostFunctionAllocator);
    auto *streamer = &csr->getHostFunctionStreamer();
    EXPECT_NE(nullptr, streamer);
    EXPECT_EQ(1u, csr->startHostFunctionWorkerCalledTimes);

    csr->ensureHostFunctionWorkerStarted(&hostFunctionAllocator);
    EXPECT_EQ(streamer, &csr->getHostFunctionStreamer());
    EXPECT_EQ(1u, csr->startHostFunctionWorkerCalledTimes);

    csr->startHostFunctionWorker(&hostFunctionAllocator);
    EXPECT_EQ(2u, csr->startHostFunctionWorkerCalledTimes); // direct call -> the counter updated but due to an early return allocation didn't change
    EXPECT_EQ(streamer, &csr->getHostFunctionStreamer());

    EXPECT_EQ(AllocationType::hostFunction, streamer->getHostFunctionIdAllocation()->getAllocationType());

    auto expectedHostFunctionIdAddress = reinterpret_cast<uint64_t>(streamer->getHostFunctionIdAllocation()->getUnderlyingBuffer());

    EXPECT_EQ(expectedHostFunctionIdAddress, streamer->getHostFunctionIdGpuAddress(0u));

    EXPECT_EQ(expectedHostFunctionIdAddress + csr->immWritePostSyncWriteOffset, streamer->getHostFunctionIdGpuAddress(1u));
}

TEST(CommandStreamReceiverHostFunctionsTest, givenDestructedCommandStreamReceiverWhenEnsureHostFunctionDataInitializationCalledThenHostFunctionAllocationsDeallocated) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    DeviceBitfield devices(0b11);

    auto csr = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, devices);
    executionEnvironment.memoryManager.reset(new OsAgnosticMemoryManager(executionEnvironment));

    MockMemoryManager mockMemoryManager(executionEnvironment);
    uint32_t rootDeviceIndex = csr->getRootDeviceIndex();
    uint32_t maxRootDeviceIndex = static_cast<uint32_t>(executionEnvironment.rootDeviceEnvironments.size() - 1);
    MockHostFunctionAllocator hostFunctionAllocator(&mockMemoryManager,
                                                    csr.get(),
                                                    MemoryConstants::pageSize64k,
                                                    rootDeviceIndex,
                                                    maxRootDeviceIndex,
                                                    csr->getActivePartitions(),
                                                    false);

    csr->ensureHostFunctionWorkerStarted(&hostFunctionAllocator);
    EXPECT_NE(nullptr, csr->getHostFunctionStreamer().getHostFunctionIdAllocation());
    EXPECT_EQ(1u, csr->createHostFunctionWorkerCounter);
}

HWTEST_F(HostFunctionTests, givenDebugFlagForHostFunctionSynchronizationWhenSetToDisableThenStoreDataImmIsProgrammed) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    DebugManagerStateRestore restorer;
    debugManager.flags.UseMemorySynchronizationForHostFunction.set(0);

    auto dcFlushRequired = pDevice->getGpgpuCommandStreamReceiver().getDcFlushSupport();
    auto partitionOffset = static_cast<uint32_t>(sizeof(uint64_t));

    constexpr auto size = 1024u;
    std::byte buff[size] = {};
    LinearStream stream(buff, size);

    uint64_t callbackAddress = 1024;
    uint64_t userDataAddress = 2048;

    HostFunction hostFunction{
        .hostFunctionAddress = callbackAddress,
        .userDataAddress = userDataAddress};

    MockGraphicsAllocation allocation;
    std::vector<uint64_t> hostFunctionId(1u, 0u);
    auto hostFunctionIdBaseAddress = reinterpret_cast<uint64_t>(hostFunctionId.data());

    std::function<void(GraphicsAllocation &)> downloadAllocationImpl = [](GraphicsAllocation &) {};
    bool isTbx = false;

    auto hostFunctionStreamer = std::make_unique<HostFunctionStreamer>(nullptr,
                                                                       &allocation,
                                                                       hostFunctionId.data(),
                                                                       downloadAllocationImpl,
                                                                       1u,
                                                                       partitionOffset,
                                                                       isTbx,
                                                                       dcFlushRequired,
                                                                       HasSemaphore64bCmd<FamilyType>);

    bool memorySynchronizationRequired = true;
    if (debugManager.flags.UseMemorySynchronizationForHostFunction.get() != -1) {
        memorySynchronizationRequired = debugManager.flags.UseMemorySynchronizationForHostFunction.get() == 1;
    }

    HostFunctionHelper<FamilyType>::programHostFunction(stream, *hostFunctionStreamer.get(), std::move(hostFunction), memorySynchronizationRequired);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream, 0);

    bool expectPipeControl = dcFlushRequired && memorySynchronizationRequired;
    EXPECT_FALSE(expectPipeControl);

    auto hostFunctionProgrammingHwCmd = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_EQ(1u, hostFunctionProgrammingHwCmd.size());

    auto miStoreUserHostFunction = genCmdCast<MI_STORE_DATA_IMM *>(*hostFunctionProgrammingHwCmd[0]);
    EXPECT_EQ(hostFunctionIdBaseAddress, miStoreUserHostFunction->getAddress());
    EXPECT_TRUE(miStoreUserHostFunction->getStoreQword());

    auto programmedHostFunction = hostFunctionStreamer->getHostFunction(1u);
    EXPECT_EQ(callbackAddress, programmedHostFunction.hostFunctionAddress);
    EXPECT_EQ(userDataAddress, programmedHostFunction.userDataAddress);
}

HWTEST_F(HostFunctionTests, givenDebugFlagForHostFunctionSynchronizationWhenSetToEnableThenPipeControlIsProgrammedIfDcFlushRequired) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore restorer;
    debugManager.flags.UseMemorySynchronizationForHostFunction.set(1);

    auto dcFlushRequired = pDevice->getGpgpuCommandStreamReceiver().getDcFlushSupport();
    auto partitionOffset = static_cast<uint32_t>(sizeof(uint64_t));

    constexpr auto size = 1024u;
    std::byte buff[size] = {};
    LinearStream stream(buff, size);

    uint64_t callbackAddress = 1024;
    uint64_t userDataAddress = 2048;

    HostFunction hostFunction{
        .hostFunctionAddress = callbackAddress,
        .userDataAddress = userDataAddress};

    MockGraphicsAllocation allocation;
    std::vector<uint64_t> hostFunctionId(1u, 0u);
    auto hostFunctionIdBaseAddress = reinterpret_cast<uint64_t>(hostFunctionId.data());

    std::function<void(GraphicsAllocation &)> downloadAllocationImpl = [](GraphicsAllocation &) {};
    bool isTbx = false;

    auto hostFunctionStreamer = std::make_unique<HostFunctionStreamer>(nullptr,
                                                                       &allocation,
                                                                       hostFunctionId.data(),
                                                                       downloadAllocationImpl,
                                                                       1u,
                                                                       partitionOffset,
                                                                       isTbx,
                                                                       dcFlushRequired,
                                                                       HasSemaphore64bCmd<FamilyType>);

    bool memorySynchronizationRequired = true;
    if (debugManager.flags.UseMemorySynchronizationForHostFunction.get() != -1) {
        memorySynchronizationRequired = debugManager.flags.UseMemorySynchronizationForHostFunction.get() == 1;
    }

    HostFunctionHelper<FamilyType>::programHostFunction(stream, *hostFunctionStreamer.get(), std::move(hostFunction), memorySynchronizationRequired);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream, 0);

    bool expectPipeControl = dcFlushRequired && memorySynchronizationRequired;
    if (expectPipeControl) {
        auto hostFunctionProgrammingHwCmd = findAll<PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        EXPECT_EQ(1u, hostFunctionProgrammingHwCmd.size());

        auto pipeControl = genCmdCast<PIPE_CONTROL *>(*hostFunctionProgrammingHwCmd[0]);
        EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControl->getPostSyncOperation());
        EXPECT_EQ(getLowPart(hostFunctionIdBaseAddress), pipeControl->getAddress());
        EXPECT_EQ(getHighPart(hostFunctionIdBaseAddress), pipeControl->getAddressHigh());
        EXPECT_EQ(1u, pipeControl->getImmediateData());
        EXPECT_TRUE(pipeControl->getDcFlushEnable());
    } else {
        auto hostFunctionProgrammingHwCmd = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        EXPECT_EQ(1u, hostFunctionProgrammingHwCmd.size());
    }
}

HWTEST_F(HostFunctionTests, whenUsePipeControlForHostFunctionIsCalledThenResultIsCorrect) {
    DebugManagerStateRestore restorer;

    for (auto dcFlushRequiredPlatform : ::testing::Bool()) {
        bool expected = dcFlushRequiredPlatform;
        bool result = HostFunctionHelper<FamilyType>::usePipeControlForHostFunction(dcFlushRequiredPlatform);
        EXPECT_EQ(expected, result);
    }

    debugManager.flags.UseMemorySynchronizationForHostFunction.set(0);
    for (auto dcFlushRequiredPlatform : ::testing::Bool()) {
        bool expected = false;
        bool result = HostFunctionHelper<FamilyType>::usePipeControlForHostFunction(dcFlushRequiredPlatform);
        EXPECT_EQ(expected, result);
    }
}

TEST(HostFunctionAllocatorTests, givenSufficientAllocationWhenAllocateChunkThenChunkIsAlignedZeroedAndAllocationIsReused) {
    MockExecutionEnvironment executionEnvironment{defaultHwInfo.get()};
    auto *mockMemoryManager = new MockMemoryManager(executionEnvironment);
    executionEnvironment.memoryManager.reset(mockMemoryManager);
    DeviceBitfield deviceBitfield(1u);
    MockCommandStreamReceiver csr(executionEnvironment, 0u, deviceBitfield);
    size_t poolSize = 2 * HostFunctionAllocator::chunkAlignment;
    HostFunctionAllocator allocator(mockMemoryManager,
                                    &csr,
                                    poolSize,
                                    0,
                                    0,
                                    csr.getActivePartitions(),
                                    false);

    auto firstChunk = allocator.obtainChunk();
    ASSERT_NE(nullptr, firstChunk.allocation);
    ASSERT_NE(nullptr, firstChunk.cpuPtr);
    EXPECT_EQ(0u, *reinterpret_cast<uint64_t *>(firstChunk.cpuPtr));

    auto firstBase = static_cast<uint8_t *>(firstChunk.allocation->getUnderlyingBuffer());
    EXPECT_EQ(firstBase, firstChunk.cpuPtr);

    auto secondChunk = allocator.obtainChunk();
    ASSERT_NE(nullptr, secondChunk.allocation);
    EXPECT_EQ(firstChunk.allocation, secondChunk.allocation);

    auto expectedOffset = HostFunctionAllocator::chunkAlignment;
    EXPECT_EQ(firstChunk.cpuPtr + expectedOffset, secondChunk.cpuPtr);

    for (uint32_t i = 0u; i < 2u; i++) {
        auto slot = reinterpret_cast<uint64_t *>(secondChunk.cpuPtr + i * HostFunctionAllocator::partitionOffset);
        EXPECT_EQ(0u, *slot);
    }
}

TEST(HostFunctionAllocatorTests, givenTooSmallAllocationWhenAllocateChunkThenThrowIsExpected) {
    MockExecutionEnvironment executionEnvironment{defaultHwInfo.get()};
    auto *mockMemoryManager = new MockMemoryManager(executionEnvironment);
    executionEnvironment.memoryManager.reset(mockMemoryManager);
    DeviceBitfield deviceBitfield(1u);
    MockCommandStreamReceiver csr(executionEnvironment, 0u, deviceBitfield);

    size_t poolSize = HostFunctionAllocator::chunkAlignment;
    HostFunctionAllocator allocator(mockMemoryManager,
                                    &csr,
                                    poolSize,
                                    0,
                                    0,
                                    csr.getActivePartitions(),
                                    false);

    auto firstChunk = allocator.obtainChunk();
    ASSERT_NE(nullptr, firstChunk.allocation);

    EXPECT_ANY_THROW(allocator.obtainChunk());
}

TEST(HostFunctionAllocatorTests, givenTbxModeWhenAllocateChunkThenWriteMemoryIsIssuedAndTbxWritableIsRestored) {

    class HostFunctionCsr : public MockCommandStreamReceiver {
      public:
        using MockCommandStreamReceiver::MockCommandStreamReceiver;

        bool writeMemory(GraphicsAllocation &gfxAllocation, bool isChunkCopy, uint64_t gpuVaChunkOffset, size_t chunkSize) override {
            ++writeMemoryCallCount;
            lastIsChunkCopy = isChunkCopy;
            lastChunkSize = chunkSize;
            lastChunkOffset = gpuVaChunkOffset;
            lastAllocation = &gfxAllocation;
            return true;
        }

        uint32_t writeMemoryCallCount = 0u;
        bool lastIsChunkCopy = false;
        size_t lastChunkSize = 0u;
        uint64_t lastChunkOffset = 0u;
        GraphicsAllocation *lastAllocation = nullptr;
    };

    MockExecutionEnvironment executionEnvironment{defaultHwInfo.get()};
    auto *mockMemoryManager = new MockMemoryManager(executionEnvironment);
    executionEnvironment.memoryManager.reset(mockMemoryManager);
    DeviceBitfield deviceBitfield(1u);
    HostFunctionCsr csr(executionEnvironment, 0u, deviceBitfield);
    size_t poolSize = HostFunctionAllocator::chunkAlignment;
    HostFunctionAllocator allocator(mockMemoryManager,
                                    &csr,
                                    poolSize,
                                    0,
                                    0,
                                    csr.getActivePartitions(),
                                    true);

    auto chunk = allocator.obtainChunk();
    ASSERT_NE(nullptr, chunk.allocation);

    constexpr uint32_t allBanks = std::numeric_limits<uint32_t>::max();
    EXPECT_FALSE(chunk.allocation->isTbxWritable(allBanks));

    EXPECT_EQ(1u, csr.writeMemoryCallCount);
    EXPECT_TRUE(csr.lastIsChunkCopy);
    EXPECT_EQ(0u, csr.lastChunkOffset);

    auto expectedAlignedSize = HostFunctionAllocator::chunkAlignment;
    EXPECT_EQ(expectedAlignedSize, csr.lastChunkSize);
    EXPECT_EQ(chunk.allocation, csr.lastAllocation);
}

TEST(HostFunctionAllocatorTests, givenMultipleObtainChunksWhenAllocatorIsDestroyedThenAllAllocationIsFreed) {
    MockExecutionEnvironment executionEnvironment{defaultHwInfo.get()};
    auto *mockMemoryManager = new MockMemoryManager(executionEnvironment);
    executionEnvironment.memoryManager.reset(mockMemoryManager);
    DeviceBitfield deviceBitfield(1u);
    MockCommandStreamReceiver csr(executionEnvironment, 0u, deviceBitfield);
    size_t poolSize = HostFunctionAllocator::chunkAlignment * 2;

    {
        HostFunctionAllocator allocator(executionEnvironment.memoryManager.get(),
                                        &csr,
                                        poolSize,
                                        0,
                                        0,
                                        csr.getActivePartitions(),
                                        false);

        allocator.obtainChunk();
        allocator.obtainChunk();
    }

    EXPECT_GE(mockMemoryManager->freeGraphicsMemoryCalled, 1u);
}
