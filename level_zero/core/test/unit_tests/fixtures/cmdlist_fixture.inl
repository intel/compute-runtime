/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"

#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {

template <typename FamilyType>
void validateTimestampRegisters(GenCmdList &cmdList,
                                GenCmdList::iterator &startIt,
                                uint32_t firstLoadRegisterRegSrcAddress,
                                uint64_t firstStoreRegMemAddress,
                                uint32_t secondLoadRegisterRegSrcAddress,
                                uint64_t secondStoreRegMemAddress,
                                bool workloadPartition,
                                bool useMask) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    constexpr uint32_t mask = 0xfffffffe;

    auto itor = useMask ? find<MI_LOAD_REGISTER_REG *>(startIt, cmdList.end()) : find<MI_STORE_REGISTER_MEM *>(startIt, cmdList.end());
    if (useMask) {
        {
            ASSERT_NE(cmdList.end(), itor);
            auto cmdLoadReg = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
            EXPECT_EQ(firstLoadRegisterRegSrcAddress, cmdLoadReg->getSourceRegisterAddress());
            EXPECT_EQ(RegisterOffsets::csGprR13, cmdLoadReg->getDestinationRegisterAddress());
        }

        itor++;
        {
            ASSERT_NE(cmdList.end(), itor);
            auto cmdLoadImm = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
            EXPECT_EQ(RegisterOffsets::csGprR14, cmdLoadImm->getRegisterOffset());
            EXPECT_EQ(mask, cmdLoadImm->getDataDword());
        }

        itor++;
        {
            ASSERT_NE(cmdList.end(), itor);
            auto cmdMath = genCmdCast<MI_MATH *>(*itor);
            EXPECT_EQ(3u, cmdMath->DW0.BitField.DwordLength);
        }

        itor++;
        {
            ASSERT_NE(cmdList.end(), itor);
            auto cmdMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
            EXPECT_EQ(RegisterOffsets::csGprR12, cmdMem->getRegisterAddress());
            EXPECT_EQ(firstStoreRegMemAddress, cmdMem->getMemoryAddress());
            if (workloadPartition) {
                EXPECT_TRUE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
            } else {
                EXPECT_FALSE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
            }
        }
    } else {
        ASSERT_NE(cmdList.end(), itor);
        auto cmdMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
        EXPECT_EQ(firstLoadRegisterRegSrcAddress, cmdMem->getRegisterAddress());
        EXPECT_EQ(firstStoreRegMemAddress, cmdMem->getMemoryAddress());
        if (workloadPartition) {
            EXPECT_TRUE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
        } else {
            EXPECT_FALSE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
        }
    }

    itor = useMask ? find<MI_LOAD_REGISTER_REG *>(itor, cmdList.end()) : find<MI_STORE_REGISTER_MEM *>(startIt, cmdList.end());
    if (useMask) {
        {
            ASSERT_NE(cmdList.end(), itor);
            auto cmdLoadReg = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
            EXPECT_EQ(secondLoadRegisterRegSrcAddress, cmdLoadReg->getSourceRegisterAddress());
            EXPECT_EQ(RegisterOffsets::csGprR13, cmdLoadReg->getDestinationRegisterAddress());
        }

        itor++;
        {
            ASSERT_NE(cmdList.end(), itor);
            auto cmdLoadImm = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
            EXPECT_EQ(RegisterOffsets::csGprR14, cmdLoadImm->getRegisterOffset());
            EXPECT_EQ(mask, cmdLoadImm->getDataDword());
        }

        itor++;
        {
            ASSERT_NE(cmdList.end(), itor);
            auto cmdMath = genCmdCast<MI_MATH *>(*itor);
            EXPECT_EQ(3u, cmdMath->DW0.BitField.DwordLength);
        }

        itor++;
        {
            ASSERT_NE(cmdList.end(), itor);
            auto cmdMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
            EXPECT_EQ(RegisterOffsets::csGprR12, cmdMem->getRegisterAddress());
            EXPECT_EQ(secondStoreRegMemAddress, cmdMem->getMemoryAddress());
            if (workloadPartition) {
                EXPECT_TRUE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
            } else {
                EXPECT_FALSE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
            }
        }
    } else {
        ASSERT_NE(cmdList.end(), itor);
        auto cmdMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
        EXPECT_EQ(secondLoadRegisterRegSrcAddress, cmdMem->getRegisterAddress());
        EXPECT_EQ(secondStoreRegMemAddress, cmdMem->getMemoryAddress());
        if (workloadPartition) {
            EXPECT_TRUE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
        } else {
            EXPECT_FALSE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
        }
    }

    itor++;
    startIt = itor;
}

template <typename FamilyType>
void validateTimestampLongRegisters(GenCmdList &cmdList,
                                    GenCmdList::iterator &startIt,
                                    uint32_t firstLoadRegisterRegSrcAddress,
                                    uint64_t firstStoreRegMemAddress,
                                    uint32_t secondLoadRegisterRegSrcAddress,
                                    uint64_t secondStoreRegMemAddress,
                                    uint32_t thirdLoadRegisterRegSrcAddress,
                                    uint64_t thirdStoreRegMemAddress,
                                    uint32_t fourthLoadRegisterRegSrcAddress,
                                    uint64_t fourthStoreRegMemAddress,
                                    bool workloadPartition,
                                    bool useMask) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    constexpr uint32_t mask = 0xfffffffe;

    auto itor = useMask ? find<MI_LOAD_REGISTER_REG *>(startIt, cmdList.end()) : find<MI_STORE_REGISTER_MEM *>(startIt, cmdList.end());
    if (useMask) {
        {
            ASSERT_NE(cmdList.end(), itor);
            auto cmdLoadReg = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
            EXPECT_EQ(firstLoadRegisterRegSrcAddress, cmdLoadReg->getSourceRegisterAddress());
            EXPECT_EQ(RegisterOffsets::csGprR13, cmdLoadReg->getDestinationRegisterAddress());
        }

        itor++;
        {
            ASSERT_NE(cmdList.end(), itor);
            auto cmdLoadImm = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
            EXPECT_EQ(RegisterOffsets::csGprR14, cmdLoadImm->getRegisterOffset());
            EXPECT_EQ(mask, cmdLoadImm->getDataDword());
        }

        itor++;
        {
            ASSERT_NE(cmdList.end(), itor);
            auto cmdMath = genCmdCast<MI_MATH *>(*itor);
            EXPECT_EQ(3u, cmdMath->DW0.BitField.DwordLength);
        }

        itor++;
        {
            ASSERT_NE(cmdList.end(), itor);
            auto cmdMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
            EXPECT_EQ(RegisterOffsets::csGprR12, cmdMem->getRegisterAddress());
            EXPECT_EQ(firstStoreRegMemAddress, cmdMem->getMemoryAddress());
            if (workloadPartition) {
                EXPECT_TRUE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
            } else {
                EXPECT_FALSE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
            }
        }
    } else {
        ASSERT_NE(cmdList.end(), itor);
        auto cmdMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
        EXPECT_EQ(firstLoadRegisterRegSrcAddress, cmdMem->getRegisterAddress());
        EXPECT_EQ(firstStoreRegMemAddress, cmdMem->getMemoryAddress());
        if (workloadPartition) {
            EXPECT_TRUE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
        } else {
            EXPECT_FALSE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
        }
    }
    itor++;
    ASSERT_NE(cmdList.end(), itor);
    auto cmdMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(secondLoadRegisterRegSrcAddress, cmdMem->getRegisterAddress());
    EXPECT_EQ(secondStoreRegMemAddress, cmdMem->getMemoryAddress());
    if (workloadPartition) {
        EXPECT_TRUE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
    } else {
        EXPECT_FALSE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
    }

    itor = useMask ? find<MI_LOAD_REGISTER_REG *>(itor, cmdList.end()) : find<MI_STORE_REGISTER_MEM *>(startIt, cmdList.end());
    if (useMask) {
        {
            ASSERT_NE(cmdList.end(), itor);
            auto cmdLoadReg = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
            EXPECT_EQ(thirdLoadRegisterRegSrcAddress, cmdLoadReg->getSourceRegisterAddress());
            EXPECT_EQ(RegisterOffsets::csGprR13, cmdLoadReg->getDestinationRegisterAddress());
        }

        itor++;
        {
            ASSERT_NE(cmdList.end(), itor);
            auto cmdLoadImm = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
            EXPECT_EQ(RegisterOffsets::csGprR14, cmdLoadImm->getRegisterOffset());
            EXPECT_EQ(mask, cmdLoadImm->getDataDword());
        }

        itor++;
        {
            ASSERT_NE(cmdList.end(), itor);
            auto cmdMath = genCmdCast<MI_MATH *>(*itor);
            EXPECT_EQ(3u, cmdMath->DW0.BitField.DwordLength);
        }

        itor++;
        {
            ASSERT_NE(cmdList.end(), itor);
            cmdMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
            EXPECT_EQ(RegisterOffsets::csGprR12, cmdMem->getRegisterAddress());
            EXPECT_EQ(thirdStoreRegMemAddress, cmdMem->getMemoryAddress());
            if (workloadPartition) {
                EXPECT_TRUE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
            } else {
                EXPECT_FALSE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
            }
        }
    } else {
        ASSERT_NE(cmdList.end(), itor);
        cmdMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
        EXPECT_EQ(thirdLoadRegisterRegSrcAddress, cmdMem->getRegisterAddress());
        EXPECT_EQ(thirdStoreRegMemAddress, cmdMem->getMemoryAddress());
        if (workloadPartition) {
            EXPECT_TRUE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
        } else {
            EXPECT_FALSE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
        }
    }
    itor++;
    ASSERT_NE(cmdList.end(), itor);
    cmdMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(fourthLoadRegisterRegSrcAddress, cmdMem->getRegisterAddress());
    EXPECT_EQ(fourthStoreRegMemAddress, cmdMem->getMemoryAddress());
    if (workloadPartition) {
        EXPECT_TRUE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
    } else {
        EXPECT_FALSE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
    }
    itor++;
    startIt = itor;
}

