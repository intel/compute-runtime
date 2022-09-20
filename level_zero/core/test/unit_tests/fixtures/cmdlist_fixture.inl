/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"

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

template <typename FamilyType>
void CmdListPipelineSelectStateFixture::testBodyShareStateRegularImmediate() {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    const ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    void *currentBuffer = nullptr;

    auto &regularCmdlistRequiredState = commandList->getRequiredStreamState();
    auto &regularCmdListFinalState = commandList->getFinalStreamState();
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
    result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    sizeAfter = regularCommandListStream.getUsed();

    EXPECT_EQ(1, regularCmdlistRequiredState.pipelineSelect.systolicMode.value);
    EXPECT_EQ(1, regularCmdListFinalState.pipelineSelect.systolicMode.value);

    currentBuffer = ptrOffset(regularCommandListStream.getCpuBase(), sizeBefore);
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
    ASSERT_EQ(1u, pipelineSelectList.size());

    auto pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(*pipelineSelectList[0]);
    EXPECT_TRUE(NEO::UnitTestHelper<FamilyType>::getSystolicFlagValueFromPipelineSelectCommand(*pipelineSelectCmd));

    cmdList.clear();
    pipelineSelectList.clear();

    auto &immediateCmdListStream = *commandListImmediate->commandContainer.getCommandStream();
    EXPECT_EQ(commandQueue->csr, commandListImmediate->csr);

    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = ultCsr.commandStream;

    size_t csrUsedBefore = csrStream.getUsed();
    sizeBefore = immediateCmdListStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    sizeAfter = immediateCmdListStream.getUsed();
    size_t csrUsedAfter = csrStream.getUsed();

    auto &immediateCmdListRequiredState = commandListImmediate->getRequiredStreamState();
    auto &immediateCmdListFinalState = commandListImmediate->getFinalStreamState();

    EXPECT_EQ(1, immediateCmdListRequiredState.pipelineSelect.systolicMode.value);
    EXPECT_EQ(1, immediateCmdListFinalState.pipelineSelect.systolicMode.value);

    currentBuffer = ptrOffset(immediateCmdListStream.getCpuBase(), sizeBefore);
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      currentBuffer,
                                                      (sizeAfter - sizeBefore)));
    pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, pipelineSelectList.size());

    cmdList.clear();
    pipelineSelectList.clear();

    EXPECT_EQ(1, csrState.pipelineSelect.systolicMode.value);

    currentBuffer = ptrOffset(csrStream.getCpuBase(), csrUsedBefore);
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
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
    auto &immediateCmdListFinalState = commandListImmediate->getFinalStreamState();

    auto &immediateCmdListStream = *commandListImmediate->commandContainer.getCommandStream();

    auto &csrState = commandQueue->csr->getStreamProperties();

    EXPECT_EQ(commandQueue->csr, commandListImmediate->csr);

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
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    sizeAfter = immediateCmdListStream.getUsed();
    size_t csrUsedAfter = csrStream.getUsed();

    EXPECT_EQ(1, immediateCmdListRequiredState.pipelineSelect.systolicMode.value);
    EXPECT_EQ(1, immediateCmdListFinalState.pipelineSelect.systolicMode.value);

    currentBuffer = ptrOffset(immediateCmdListStream.getCpuBase(), sizeBefore);
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      currentBuffer,
                                                      (sizeAfter - sizeBefore)));
    pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, pipelineSelectList.size());

    cmdList.clear();
    pipelineSelectList.clear();

    EXPECT_EQ(1, csrState.pipelineSelect.systolicMode.value);

    currentBuffer = ptrOffset(csrStream.getCpuBase(), csrUsedBefore);
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      currentBuffer,
                                                      (csrUsedAfter - csrUsedBefore)));
    pipelineSelectList = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, pipelineSelectList.size());

    auto pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(*pipelineSelectList[0]);
    EXPECT_TRUE(NEO::UnitTestHelper<FamilyType>::getSystolicFlagValueFromPipelineSelectCommand(*pipelineSelectCmd));

    cmdList.clear();
    pipelineSelectList.clear();

    auto &regularCmdlistRequiredState = commandList->getRequiredStreamState();
    auto &regularCmdListFinalState = commandList->getFinalStreamState();

    auto commandListHandle = commandList->toHandle();

    auto &regularCommandListStream = *commandList->commandContainer.getCommandStream();
    auto &cmdQueueStream = commandQueue->commandStream;

    sizeBefore = regularCommandListStream.getUsed();
    result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    sizeAfter = regularCommandListStream.getUsed();

    EXPECT_EQ(1, regularCmdlistRequiredState.pipelineSelect.systolicMode.value);
    EXPECT_EQ(1, regularCmdListFinalState.pipelineSelect.systolicMode.value);

    currentBuffer = ptrOffset(regularCommandListStream.getCpuBase(), sizeBefore);
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

