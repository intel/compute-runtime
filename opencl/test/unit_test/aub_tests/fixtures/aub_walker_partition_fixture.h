/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/walker_partition_xehp_and_later.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/command_stream/aub_command_stream_fixture.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/fixtures/simple_arg_kernel_fixture.h"

extern bool generateRandomInput;

struct DispatchParameters {
    size_t globalWorkSize[3];
    size_t localWorkSize[3];
};

extern DispatchParameters dispatchParametersForTests[];

struct AubWalkerPartitionFixture : public KernelAUBFixture<SimpleKernelFixture> {
    void setUp();

    void tearDown();

    template <typename FamilyType>
    void validatePartitionProgramming(uint64_t postSyncAddress, int32_t partitionCount) {
        using WalkerType = typename FamilyType::DefaultWalkerType;
        using PostSyncType = decltype(FamilyType::template getPostSyncType<WalkerType>());
        uint32_t totalWorkgroupCount = 1u;
        uint32_t totalWorkItemsInWorkgroup = 1u;
        uint32_t totalWorkItemsCount = 1;

        for (auto dimension = 0u; dimension < workingDimensions; dimension++) {
            totalWorkgroupCount *= static_cast<uint32_t>(dispatchParamters.globalWorkSize[dimension] / dispatchParamters.localWorkSize[dimension]);
            totalWorkItemsInWorkgroup *= static_cast<uint32_t>(dispatchParamters.localWorkSize[dimension]);
            totalWorkItemsCount *= static_cast<uint32_t>(dispatchParamters.globalWorkSize[dimension]);
        }

        const uint32_t workgroupCount = static_cast<uint32_t>(dispatchParamters.globalWorkSize[partitionType - 1] / dispatchParamters.localWorkSize[partitionType - 1]);
        auto partitionSize = Math::divideAndRoundUp(workgroupCount, partitionCount);

        if (static_cast<uint32_t>(partitionType) > workingDimensions) {
            partitionSize = 1;
        }

        hwParser.parseCommands<FamilyType>(pCmdQ->getCS(0), 0);
        hwParser.findHardwareCommands<FamilyType>();

        auto walkerCmd = genCmdCast<WalkerType *>(*hwParser.itorWalker);

        EXPECT_EQ(0u, walkerCmd->getPartitionId());

        if (partitionCount > 1) {
            EXPECT_TRUE(walkerCmd->getWorkloadPartitionEnable());
            EXPECT_EQ(partitionSize, walkerCmd->getPartitionSize());
            EXPECT_EQ(partitionType, walkerCmd->getPartitionType());
        } else {
            EXPECT_FALSE(walkerCmd->getWorkloadPartitionEnable());
            EXPECT_EQ(0u, walkerCmd->getPartitionSize());
            EXPECT_EQ(0u, walkerCmd->getPartitionType());
        }

        EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP, walkerCmd->getPostSync().getOperation());
        EXPECT_EQ(postSyncAddress, walkerCmd->getPostSync().getDestinationAddress());

        int notExpectedValue[] = {1, 1, 1, 1};

        for (auto partitionId = 0; partitionId < debugManager.flags.ExperimentalSetWalkerPartitionCount.get(); partitionId++) {
            expectNotEqualMemory<FamilyType>(reinterpret_cast<void *>(postSyncAddress), &notExpectedValue, sizeof(notExpectedValue));
            postSyncAddress += 16; // next post sync needs to be right after the previous one
        }

        auto dstGpuAddress = addrToPtr(ptrOffset(dstBuffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress(), dstBuffer->getOffset()));
        expectMemory<FamilyType>(dstGpuAddress, &totalWorkItemsCount, sizeof(uint32_t));
        auto groupSpecificWorkCounts = ptrOffset(dstGpuAddress, 4);
        StackVec<uint32_t, 8> workgroupCounts;
        workgroupCounts.resize(totalWorkgroupCount);

        for (uint32_t workgroupId = 0u; workgroupId < totalWorkgroupCount; workgroupId++) {
            workgroupCounts[workgroupId] = totalWorkItemsInWorkgroup;
        }

        expectMemory<FamilyType>(groupSpecificWorkCounts, workgroupCounts.begin(), workgroupCounts.size() * sizeof(uint32_t));
    }

    template <typename FamilyType>
    typename FamilyType::PIPE_CONTROL *retrieveSyncPipeControl(void *startAddress,
                                                               const RootDeviceEnvironment &rootDeviceEnvironment) {
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

        uint8_t buffer[256];
        LinearStream stream(buffer, 256);
        MemorySynchronizationCommands<FamilyType>::addBarrierWa(stream, 0ull, rootDeviceEnvironment, NEO::PostSyncMode::immediateData);
        void *syncPipeControlAddress = reinterpret_cast<void *>(reinterpret_cast<size_t>(startAddress) + stream.getUsed());
        PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(syncPipeControlAddress);
        return pipeControl;
    }

    std::unique_ptr<DebugManagerStateRestore> debugRestorer;
    std::unique_ptr<Buffer> dstBuffer;
    size_t sizeUserMemory = 0;

    cl_uint workingDimensions = 1;
    int32_t partitionCount;
    int32_t partitionType;

    HardwareParse hwParser;
    DispatchParameters dispatchParamters;
};

struct AubWalkerPartitionTest : public AubWalkerPartitionFixture,
                                public ::testing::TestWithParam<std::tuple<int32_t, int32_t, DispatchParameters, uint32_t>> {
    void SetUp();
    void TearDown();
};

struct AubWalkerPartitionZeroFixture : public AubWalkerPartitionFixture {
    void setUp();
    void tearDown();

    void flushStream();

    std::unique_ptr<LinearStream> taskStream;
    GraphicsAllocation *streamAllocation = nullptr;
    GraphicsAllocation *helperSurface = nullptr;
    std::unique_ptr<AllocationProperties> commandBufferProperties;
};