template <typename FamilyType>
void CmdListPipelineSelectStateFixture::testBody() {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    void *currentBuffer = nullptr;

    const auto &cmdlistRequiredState = commandList->getRequiredStreamState();
    const auto &cmdListFinalState = commandList->getFinalStreamState();
    auto &csrState = commandQueue->csr->getStreamProperties();

    auto commandListHandle = commandList->toHandle();

    auto &commandListStream = *commandList->commandContainer.getCommandStream();
    auto &cmdQueueStream = commandQueue->commandStream;

    GenCmdList cmdList;
    std::vector<GenCmdList::iterator> pipelineSelectList;
    size_t sizeBefore = 0;
    size_t sizeAfter = 0;
    auto result = ZE_RESULT_SUCCESS;

    bool systolicModeSupported = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->csr)->pipelineSupportFlags.systolicMode;

    PIPELINE_SELECT *pipelineSelectCmd = nullptr;

    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesSystolicPipelineSelectMode = 0;

        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        if (systolicModeSupported) {
            EXPECT_EQ(0, cmdlistRequiredState.pipelineSelect.systolicMode.value);
            EXPECT_EQ(0, cmdListFinalState.pipelineSelect.systolicMode.value);
        } else {
            EXPECT_EQ(-1, cmdlistRequiredState.pipelineSelect.systolicMode.value);
            EXPECT_EQ(-1, cmdListFinalState.pipelineSelect.systolicMode.value);
        }

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, pipelineSelectList.size());

        cmdList.clear();
        pipelineSelectList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        if (systolicModeSupported) {
            EXPECT_EQ(0, csrState.pipelineSelect.systolicMode.value);
        } else {
            EXPECT_EQ(-1, csrState.pipelineSelect.systolicMode.value);
        }

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, pipelineSelectList.size());

        pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(*pipelineSelectList[0]);
        EXPECT_FALSE(NEO::UnitTestHelper<FamilyType>::getSystolicFlagValueFromPipelineSelectCommand(*pipelineSelectCmd));

        cmdList.clear();
        pipelineSelectList.clear();
        commandList->reset();
    }

    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesSystolicPipelineSelectMode = 1;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        if (systolicModeSupported) {
            EXPECT_EQ(1, cmdlistRequiredState.pipelineSelect.systolicMode.value);
            EXPECT_EQ(1, cmdListFinalState.pipelineSelect.systolicMode.value);
        } else {
            EXPECT_EQ(-1, cmdlistRequiredState.pipelineSelect.systolicMode.value);
            EXPECT_EQ(-1, cmdListFinalState.pipelineSelect.systolicMode.value);
        }

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, pipelineSelectList.size());

        cmdList.clear();
        pipelineSelectList.clear();

        mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesSystolicPipelineSelectMode = 0;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        if (systolicModeSupported) {
            EXPECT_EQ(1, cmdlistRequiredState.pipelineSelect.systolicMode.value);
            EXPECT_EQ(0, cmdListFinalState.pipelineSelect.systolicMode.value);
        } else {
            EXPECT_EQ(-1, cmdlistRequiredState.pipelineSelect.systolicMode.value);
            EXPECT_EQ(-1, cmdListFinalState.pipelineSelect.systolicMode.value);
        }

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());

        if (systolicModeSupported) {
            ASSERT_EQ(1u, pipelineSelectList.size());

            pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(*pipelineSelectList[0]);
            EXPECT_FALSE(NEO::UnitTestHelper<FamilyType>::getSystolicFlagValueFromPipelineSelectCommand(*pipelineSelectCmd));
        } else {
            EXPECT_EQ(0u, pipelineSelectList.size());
        }

        cmdList.clear();
        pipelineSelectList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        if (systolicModeSupported) {
            EXPECT_EQ(0, csrState.pipelineSelect.systolicMode.value);
        } else {
            EXPECT_EQ(-1, csrState.pipelineSelect.systolicMode.value);
        }

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));

        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());

        if (systolicModeSupported) {
            ASSERT_EQ(1u, pipelineSelectList.size());

            pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(*pipelineSelectList[0]);
            EXPECT_TRUE(NEO::UnitTestHelper<FamilyType>::getSystolicFlagValueFromPipelineSelectCommand(*pipelineSelectCmd));
        } else {
            EXPECT_EQ(0u, pipelineSelectList.size());
        }

        cmdList.clear();
        pipelineSelectList.clear();
        commandList->reset();
    }

    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesSystolicPipelineSelectMode = 1;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        if (systolicModeSupported) {
            EXPECT_EQ(1, cmdlistRequiredState.pipelineSelect.systolicMode.value);
            EXPECT_EQ(1, cmdListFinalState.pipelineSelect.systolicMode.value);
        } else {
            EXPECT_EQ(-1, cmdlistRequiredState.pipelineSelect.systolicMode.value);
            EXPECT_EQ(-1, cmdListFinalState.pipelineSelect.systolicMode.value);
        }

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, pipelineSelectList.size());

        cmdList.clear();
        pipelineSelectList.clear();

        mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesSystolicPipelineSelectMode = 0;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        if (systolicModeSupported) {
            EXPECT_EQ(1, cmdlistRequiredState.pipelineSelect.systolicMode.value);
            EXPECT_EQ(0, cmdListFinalState.pipelineSelect.systolicMode.value);
        } else {
            EXPECT_EQ(-1, cmdlistRequiredState.pipelineSelect.systolicMode.value);
            EXPECT_EQ(-1, cmdListFinalState.pipelineSelect.systolicMode.value);
        }

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());

        if (systolicModeSupported) {
            ASSERT_EQ(1u, pipelineSelectList.size());

            pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(*pipelineSelectList[0]);
            EXPECT_FALSE(NEO::UnitTestHelper<FamilyType>::getSystolicFlagValueFromPipelineSelectCommand(*pipelineSelectCmd));
        } else {
            EXPECT_EQ(0u, pipelineSelectList.size());
        }

        cmdList.clear();
        pipelineSelectList.clear();

        mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesSystolicPipelineSelectMode = 1;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        if (systolicModeSupported) {
            EXPECT_EQ(1, cmdlistRequiredState.pipelineSelect.systolicMode.value);
            EXPECT_EQ(1, cmdListFinalState.pipelineSelect.systolicMode.value);
        } else {
            EXPECT_EQ(-1, cmdlistRequiredState.pipelineSelect.systolicMode.value);
            EXPECT_EQ(-1, cmdListFinalState.pipelineSelect.systolicMode.value);
        }

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());

        if (systolicModeSupported) {
            ASSERT_EQ(1u, pipelineSelectList.size());

            pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(*pipelineSelectList[0]);
            EXPECT_TRUE(NEO::UnitTestHelper<FamilyType>::getSystolicFlagValueFromPipelineSelectCommand(*pipelineSelectCmd));
        } else {
            EXPECT_EQ(0u, pipelineSelectList.size());
        }

        cmdList.clear();
        pipelineSelectList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        if (systolicModeSupported) {
            EXPECT_EQ(1, csrState.pipelineSelect.systolicMode.value);
        } else {
            EXPECT_EQ(-1, csrState.pipelineSelect.systolicMode.value);
        }

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());

        if (systolicModeSupported) {
            ASSERT_EQ(1u, pipelineSelectList.size());

            pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(*pipelineSelectList[0]);
            EXPECT_TRUE(NEO::UnitTestHelper<FamilyType>::getSystolicFlagValueFromPipelineSelectCommand(*pipelineSelectCmd));
        } else {
            EXPECT_EQ(0u, pipelineSelectList.size());
        }

        cmdList.clear();
        pipelineSelectList.clear();
        commandList->reset();
    }
    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesSystolicPipelineSelectMode = 1;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        if (systolicModeSupported) {
            EXPECT_EQ(1, cmdlistRequiredState.pipelineSelect.systolicMode.value);
            EXPECT_EQ(1, cmdListFinalState.pipelineSelect.systolicMode.value);
        } else {
            EXPECT_EQ(-1, cmdlistRequiredState.pipelineSelect.systolicMode.value);
            EXPECT_EQ(-1, cmdListFinalState.pipelineSelect.systolicMode.value);
        }

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, pipelineSelectList.size());

        cmdList.clear();
        pipelineSelectList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        if (systolicModeSupported) {
            EXPECT_EQ(1, csrState.pipelineSelect.systolicMode.value);
        } else {
            EXPECT_EQ(-1, csrState.pipelineSelect.systolicMode.value);
        }

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, pipelineSelectList.size());
    }
}