template <typename FamilyType>
void CmdListThreadArbitrationFixture::testBody() {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using EU_THREAD_SCHEDULING_MODE_OVERRIDE = typename STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_OVERRIDE;

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
    std::vector<GenCmdList::iterator> stateComputeModeList;
    size_t sizeBefore = 0;
    size_t sizeAfter = 0;
    auto result = ZE_RESULT_SUCCESS;

    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.threadArbitrationPolicy = NEO::ThreadArbitrationPolicy::AgeBased;

        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::AgeBased, cmdlistRequiredState.stateComputeMode.threadArbitrationPolicy.value);
        EXPECT_EQ(NEO::ThreadArbitrationPolicy::AgeBased, cmdListFinalState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, stateComputeModeList.size());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::AgeBased, csrState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateComputeModeList.size());

        auto stateComputeModeCmd = genCmdCast<STATE_COMPUTE_MODE *>(*stateComputeModeList[0]);
        EXPECT_EQ(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_OLDEST_FIRST, stateComputeModeCmd->getEuThreadSchedulingModeOverride());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->reset();
    }

    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.threadArbitrationPolicy = NEO::ThreadArbitrationPolicy::RoundRobin;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobin, cmdlistRequiredState.stateComputeMode.threadArbitrationPolicy.value);
        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobin, cmdListFinalState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, stateComputeModeList.size());

        cmdList.clear();
        stateComputeModeList.clear();

        mockKernelImmData->kernelDescriptor->kernelAttributes.threadArbitrationPolicy = NEO::ThreadArbitrationPolicy::AgeBased;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobin, cmdlistRequiredState.stateComputeMode.threadArbitrationPolicy.value);
        EXPECT_EQ(NEO::ThreadArbitrationPolicy::AgeBased, cmdListFinalState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateComputeModeList.size());

        auto stateComputeModeCmd = genCmdCast<STATE_COMPUTE_MODE *>(*stateComputeModeList[0]);
        EXPECT_EQ(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_OLDEST_FIRST, stateComputeModeCmd->getEuThreadSchedulingModeOverride());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::AgeBased, csrState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateComputeModeList.size());

        stateComputeModeCmd = genCmdCast<STATE_COMPUTE_MODE *>(*stateComputeModeList[0]);
        EXPECT_EQ(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN, stateComputeModeCmd->getEuThreadSchedulingModeOverride());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->reset();
    }

    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.threadArbitrationPolicy = NEO::ThreadArbitrationPolicy::RoundRobin;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobin, cmdlistRequiredState.stateComputeMode.threadArbitrationPolicy.value);
        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobin, cmdListFinalState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, stateComputeModeList.size());

        cmdList.clear();
        stateComputeModeList.clear();

        mockKernelImmData->kernelDescriptor->kernelAttributes.threadArbitrationPolicy = NEO::ThreadArbitrationPolicy::AgeBased;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobin, cmdlistRequiredState.stateComputeMode.threadArbitrationPolicy.value);
        EXPECT_EQ(NEO::ThreadArbitrationPolicy::AgeBased, cmdListFinalState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateComputeModeList.size());

        auto stateComputeModeCmd = genCmdCast<STATE_COMPUTE_MODE *>(*stateComputeModeList[0]);
        EXPECT_EQ(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_OLDEST_FIRST, stateComputeModeCmd->getEuThreadSchedulingModeOverride());

        cmdList.clear();
        stateComputeModeList.clear();

        mockKernelImmData->kernelDescriptor->kernelAttributes.threadArbitrationPolicy = NEO::ThreadArbitrationPolicy::RoundRobinAfterDependency;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobin, cmdlistRequiredState.stateComputeMode.threadArbitrationPolicy.value);
        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobinAfterDependency, cmdListFinalState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateComputeModeList.size());

        stateComputeModeCmd = genCmdCast<STATE_COMPUTE_MODE *>(*stateComputeModeList[0]);
        EXPECT_EQ(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN, stateComputeModeCmd->getEuThreadSchedulingModeOverride());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobinAfterDependency, csrState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateComputeModeList.size());

        stateComputeModeCmd = genCmdCast<STATE_COMPUTE_MODE *>(*stateComputeModeList[0]);
        EXPECT_EQ(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN, stateComputeModeCmd->getEuThreadSchedulingModeOverride());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->reset();
    }
    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.threadArbitrationPolicy = NEO::ThreadArbitrationPolicy::RoundRobinAfterDependency;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobinAfterDependency, cmdlistRequiredState.stateComputeMode.threadArbitrationPolicy.value);
        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobinAfterDependency, cmdListFinalState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, stateComputeModeList.size());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_EQ(NEO::ThreadArbitrationPolicy::RoundRobinAfterDependency, csrState.stateComputeMode.threadArbitrationPolicy.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
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

    auto commandListHandle = commandList->toHandle();

    auto &commandListStream = *commandList->commandContainer.getCommandStream();
    auto &cmdQueueStream = commandQueue->commandStream;

    GenCmdList cmdList;
    std::vector<GenCmdList::iterator> stateComputeModeList;
    size_t sizeBefore = 0;
    size_t sizeAfter = 0;
    auto result = ZE_RESULT_SUCCESS;

    {
        mockKernelImmData->kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::DefaultGrfNumber;

        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(0, cmdlistRequiredState.stateComputeMode.largeGrfMode.value);
        EXPECT_EQ(0, cmdListFinalState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, stateComputeModeList.size());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_EQ(0, csrState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
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
        mockKernelImmData->kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::LargeGrfNumber;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(1, cmdlistRequiredState.stateComputeMode.largeGrfMode.value);
        EXPECT_EQ(1, cmdListFinalState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, stateComputeModeList.size());

        cmdList.clear();
        stateComputeModeList.clear();

        mockKernelImmData->kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::DefaultGrfNumber;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(1, cmdlistRequiredState.stateComputeMode.largeGrfMode.value);
        EXPECT_EQ(0, cmdListFinalState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
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
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_EQ(0, csrState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
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
        mockKernelImmData->kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::LargeGrfNumber;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(1, cmdlistRequiredState.stateComputeMode.largeGrfMode.value);
        EXPECT_EQ(1, cmdListFinalState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, stateComputeModeList.size());

        cmdList.clear();
        stateComputeModeList.clear();

        mockKernelImmData->kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::DefaultGrfNumber;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(1, cmdlistRequiredState.stateComputeMode.largeGrfMode.value);
        EXPECT_EQ(0, cmdListFinalState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateComputeModeList.size());

        auto stateComputeModeCmd = genCmdCast<STATE_COMPUTE_MODE *>(*stateComputeModeList[0]);
        EXPECT_FALSE(stateComputeModeCmd->getLargeGrfMode());

        cmdList.clear();
        stateComputeModeList.clear();

        mockKernelImmData->kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::LargeGrfNumber;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(1, cmdlistRequiredState.stateComputeMode.largeGrfMode.value);
        EXPECT_EQ(1, cmdListFinalState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
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
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_EQ(1, csrState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
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
        mockKernelImmData->kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::LargeGrfNumber;
        sizeBefore = commandListStream.getUsed();
        result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = commandListStream.getUsed();

        EXPECT_EQ(1, cmdlistRequiredState.stateComputeMode.largeGrfMode.value);
        EXPECT_EQ(1, cmdListFinalState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(commandListStream.getCpuBase(), sizeBefore);

        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, stateComputeModeList.size());

        cmdList.clear();
        stateComputeModeList.clear();
        commandList->close();

        sizeBefore = cmdQueueStream.getUsed();
        result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        sizeAfter = cmdQueueStream.getUsed();

        EXPECT_EQ(1, csrState.stateComputeMode.largeGrfMode.value);

        currentBuffer = ptrOffset(cmdQueueStream.getCpuBase(), sizeBefore);
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                          currentBuffer,
                                                          (sizeAfter - sizeBefore)));
        stateComputeModeList = findAll<STATE_COMPUTE_MODE *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(0u, stateComputeModeList.size());
    }
}

} // namespace ult
} // namespace L0
