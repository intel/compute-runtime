/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/unit_test_helper.h"

#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"

namespace L0 {
namespace ult {

template <typename FamilyType>
void validateTimestampRegisters(GenCmdList &cmdList,
                                GenCmdList::iterator &startIt,
                                uint32_t firstLoadRegisterRegSrcAddress,
                                uint64_t firstStoreRegMemAddress,
                                uint32_t secondLoadRegisterRegSrcAddress,
                                uint64_t secondStoreRegMemAddress,
                                bool workloadPartition) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    constexpr uint32_t mask = 0xfffffffe;

    auto itor = find<MI_LOAD_REGISTER_REG *>(startIt, cmdList.end());

    {
        ASSERT_NE(cmdList.end(), itor);
        auto cmdLoadReg = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
        EXPECT_EQ(firstLoadRegisterRegSrcAddress, cmdLoadReg->getSourceRegisterAddress());
        EXPECT_EQ(CS_GPR_R0, cmdLoadReg->getDestinationRegisterAddress());
    }

    itor++;
    {
        ASSERT_NE(cmdList.end(), itor);
        auto cmdLoadImm = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
        EXPECT_EQ(CS_GPR_R1, cmdLoadImm->getRegisterOffset());
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
        EXPECT_EQ(CS_GPR_R2, cmdMem->getRegisterAddress());
        EXPECT_EQ(firstStoreRegMemAddress, cmdMem->getMemoryAddress());
        if (workloadPartition) {
            EXPECT_TRUE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
        } else {
            EXPECT_FALSE(UnitTestHelper<FamilyType>::getWorkloadPartitionForStoreRegisterMemCmd(*cmdMem));
        }
    }

    itor++;
    {
        ASSERT_NE(cmdList.end(), itor);
        auto cmdLoadReg = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
        EXPECT_EQ(secondLoadRegisterRegSrcAddress, cmdLoadReg->getSourceRegisterAddress());
        EXPECT_EQ(CS_GPR_R0, cmdLoadReg->getDestinationRegisterAddress());
    }

    itor++;
    {
        ASSERT_NE(cmdList.end(), itor);
        auto cmdLoadImm = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
        EXPECT_EQ(CS_GPR_R1, cmdLoadImm->getRegisterOffset());
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
        EXPECT_EQ(CS_GPR_R2, cmdMem->getRegisterAddress());
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
void CmdListPipelineSelectStateFixture::testBody() {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    void *currentBuffer = nullptr;

    auto &cmdlistRequiredState = commandList->getRequiredStreamState();
    auto &cmdListFinalState = commandList->getFinalStreamState();
    auto &csrState = commandQueue->csr->getStreamProperties();

    auto commandListHandle = commandList->toHandle();

    auto &commandListStream = *commandList->commandContainer.getCommandStream();
    auto &cmdQueueStream = commandQueue->commandStream;

    GenCmdList cmdList;
    std::vector<GenCmdList::iterator> pipelineSelectList;
    size_t sizeBefore = 0;
    size_t sizeAfter = 0;
    auto result = ZE_RESULT_SUCCESS;

    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesSystolicPipelineSelectMode = 0;

        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(0, cmdlistRequiredState.pipelineSelect.systolicMode.value);
        EXPECT_EQ(0, cmdListFinalState.pipelineSelect.systolicMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, pipelineSelectList.size());

        cmdList.clear();
        pipelineSelectList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_EQ(0, csrState.pipelineSelect.systolicMode.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, pipelineSelectList.size());

        auto pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(*pipelineSelectList[0]);
        EXPECT_FALSE(NEO::UnitTestHelper<FamilyType>::getSystolicFlagValueFromPipelineSelectCommand(*pipelineSelectCmd));

        cmdList.clear();
        pipelineSelectList.clear();
        commandList->reset();
    }

    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesSystolicPipelineSelectMode = 1;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(1, cmdlistRequiredState.pipelineSelect.systolicMode.value);
        EXPECT_EQ(1, cmdListFinalState.pipelineSelect.systolicMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, pipelineSelectList.size());

        cmdList.clear();
        pipelineSelectList.clear();

        mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesSystolicPipelineSelectMode = 0;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(1, cmdlistRequiredState.pipelineSelect.systolicMode.value);
        EXPECT_EQ(0, cmdListFinalState.pipelineSelect.systolicMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, pipelineSelectList.size());

        auto pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(*pipelineSelectList[0]);
        EXPECT_FALSE(NEO::UnitTestHelper<FamilyType>::getSystolicFlagValueFromPipelineSelectCommand(*pipelineSelectCmd));

        cmdList.clear();
        pipelineSelectList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_EQ(0, csrState.pipelineSelect.systolicMode.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, pipelineSelectList.size());

        pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(*pipelineSelectList[0]);
        EXPECT_TRUE(NEO::UnitTestHelper<FamilyType>::getSystolicFlagValueFromPipelineSelectCommand(*pipelineSelectCmd));

        cmdList.clear();
        pipelineSelectList.clear();
        commandList->reset();
    }

    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesSystolicPipelineSelectMode = 1;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(1, cmdlistRequiredState.pipelineSelect.systolicMode.value);
        EXPECT_EQ(1, cmdListFinalState.pipelineSelect.systolicMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, pipelineSelectList.size());

        cmdList.clear();
        pipelineSelectList.clear();

        mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesSystolicPipelineSelectMode = 0;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(1, cmdlistRequiredState.pipelineSelect.systolicMode.value);
        EXPECT_EQ(0, cmdListFinalState.pipelineSelect.systolicMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, pipelineSelectList.size());

        auto pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(*pipelineSelectList[0]);
        EXPECT_FALSE(NEO::UnitTestHelper<FamilyType>::getSystolicFlagValueFromPipelineSelectCommand(*pipelineSelectCmd));

        cmdList.clear();
        pipelineSelectList.clear();

        mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesSystolicPipelineSelectMode = 1;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(1, cmdlistRequiredState.pipelineSelect.systolicMode.value);
        EXPECT_EQ(1, cmdListFinalState.pipelineSelect.systolicMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, pipelineSelectList.size());

        pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(*pipelineSelectList[0]);
        EXPECT_TRUE(NEO::UnitTestHelper<FamilyType>::getSystolicFlagValueFromPipelineSelectCommand(*pipelineSelectCmd));

        cmdList.clear();
        pipelineSelectList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_EQ(1, csrState.pipelineSelect.systolicMode.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, pipelineSelectList.size());

        pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(*pipelineSelectList[0]);
        EXPECT_TRUE(NEO::UnitTestHelper<FamilyType>::getSystolicFlagValueFromPipelineSelectCommand(*pipelineSelectCmd));

        cmdList.clear();
        pipelineSelectList.clear();
        commandList->reset();
    }
    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.flags.usesSystolicPipelineSelectMode = 1;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(1, cmdlistRequiredState.pipelineSelect.systolicMode.value);
        EXPECT_EQ(1, cmdListFinalState.pipelineSelect.systolicMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, pipelineSelectList.size());

        cmdList.clear();
        pipelineSelectList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_EQ(1, csrState.pipelineSelect.systolicMode.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, pipelineSelectList.size());
    }
}

} // namespace ult
} // namespace L0