template <typename FamilyType>
void CmdListPipelineSelectStateFixture::testBodyShareStateRegularImmediate() {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    void *currentBuffer = nullptr;

    const auto &regularCmdlistRequiredState = commandList->getRequiredStreamState();
    const auto &regularCmdListFinalState = commandList->getFinalStreamState();
    auto &csrState = commandQueue->csr->getStreamProperties();

    auto commandListHandle = commandList->toHandle();

    auto &regularCommandListStream = *commandList->commandContainer.getCommandStream();
    auto &cmdQueueStream = commandQueue->commandStream;

    GenCmdList cmdList;
    std::vector<GenCmdList::iterator> pipelineSelectList;
    size_t sizeBefore = 0;
    size_t sizeAfter = 0;
    auto result = ZE_RESULT_SUCCESS;

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesSystolicPipelineSelectMode = 1;

    sizeBefore = regularCommandListStream.getUsed();
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    sizeAfter = regularCommandListStream.getUsed();

    bool systolicModeSupported = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->csr)->pipelineSupportFlags.systolicMode;

    if (systolicModeSupported) {
        EXPECT_EQ(1, regularCmdlistRequiredState.pipelineSelect.systolicMode.value);
        EXPECT_EQ(1, regularCmdListFinalState.pipelineSelect.systolicMode.value);
    } else {
        EXPECT_EQ(-1, regularCmdlistRequiredState.pipelineSelect.systolicMode.value);
        EXPECT_EQ(-1, regularCmdListFinalState.pipelineSelect.systolicMode.value);
    }

    currentBuffer = ptrOffset(regularCommandListStream.getCpuBase(), sizeBefore);
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      currentBuffer,
                                                      (sizeAfter - sizeBefore)));
    pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, pipelineSelectList.size());

    cmdList.clear();
    pipelineSelectList.clear();
    commandList->close();

    sizeBefore = cmdQueueStream.getUsed();
    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    sizeAfter = cmdQueueStream.getUsed();

    if (systolicModeSupported) {
        EXPECT_EQ(1, csrState.pipelineSelect.systolicMode.value);
    } else {
        EXPECT_EQ(-1, csrState.pipelineSelect.systolicMode.value);
    }

    currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      currentBuffer,
                                                      (sizeAfter - sizeBefore)));

    pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());

    ASSERT_EQ(1u, pipelineSelectList.size());

    auto pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(*pipelineSelectList[0]);

    EXPECT_EQ(systolicModeSupported, NEO::UnitTestHelper<FamilyType>::getSystolicFlagValueFromPipelineSelectCommand(*pipelineSelectCmd));

    cmdList.clear();
    pipelineSelectList.clear();

    auto &immediateCmdListStream = *commandListImmediate->commandContainer.getCommandStream();
    EXPECT_EQ(commandQueue->csr, commandListImmediate->getCsr(false));

    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = ultCsr.commandStream;

    size_t csrUsedBefore = csrStream.getUsed();
    sizeBefore = immediateCmdListStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    sizeAfter = immediateCmdListStream.getUsed();
    size_t csrUsedAfter = csrStream.getUsed();

    auto &immediateCmdListRequiredState = commandListImmediate->getRequiredStreamState();

    if (systolicModeSupported) {
        EXPECT_EQ(1, immediateCmdListRequiredState.pipelineSelect.systolicMode.value);
    } else {
        EXPECT_EQ(-1, immediateCmdListRequiredState.pipelineSelect.systolicMode.value);
    }

    currentBuffer = ptrOffset(immediateCmdListStream.getCpuBase(), sizeBefore);
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      currentBuffer,
                                                      (sizeAfter - sizeBefore)));
    pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, pipelineSelectList.size());

    cmdList.clear();
    pipelineSelectList.clear();

    if (systolicModeSupported) {
        EXPECT_EQ(1, csrState.pipelineSelect.systolicMode.value);
    } else {
        EXPECT_EQ(-1, csrState.pipelineSelect.systolicMode.value);
    }

    currentBuffer = ptrOffset(csrStream.getCpuBase(), csrUsedBefore);
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      currentBuffer,
                                                      (csrUsedAfter - csrUsedBefore)));
    pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, pipelineSelectList.size());
}

template <typename FamilyType>
void CmdListPipelineSelectStateFixture::testBodyShareStateImmediateRegular() {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    void *currentBuffer = nullptr;

    auto &immediateCmdListRequiredState = commandListImmediate->getRequiredStreamState();

    auto &immediateCmdListStream = *commandListImmediate->commandContainer.getCommandStream();

    auto &csrState = commandQueue->csr->getStreamProperties();

    EXPECT_EQ(commandQueue->csr, commandListImmediate->getCsr(false));

    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = ultCsr.commandStream;

    GenCmdList cmdList;
    std::vector<GenCmdList::iterator> pipelineSelectList;
    size_t sizeBefore = 0;
    size_t sizeAfter = 0;
    auto result = ZE_RESULT_SUCCESS;

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesSystolicPipelineSelectMode = 1;

    size_t csrUsedBefore = csrStream.getUsed();
    sizeBefore = immediateCmdListStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    sizeAfter = immediateCmdListStream.getUsed();
    size_t csrUsedAfter = csrStream.getUsed();

    bool systolicModeSupported = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->csr)->pipelineSupportFlags.systolicMode;

    if (systolicModeSupported) {
        EXPECT_EQ(1, immediateCmdListRequiredState.pipelineSelect.systolicMode.value);
    } else {
        EXPECT_EQ(-1, immediateCmdListRequiredState.pipelineSelect.systolicMode.value);
    }

    currentBuffer = ptrOffset(immediateCmdListStream.getCpuBase(), sizeBefore);
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      currentBuffer,
                                                      (sizeAfter - sizeBefore)));
    pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, pipelineSelectList.size());

    cmdList.clear();
    pipelineSelectList.clear();

    if (systolicModeSupported) {
        EXPECT_EQ(1, csrState.pipelineSelect.systolicMode.value);
    } else {
        EXPECT_EQ(-1, csrState.pipelineSelect.systolicMode.value);
    }

    currentBuffer = ptrOffset(csrStream.getCpuBase(), csrUsedBefore);
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      currentBuffer,
                                                      (csrUsedAfter - csrUsedBefore)));
    pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, pipelineSelectList.size());

    auto pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(*pipelineSelectList[0]);
    EXPECT_EQ(systolicModeSupported, NEO::UnitTestHelper<FamilyType>::getSystolicFlagValueFromPipelineSelectCommand(*pipelineSelectCmd));

    cmdList.clear();
    pipelineSelectList.clear();

    const auto &regularCmdlistRequiredState = commandList->getRequiredStreamState();
    const auto &regularCmdListFinalState = commandList->getFinalStreamState();

    auto commandListHandle = commandList->toHandle();

    auto &regularCommandListStream = *commandList->commandContainer.getCommandStream();
    auto &cmdQueueStream = commandQueue->commandStream;

    sizeBefore = regularCommandListStream.getUsed();
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    sizeAfter = regularCommandListStream.getUsed();

    if (systolicModeSupported) {
        EXPECT_EQ(1, regularCmdlistRequiredState.pipelineSelect.systolicMode.value);
        EXPECT_EQ(1, regularCmdListFinalState.pipelineSelect.systolicMode.value);
    } else {
        EXPECT_EQ(-1, regularCmdlistRequiredState.pipelineSelect.systolicMode.value);
        EXPECT_EQ(-1, regularCmdListFinalState.pipelineSelect.systolicMode.value);
    }

    currentBuffer = ptrOffset(regularCommandListStream.getCpuBase(), sizeBefore);
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      currentBuffer,
                                                      (sizeAfter - sizeBefore)));
    pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, pipelineSelectList.size());

    cmdList.clear();
    pipelineSelectList.clear();
    commandList->close();

    sizeBefore = cmdQueueStream.getUsed();
    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    sizeAfter = cmdQueueStream.getUsed();

    if (systolicModeSupported) {
        EXPECT_EQ(1, csrState.pipelineSelect.systolicMode.value);
    } else {
        EXPECT_EQ(-1, csrState.pipelineSelect.systolicMode.value);
    }

    currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      currentBuffer,
                                                      (sizeAfter - sizeBefore)));
    pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, pipelineSelectList.size());
}

template <typename FamilyType>
void CmdListPipelineSelectStateFixture::testBodySystolicAndScratchOnSecondCommandList() {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using FrontEndStateCommand = typename FamilyType::FrontEndStateCommand;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    auto result = ZE_RESULT_SUCCESS;
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesSystolicPipelineSelectMode = 1;
    mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x40;

    result = commandList2->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList2->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    // execute first clear command list to settle global init
    ze_command_list_handle_t commandLists[2] = {commandList->toHandle(), nullptr};
    result = commandQueue->executeCommandLists(1, commandLists, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto queueSize = cmdQueueStream.getUsed();
    // add command list that requires both scratch and systolic.
    // scratch makes globally front end dirty and so global init too,
    // but dispatch systolic programming only before second command list
    commandLists[1] = commandList2->toHandle();
    result = commandQueue->executeCommandLists(2, commandLists, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(cmdQueueStream.getCpuBase(), queueSize),
                                                      (cmdQueueStream.getUsed() - queueSize)));

    // first is scratch command dispatched globally
    auto iterFeCmd = find<FrontEndStateCommand *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), iterFeCmd);

    // find bb start jumping to first command list
    auto iterBbStartCmd = find<MI_BATCH_BUFFER_START *>(iterFeCmd, cmdList.end());
    ASSERT_NE(cmdList.end(), iterBbStartCmd);
    auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(*iterBbStartCmd);

    auto firstCmdListBufferAddress = commandList->getCmdContainer().getCmdBufferAllocations()[0]->getGpuAddress();
    EXPECT_EQ(firstCmdListBufferAddress, bbStart->getBatchBufferStartAddress());

    // 3rd is pipeline select before systolic kernel
    auto iterPsCmd = find<PIPELINE_SELECT *>(iterBbStartCmd, cmdList.end());

    bool systolicModeSupported = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->csr)->pipelineSupportFlags.systolicMode;

    if (systolicModeSupported) {
        ASSERT_NE(cmdList.end(), iterPsCmd);
        auto pipelineSelectCmd = reinterpret_cast<PIPELINE_SELECT *>(*iterPsCmd);

        EXPECT_TRUE(NEO::UnitTestHelper<FamilyType>::getSystolicFlagValueFromPipelineSelectCommand(*pipelineSelectCmd));
        iterBbStartCmd = find<MI_BATCH_BUFFER_START *>(iterPsCmd, cmdList.end());

    } else {
        EXPECT_EQ(cmdList.end(), iterPsCmd);
        iterBbStartCmd = find<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
        iterBbStartCmd = find<MI_BATCH_BUFFER_START *>(++iterBbStartCmd, cmdList.end());
    }

    // find bb start jumping to second command list
    ASSERT_NE(cmdList.end(), iterBbStartCmd);
    bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(*iterBbStartCmd);

    auto secondCmdListBufferAddress = commandList2->getCmdContainer().getCmdBufferAllocations()[0]->getGpuAddress();
    EXPECT_EQ(secondCmdListBufferAddress, bbStart->getBatchBufferStartAddress());
}

template <typename FamilyType>
void CmdListThreadArbitrationFixture::testBody() {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using EU_THREAD_SCHEDULING_MODE = typename STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE;

    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    void *currentBuffer = nullptr;

    const auto &cmdlistRequiredState = commandList->getRequiredStreamState();
    const auto &cmdListFinalState = commandList->getFinalStreamState();
    auto &csrState = commandQueue->csr->getStreamProperties();

    auto commandListHandle = commandList->toHandle();

    auto &commandListStream = *commandList->commandContainer.getCommandStream();
    auto &cmdQueueStream = commandQueue->commandStream;
    auto queueCsr = commandQueue->getCsr();

    GenCmdList cmdList;
    std::vector<GenCmdList::iterator> stateComputeModeList;
    size_t sizeBefore = 0;
    size_t sizeAfter = 0;
    auto result = ZE_RESULT_SUCCESS;

    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.threadArbitrationPolicy = NEO::ThreadArbitrationPolicy::AgeBased;

        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::AgeBased, cmdlistRequiredState.stateComputeMode.threadArbitrationPolicy.value);
        EXPECT_EQ(NEO::ThreadArbitrationPolicy::AgeBased, cmdListFinalState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, stateComputeModeList.size());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->close();

        EXPECT_TRUE(queueCsr->getStateComputeModeDirty());

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_FALSE(queueCsr->getStateComputeModeDirty());

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::AgeBased, csrState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateComputeModeList.size());

        auto stateComputeModeCmd = genCmdCast<STATE_COMPUTE_MODE *>(*stateComputeModeList[0]);
        EXPECT_EQ(EU_THREAD_SCHEDULING_MODE::EU_THREAD_SCHEDULING_MODE_OLDEST_FIRST, stateComputeModeCmd->getEuThreadSchedulingMode());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->reset();
    }

    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.threadArbitrationPolicy = NEO::ThreadArbitrationPolicy::RoundRobin;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobin, cmdlistRequiredState.stateComputeMode.threadArbitrationPolicy.value);
        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobin, cmdListFinalState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, stateComputeModeList.size());

        cmdList.clear();
        stateComputeModeList.clear();

        mockKernelImmData->kernelDescriptor->kernelAttributes.threadArbitrationPolicy = NEO::ThreadArbitrationPolicy::AgeBased;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobin, cmdlistRequiredState.stateComputeMode.threadArbitrationPolicy.value);
        EXPECT_EQ(NEO::ThreadArbitrationPolicy::AgeBased, cmdListFinalState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateComputeModeList.size());

        auto stateComputeModeCmd = genCmdCast<STATE_COMPUTE_MODE *>(*stateComputeModeList[0]);
        EXPECT_EQ(EU_THREAD_SCHEDULING_MODE::EU_THREAD_SCHEDULING_MODE_OLDEST_FIRST, stateComputeModeCmd->getEuThreadSchedulingMode());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::AgeBased, csrState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateComputeModeList.size());

        stateComputeModeCmd = genCmdCast<STATE_COMPUTE_MODE *>(*stateComputeModeList[0]);
        EXPECT_EQ(EU_THREAD_SCHEDULING_MODE::EU_THREAD_SCHEDULING_MODE_ROUND_ROBIN, stateComputeModeCmd->getEuThreadSchedulingMode());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->reset();
    }

    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.threadArbitrationPolicy = NEO::ThreadArbitrationPolicy::RoundRobin;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobin, cmdlistRequiredState.stateComputeMode.threadArbitrationPolicy.value);
        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobin, cmdListFinalState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, stateComputeModeList.size());

        cmdList.clear();
        stateComputeModeList.clear();

        mockKernelImmData->kernelDescriptor->kernelAttributes.threadArbitrationPolicy = NEO::ThreadArbitrationPolicy::AgeBased;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobin, cmdlistRequiredState.stateComputeMode.threadArbitrationPolicy.value);
        EXPECT_EQ(NEO::ThreadArbitrationPolicy::AgeBased, cmdListFinalState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateComputeModeList.size());

        auto stateComputeModeCmd = genCmdCast<STATE_COMPUTE_MODE *>(*stateComputeModeList[0]);
        EXPECT_EQ(EU_THREAD_SCHEDULING_MODE::EU_THREAD_SCHEDULING_MODE_OLDEST_FIRST, stateComputeModeCmd->getEuThreadSchedulingMode());

        cmdList.clear();
        stateComputeModeList.clear();

        mockKernelImmData->kernelDescriptor->kernelAttributes.threadArbitrationPolicy = NEO::ThreadArbitrationPolicy::RoundRobinAfterDependency;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobin, cmdlistRequiredState.stateComputeMode.threadArbitrationPolicy.value);
        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobinAfterDependency, cmdListFinalState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateComputeModeList.size());

        stateComputeModeCmd = genCmdCast<STATE_COMPUTE_MODE *>(*stateComputeModeList[0]);
        EXPECT_EQ(EU_THREAD_SCHEDULING_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN, stateComputeModeCmd->getEuThreadSchedulingMode());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobinAfterDependency, csrState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateComputeModeList.size());

        stateComputeModeCmd = genCmdCast<STATE_COMPUTE_MODE *>(*stateComputeModeList[0]);
        EXPECT_EQ(EU_THREAD_SCHEDULING_MODE::EU_THREAD_SCHEDULING_MODE_ROUND_ROBIN, stateComputeModeCmd->getEuThreadSchedulingMode());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->reset();
    }
    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.threadArbitrationPolicy = NEO::ThreadArbitrationPolicy::RoundRobinAfterDependency;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobinAfterDependency, cmdlistRequiredState.stateComputeMode.threadArbitrationPolicy.value);
        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobinAfterDependency, cmdListFinalState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, stateComputeModeList.size());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobinAfterDependency, csrState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, stateComputeModeList.size());
    }
}

template <typename FamilyType>
void CmdListLargeGrfFixture::testBody() {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    void *currentBuffer = nullptr;

    auto &cmdlistRequiredState = commandList->getRequiredStreamState();
    auto &cmdListFinalState = commandList->getFinalStreamState();
    auto &csrState = commandQueue->csr->getStreamProperties();
    auto queueCsr = commandQueue->getCsr();

    auto commandListHandle = commandList->toHandle();

    auto &commandListStream = *commandList->commandContainer.getCommandStream();
    auto &cmdQueueStream = commandQueue->commandStream;

    GenCmdList cmdList;
    std::vector<GenCmdList::iterator> stateComputeModeList;
    size_t sizeBefore = 0;
    size_t sizeAfter = 0;
    auto result = ZE_RESULT_SUCCESS;

    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::defaultGrfNumber;

        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(0, cmdlistRequiredState.stateComputeMode.largeGrfMode.value);
        EXPECT_EQ(0, cmdListFinalState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, stateComputeModeList.size());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->close();

        EXPECT_TRUE(queueCsr->getStateComputeModeDirty());

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_FALSE(queueCsr->getStateComputeModeDirty());

        EXPECT_EQ(0, csrState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateComputeModeList.size());

        auto stateComputeModeCmd = genCmdCast<STATE_COMPUTE_MODE *>(*stateComputeModeList[0]);
        EXPECT_FALSE(stateComputeModeCmd->getLargeGrfMode());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->reset();
    }

    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::largeGrfNumber;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(1, cmdlistRequiredState.stateComputeMode.largeGrfMode.value);
        EXPECT_EQ(1, cmdListFinalState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, stateComputeModeList.size());

        cmdList.clear();
        stateComputeModeList.clear();

        mockKernelImmData->kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::defaultGrfNumber;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(1, cmdlistRequiredState.stateComputeMode.largeGrfMode.value);
        EXPECT_EQ(0, cmdListFinalState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateComputeModeList.size());

        auto stateComputeModeCmd = genCmdCast<STATE_COMPUTE_MODE *>(*stateComputeModeList[0]);
        EXPECT_FALSE(stateComputeModeCmd->getLargeGrfMode());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_EQ(0, csrState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateComputeModeList.size());

        stateComputeModeCmd = genCmdCast<STATE_COMPUTE_MODE *>(*stateComputeModeList[0]);
        EXPECT_TRUE(stateComputeModeCmd->getLargeGrfMode());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->reset();
    }

    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::largeGrfNumber;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(1, cmdlistRequiredState.stateComputeMode.largeGrfMode.value);
        EXPECT_EQ(1, cmdListFinalState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, stateComputeModeList.size());

        cmdList.clear();
        stateComputeModeList.clear();

        mockKernelImmData->kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::defaultGrfNumber;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(1, cmdlistRequiredState.stateComputeMode.largeGrfMode.value);
        EXPECT_EQ(0, cmdListFinalState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateComputeModeList.size());

        auto stateComputeModeCmd = genCmdCast<STATE_COMPUTE_MODE *>(*stateComputeModeList[0]);
        EXPECT_FALSE(stateComputeModeCmd->getLargeGrfMode());

        cmdList.clear();
        stateComputeModeList.clear();

        mockKernelImmData->kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::largeGrfNumber;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(1, cmdlistRequiredState.stateComputeMode.largeGrfMode.value);
        EXPECT_EQ(1, cmdListFinalState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateComputeModeList.size());

        stateComputeModeCmd = genCmdCast<STATE_COMPUTE_MODE *>(*stateComputeModeList[0]);
        EXPECT_TRUE(stateComputeModeCmd->getLargeGrfMode());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_EQ(1, csrState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateComputeModeList.size());

        stateComputeModeCmd = genCmdCast<STATE_COMPUTE_MODE *>(*stateComputeModeList[0]);
        EXPECT_TRUE(stateComputeModeCmd->getLargeGrfMode());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->reset();
    }
    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::largeGrfNumber;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(1, cmdlistRequiredState.stateComputeMode.largeGrfMode.value);
        EXPECT_EQ(1, cmdListFinalState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, stateComputeModeList.size());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_EQ(1, csrState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, stateComputeModeList.size());
    }
}

template <typename FamilyType>
void TbxImmediateCommandListFixture::setUpT() {
    ModuleImmutableDataFixture::setUp();

    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->memoryOperationsInterface =
        std::make_unique<NEO::MockMemoryOperations>();

    neoDevice->getUltCommandStreamReceiver<FamilyType>().commandStreamReceiverType = CommandStreamReceiverType::tbx;
    ModuleMutableCommandListFixture::setUpImpl();

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    ze_result_t returnValue;
    eventPool = std::unique_ptr<EventPool>(static_cast<EventPool *>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue)));
    event = std::unique_ptr<Event>(static_cast<Event *>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, returnValue)));

    setEvent();
}

template <typename FamilyType>
void ImmediateCmdListSharedHeapsFlushTaskFixtureInit::testBody(NonKernelOperation operation) {
    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = ZE_RESULT_SUCCESS;

    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    validateDispatchFlags(false, ultCsr.recordedImmediateDispatchFlags, ultCsr.recordedSsh);

    result = commandListImmediateCoexisting->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    validateDispatchFlags(false, ultCsr.recordedImmediateDispatchFlags, ultCsr.recordedSsh);

    auto &cmdContainer = commandListImmediate->commandContainer;
    auto &cmdContainerCoexisting = commandListImmediateCoexisting->commandContainer;

    auto sshFirstCmdList = cmdContainer.getSurfaceStateHeapReserve().indirectHeapReservation;
    auto sshCoexistingCmdList = cmdContainerCoexisting.getSurfaceStateHeapReserve().indirectHeapReservation;

    void *firstSshCpuPointer = sshFirstCmdList->getCpuBase();

    EXPECT_EQ(sshFirstCmdList->getCpuBase(), sshCoexistingCmdList->getCpuBase());

    auto csrSshHeap = &ultCsr.getIndirectHeap(HeapType::surfaceState, 0);

    EXPECT_EQ(csrSshHeap->getCpuBase(), sshFirstCmdList->getCpuBase());

    csrSshHeap->getSpace(csrSshHeap->getAvailableSpace());

    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    validateDispatchFlags(false, ultCsr.recordedImmediateDispatchFlags, ultCsr.recordedSsh);

    EXPECT_NE(firstSshCpuPointer, sshFirstCmdList->getCpuBase());

    result = commandListImmediateCoexisting->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    validateDispatchFlags(false, ultCsr.recordedImmediateDispatchFlags, ultCsr.recordedSsh);

    EXPECT_EQ(sshFirstCmdList->getCpuBase(), sshCoexistingCmdList->getCpuBase());
    EXPECT_EQ(csrSshHeap->getCpuBase(), sshFirstCmdList->getCpuBase());

    appendNonKernelOperation(commandListImmediate.get(), operation);
    validateDispatchFlags(true, ultCsr.recordedImmediateDispatchFlags, ultCsr.recordedSsh);

    appendNonKernelOperation(commandListImmediateCoexisting.get(), operation);
    validateDispatchFlags(true, ultCsr.recordedImmediateDispatchFlags, ultCsr.recordedSsh);
}

template <typename FamilyType>
void CommandListScratchPatchFixtureInit::testScratchInline(bool useImmediate, bool patchPreamble) {
    auto csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;
    auto scratchController = csr->getScratchSpaceController();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->csr);
    ultCsr->storeMakeResidentAllocations = true;

    NEO::EncodeDispatchKernelArgs dispatchKernelArgs = {};
    dispatchKernelArgs.isHeaplessModeEnabled = true;

    size_t inlineOffset = NEO::EncodeDispatchKernel<FamilyType>::getInlineDataOffset(dispatchKernelArgs);

    auto scratchCmdList = static_cast<L0::CommandList *>(commandList.get());
    auto cmdListStream = commandList->commandContainer.getCommandStream();
    if (useImmediate) {
        scratchCmdList = static_cast<L0::CommandList *>(commandListImmediate.get());
        cmdListStream = commandListImmediate->commandContainer.getCommandStream();
    }

    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    auto cmdListGpuBase = cmdListStream->getGpuBase();
    auto cmdListCpuBase = cmdListStream->getCpuBase();

    auto result = ZE_RESULT_SUCCESS;
    size_t usedBefore = cmdListStream->getUsed();
    result = scratchCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    size_t usedAfter = cmdListStream->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint64_t surfaceHeapGpuBase = getSurfStateGpuBase(useImmediate);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdListStream->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto walkerIterator = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerIterator);
    void *walkerPtrWithScratch = *walkerIterator;
    auto walkerOffset = ptrDiff(walkerPtrWithScratch, cmdListCpuBase);

    mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x0;

    usedBefore = cmdListStream->getUsed();
    result = scratchCmdList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    usedAfter = cmdListStream->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdListStream->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    walkerIterator = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerIterator);
    void *walkerPtrWithoutScratch = *walkerIterator;

    if (!useImmediate) {
        result = commandList->close();
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(1u, commandList->getActiveScratchPatchElements());

        auto commandListHandle = commandList->toHandle();

        void *queueCpuBase = commandQueue->commandStream.getCpuBase();
        auto usedSpaceBefore = commandQueue->commandStream.getUsed();
        commandQueue->setPatchingPreamble(patchPreamble, false);
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto usedSpaceAfter = commandQueue->commandStream.getUsed();
        ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

        if (patchPreamble) {
            cmdList.clear();
            ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
                cmdList,
                ptrOffset(queueCpuBase, usedSpaceBefore),
                usedSpaceAfter - usedSpaceBefore));
        }
    }

    auto scratchAddress = scratchController->getScratchPatchAddress();
    auto fullScratchAddress = surfaceHeapGpuBase + scratchAddress;

    uint64_t scratchInlineValue = 0;

    if (patchPreamble) {
        using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

        auto sdiCmds = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        ASSERT_LT(2u, sdiCmds.size()); // last two SDI encodes returning BB_START

        uint64_t walkerScratchInlineGpuVa = cmdListGpuBase + walkerOffset + (inlineOffset + scratchInlineOffset);

        uint32_t scratchLowerDword = static_cast<uint32_t>(fullScratchAddress & std::numeric_limits<uint32_t>::max());
        uint32_t scratchUpperDword = static_cast<uint32_t>(fullScratchAddress >> 32);

        size_t sdiMax = sdiCmds.size() - 2;
        for (size_t i = 0; i < sdiMax; i++) {
            auto sdiCmd = reinterpret_cast<MI_STORE_DATA_IMM *>(*sdiCmds[i]);
            EXPECT_EQ(walkerScratchInlineGpuVa, sdiCmd->getAddress());
            if (i == 0) {
                EXPECT_EQ(scratchLowerDword, sdiCmd->getDataDword0());
            } else {
                EXPECT_EQ(scratchUpperDword, sdiCmd->getDataDword0());
            }
            if (sdiCmd->getStoreQword() == false) {
                walkerScratchInlineGpuVa += sizeof(uint32_t);
            } else {
                EXPECT_EQ(scratchUpperDword, sdiCmd->getDataDword1());
            }
        }
    } else {
        void *scratchInlinePtr = ptrOffset(walkerPtrWithScratch, (inlineOffset + scratchInlineOffset));
        std::memcpy(&scratchInlineValue, scratchInlinePtr, sizeof(scratchInlineValue));
        EXPECT_EQ(fullScratchAddress, scratchInlineValue);

        scratchInlinePtr = ptrOffset(walkerPtrWithoutScratch, (inlineOffset + scratchInlineOffset));
        std::memcpy(&scratchInlineValue, scratchInlinePtr, sizeof(scratchInlineValue));
        EXPECT_EQ(0u, scratchInlineValue);
    }

    auto scratch0Allocation = scratchController->getScratchSpaceSlot0Allocation();
    bool scratchInResidency = ultCsr->isMadeResident(scratch0Allocation);
    EXPECT_TRUE(scratchInResidency);

    commandList->reset();
    EXPECT_EQ(0u, commandList->getActiveScratchPatchElements());
}

template <typename FamilyType>
void CommandListScratchPatchFixtureInit::testScratchGrowingPatching() {
    auto csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;
    auto scratchController = csr->getScratchSpaceController();

    NEO::EncodeDispatchKernelArgs dispatchKernelArgs = {};
    dispatchKernelArgs.isHeaplessModeEnabled = true;

    size_t inlineOffset = NEO::EncodeDispatchKernel<FamilyType>::getInlineDataOffset(dispatchKernelArgs);

    auto cmdListStream = commandList->commandContainer.getCommandStream();

    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    auto result = ZE_RESULT_SUCCESS;
    size_t usedBefore = cmdListStream->getUsed();
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    size_t usedAfter = cmdListStream->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint64_t surfaceHeapGpuBase = getSurfStateGpuBase(false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdListStream->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto walkerIterator = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerIterator);
    void *walkerPtrWithScratch = *walkerIterator;

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto commandListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto scratchAddress = scratchController->getScratchPatchAddress();
    auto fullScratchAddress = surfaceHeapGpuBase + scratchAddress;

    auto patchIndex = launchParams.scratchAddressPatchIndex;
    ASSERT_TRUE(commandList->commandsToPatch.size() > patchIndex);

    auto currentScratchPatchAddress = commandList->commandsToPatch[patchIndex].scratchAddressAfterPatch;
    auto expectedScratchPatchAddress = getExpectedScratchPatchAddress(scratchAddress);

    EXPECT_EQ(expectedScratchPatchAddress, currentScratchPatchAddress);
    EXPECT_EQ(scratchController, commandList->getCommandListUsedScratchController());

    uint64_t scratchInlineValue = 0;

    void *scratchInlinePtr = ptrOffset(walkerPtrWithScratch, (inlineOffset + scratchInlineOffset));
    std::memcpy(&scratchInlineValue, scratchInlinePtr, sizeof(scratchInlineValue));
    EXPECT_EQ(fullScratchAddress, scratchInlineValue);

    commandList->reset();

    EXPECT_EQ(nullptr, commandList->getCommandListUsedScratchController());

    mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[1] = 0x40;

    usedBefore = cmdListStream->getUsed();
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    usedAfter = cmdListStream->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdListStream->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    walkerIterator = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerIterator);
    void *walkerPtrWithSlot1Scratch = *walkerIterator;

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    scratchAddress = scratchController->getScratchPatchAddress();
    auto fullScratchSlot1Address = surfaceHeapGpuBase + scratchAddress;

    patchIndex = launchParams.scratchAddressPatchIndex;
    ASSERT_TRUE(commandList->commandsToPatch.size() > patchIndex);

    currentScratchPatchAddress = commandList->commandsToPatch[patchIndex].scratchAddressAfterPatch;
    expectedScratchPatchAddress = getExpectedScratchPatchAddress(scratchAddress);

    EXPECT_EQ(expectedScratchPatchAddress, currentScratchPatchAddress);
    EXPECT_EQ(scratchController, commandList->getCommandListUsedScratchController());

    scratchInlinePtr = ptrOffset(walkerPtrWithSlot1Scratch, (inlineOffset + scratchInlineOffset));
    std::memcpy(&scratchInlineValue, scratchInlinePtr, sizeof(scratchInlineValue));
    EXPECT_EQ(fullScratchSlot1Address, scratchInlineValue);

    memset(scratchInlinePtr, 0, scratchInlinePointerSize);

    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    std::memcpy(&scratchInlineValue, scratchInlinePtr, sizeof(scratchInlineValue));
    EXPECT_EQ(0u, scratchInlineValue);
}

template <typename FamilyType>
void CommandListScratchPatchFixtureInit::testScratchSameNotPatching() {
    auto csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;
    auto scratchController = csr->getScratchSpaceController();

    NEO::EncodeDispatchKernelArgs dispatchKernelArgs = {};
    dispatchKernelArgs.isHeaplessModeEnabled = true;

    size_t inlineOffset = NEO::EncodeDispatchKernel<FamilyType>::getInlineDataOffset(dispatchKernelArgs);

    auto cmdListStream = commandList->commandContainer.getCommandStream();

    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    EXPECT_TRUE(NEO::isUndefined(launchParams.scratchAddressPatchIndex));

    auto result = ZE_RESULT_SUCCESS;
    size_t usedBefore = cmdListStream->getUsed();
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    size_t usedAfter = cmdListStream->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint64_t surfaceHeapGpuBase = getSurfStateGpuBase(false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdListStream->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto walkerIterator = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerIterator);
    void *walkerPtrWithScratch = *walkerIterator;

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    const CommandToPatch *scratchCmd = nullptr;
    size_t scratchCmdIndex = 0;
    for (const auto &cmdToPatch : commandList->getCommandsToPatch()) {
        if (cmdToPatch.type == CommandToPatch::CommandType::ComputeWalkerInlineDataScratch) {
            scratchCmd = &cmdToPatch;
            break;
        }
        scratchCmdIndex += 1;
    }
    ASSERT_NE(scratchCmd, nullptr);
    EXPECT_EQ(scratchCmd->scratchAddressAfterPatch, 0u);
    EXPECT_EQ(scratchCmdIndex, launchParams.scratchAddressPatchIndex);

    auto commandListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto scratchAddress = scratchController->getScratchPatchAddress();
    auto fullScratchAddress = surfaceHeapGpuBase + scratchAddress;
    auto expectedSavedScratchAddress = fixtureGlobalStatelessMode ? fullScratchAddress : scratchAddress;

    uint64_t scratchInlineValue = 0;

    void *scratchInlinePtr = ptrOffset(walkerPtrWithScratch, (inlineOffset + scratchInlineOffset));
    std::memcpy(&scratchInlineValue, scratchInlinePtr, sizeof(scratchInlineValue));
    EXPECT_EQ(fullScratchAddress, scratchInlineValue);
    EXPECT_EQ(expectedSavedScratchAddress, scratchCmd->scratchAddressAfterPatch);

    memset(scratchInlinePtr, 0, scratchInlinePointerSize);

    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    std::memcpy(&scratchInlineValue, scratchInlinePtr, sizeof(scratchInlineValue));
    EXPECT_EQ(0u, scratchInlineValue);
    EXPECT_EQ(expectedSavedScratchAddress, scratchCmd->scratchAddressAfterPatch);
}

template <typename FamilyType>
void CommandListScratchPatchFixtureInit::testScratchImmediatePatching() {
    auto csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;
    auto scratchController = csr->getScratchSpaceController();

    NEO::EncodeDispatchKernelArgs dispatchKernelArgs = {};
    dispatchKernelArgs.isHeaplessModeEnabled = true;

    size_t inlineOffset = NEO::EncodeDispatchKernel<FamilyType>::getInlineDataOffset(dispatchKernelArgs);

    auto cmdListStream = commandListImmediate->commandContainer.getCommandStream();
    commandListImmediate->commandContainer.setImmediateCmdListCsr(csr);

    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    auto result = ZE_RESULT_SUCCESS;
    size_t usedBefore = cmdListStream->getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    size_t usedAfter = cmdListStream->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint64_t surfaceHeapGpuBase = getSurfStateGpuBase(true);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdListStream->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto walkerIterator = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerIterator);
    void *walkerPtrWithScratch = *walkerIterator;

    auto scratchAddress = scratchController->getScratchPatchAddress();
    auto fullScratchAddress = surfaceHeapGpuBase + scratchAddress;

    uint64_t scratchInlineValue = 0;

    void *scratchInlinePtr = ptrOffset(walkerPtrWithScratch, (inlineOffset + scratchInlineOffset));
    std::memcpy(&scratchInlineValue, scratchInlinePtr, sizeof(scratchInlineValue));
    EXPECT_EQ(fullScratchAddress, scratchInlineValue);

    memset(scratchInlinePtr, 0, scratchInlinePointerSize);

    std::memcpy(&scratchInlineValue, scratchInlinePtr, sizeof(scratchInlineValue));
    EXPECT_EQ(0u, scratchInlineValue);
}

template <typename FamilyType>
void CommandListScratchPatchFixtureInit::testScratchChangedControllerPatching() {
    auto csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(csr);

    auto scratchControllerInitial = csr->getScratchSpaceController();

    NEO::EncodeDispatchKernelArgs dispatchKernelArgs = {};
    dispatchKernelArgs.isHeaplessModeEnabled = true;

    size_t inlineOffset = NEO::EncodeDispatchKernel<FamilyType>::getInlineDataOffset(dispatchKernelArgs);

    auto cmdListStream = commandList->commandContainer.getCommandStream();

    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    auto result = ZE_RESULT_SUCCESS;
    size_t usedBefore = cmdListStream->getUsed();
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    size_t usedAfter = cmdListStream->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uint64_t surfaceHeapGpuBase = getSurfStateGpuBase(false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdListStream->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto walkerIterator = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerIterator);
    void *walkerPtrWithScratch = *walkerIterator;

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto commandListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto scratchAddress = scratchControllerInitial->getScratchPatchAddress();
    auto fullScratchAddress = surfaceHeapGpuBase + scratchAddress;

    auto patchIndex = launchParams.scratchAddressPatchIndex;
    ASSERT_TRUE(commandList->commandsToPatch.size() > patchIndex);

    auto currentScratchPatchAddress = commandList->commandsToPatch[patchIndex].scratchAddressAfterPatch;
    auto expectedScratchPatchAddress = getExpectedScratchPatchAddress(scratchAddress);

    EXPECT_EQ(expectedScratchPatchAddress, currentScratchPatchAddress);
    EXPECT_EQ(scratchControllerInitial, commandList->getCommandListUsedScratchController());

    uint64_t scratchInlineValue = 0;

    void *scratchInlinePtr = ptrOffset(walkerPtrWithScratch, (inlineOffset + scratchInlineOffset));
    std::memcpy(&scratchInlineValue, scratchInlinePtr, sizeof(scratchInlineValue));
    EXPECT_EQ(fullScratchAddress, scratchInlineValue);

    memset(scratchInlinePtr, 0, scratchInlinePointerSize);

    // simulate execution on different scratch controller (execution of command list from normal to low priority queue)
    ultCsr->createScratchSpaceController();
    auto scratchControllerSecond = csr->getScratchSpaceController();

    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    scratchAddress = scratchControllerSecond->getScratchPatchAddress();
    fullScratchAddress = surfaceHeapGpuBase + scratchAddress;

    expectedScratchPatchAddress = getExpectedScratchPatchAddress(scratchAddress);
    currentScratchPatchAddress = commandList->commandsToPatch[patchIndex].scratchAddressAfterPatch;

    EXPECT_EQ(expectedScratchPatchAddress, currentScratchPatchAddress);
    EXPECT_EQ(scratchControllerSecond, commandList->getCommandListUsedScratchController());

    scratchInlinePtr = ptrOffset(walkerPtrWithScratch, (inlineOffset + scratchInlineOffset));
    std::memcpy(&scratchInlineValue, scratchInlinePtr, sizeof(scratchInlineValue));
    EXPECT_EQ(fullScratchAddress, scratchInlineValue);

    memset(scratchInlinePtr, 0, scratchInlinePointerSize);

    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    std::memcpy(&scratchInlineValue, scratchInlinePtr, sizeof(scratchInlineValue));
    EXPECT_EQ(0u, scratchInlineValue);
}

template <typename FamilyType>
void CommandListScratchPatchFixtureInit::testScratchCommandViewNoPatching() {
    uint8_t computeWalkerHostBuffer[512];
    uint8_t payloadHostBuffer[256];

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    launchParams.makeKernelCommandView = true;
    launchParams.cmdWalkerBuffer = computeWalkerHostBuffer;
    launchParams.hostPayloadBuffer = payloadHostBuffer;

    auto result = ZE_RESULT_SUCCESS;
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0u, commandList->getCommandListPerThreadScratchSize(0));
    EXPECT_EQ(0u, commandList->getCommandListPerThreadScratchSize(1));

    auto &cmdsToPatch = commandList->getCommandsToPatch();
    bool foundScratchPatchCmd = false;

    for (auto &cmdToPatch : cmdsToPatch) {
        if (cmdToPatch.type == CommandToPatch::CommandType::ComputeWalkerInlineDataScratch) {
            foundScratchPatchCmd = true;
            break;
        }
    }
    EXPECT_FALSE(foundScratchPatchCmd);
}

template <typename FamilyType>
void CommandListScratchPatchFixtureInit::testExternalScratchPatching() {
    auto csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;
    auto scratchController = csr->getScratchSpaceController();

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->csr);
    ultCsr->storeMakeResidentAllocations = true;

    NEO::EncodeDispatchKernelArgs dispatchKernelArgs = {};
    dispatchKernelArgs.isHeaplessModeEnabled = true;

    size_t inlineOffset = NEO::EncodeDispatchKernel<FamilyType>::getInlineDataOffset(dispatchKernelArgs);

    auto cmdListStream = commandList->commandContainer.getCommandStream();

    mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x0;
    mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[1] = 0x0;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    launchParams.externalPerThreadScratchSize[0] = 0x80;
    launchParams.externalPerThreadScratchSize[1] = 0x40;

    auto result = ZE_RESULT_SUCCESS;
    size_t usedBefore = cmdListStream->getUsed();
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    size_t usedAfter = cmdListStream->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(0x80u, commandList->getCommandListPerThreadScratchSize(0));
    EXPECT_EQ(0x40u, commandList->getCommandListPerThreadScratchSize(1));

    uint64_t surfaceHeapGpuBase = getSurfStateGpuBase(false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdListStream->getCpuBase(), usedBefore),
        usedAfter - usedBefore));

    auto walkerIterator = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), walkerIterator);
    void *walkerPtrWithScratch = *walkerIterator;

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto commandListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto scratchAddress = scratchController->getScratchPatchAddress();
    auto fullScratchAddress = surfaceHeapGpuBase + scratchAddress;

    auto patchIndex = launchParams.scratchAddressPatchIndex;
    ASSERT_TRUE(commandList->commandsToPatch.size() > patchIndex);

    auto currentScratchPatchAddress = commandList->commandsToPatch[patchIndex].scratchAddressAfterPatch;
    auto expectedScratchPatchAddress = getExpectedScratchPatchAddress(scratchAddress);

    EXPECT_EQ(expectedScratchPatchAddress, currentScratchPatchAddress);
    EXPECT_EQ(scratchController, commandList->getCommandListUsedScratchController());

    uint64_t scratchInlineValue = 0;

    void *scratchInlinePtr = ptrOffset(walkerPtrWithScratch, (inlineOffset + scratchInlineOffset));
    std::memcpy(&scratchInlineValue, scratchInlinePtr, sizeof(scratchInlineValue));
    EXPECT_EQ(fullScratchAddress, scratchInlineValue);
}

template <typename FamilyType>
void CommandListScratchPatchFixtureInit::testScratchUndefinedPatching() {
    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = ZE_RESULT_SUCCESS;

    struct TestParam {
        uint8_t pointerSize;
        InlineDataOffset offset;
    };

    std::vector<TestParam> testParams = {
        {undefined<uint8_t>, 0u},
        {8u, undefined<InlineDataOffset>}};

    for (const auto &testParam : testParams) {
        mockKernelImmData->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress.pointerSize = testParam.pointerSize;
        mockKernelImmData->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress.offset = testParam.offset;

        result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        bool foundScratchPatchCmd = false;
        const auto &cmdsToPatch = commandList->getCommandsToPatch();
        for (const auto &cmdToPatch : cmdsToPatch) {
            if (cmdToPatch.type == CommandToPatch::CommandType::ComputeWalkerInlineDataScratch) {
                foundScratchPatchCmd = true;
                if (isUndefined(testParam.pointerSize)) {
                    EXPECT_TRUE(isUndefined(cmdToPatch.patchSize));
                }
                if (isUndefined(testParam.offset)) {
                    EXPECT_TRUE(isUndefined(cmdToPatch.offset));
                }
                break;
            }
        }
        EXPECT_TRUE(foundScratchPatchCmd);

        result = commandList->reset();
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    }
}

} // namespace ult
} // namespace L0
