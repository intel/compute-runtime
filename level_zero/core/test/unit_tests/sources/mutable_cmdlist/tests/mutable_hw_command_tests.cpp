/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_command_walker_hw.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/mutable_hw_command_fixture.h"

namespace L0 {

namespace ult {

using MutableHwCommandTest = Test<MutableHwCommandFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableHwCommandTest,
            givenMutableComputeWalkerWhenCallingBasicGettersThenCorrectValuesAreReturned) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);

    EXPECT_EQ(this->indirectOffset, mutableWalker->getIndirectOffset());
    EXPECT_EQ(this->scratchOffset, mutableWalker->getScratchOffset());
    EXPECT_EQ(this->cmdBufferGpuPtr, mutableWalker->getWalkerCmdPointer());

    size_t inlineDataOffset = offsetof(WalkerType, TheStructure.Common.InlineData);
    EXPECT_EQ(inlineDataOffset, mutableWalker->getInlineDataOffset());

    auto walkerInlineDataPtr = ptrOffset(this->cmdBufferGpuPtr, inlineDataOffset);
    EXPECT_EQ(walkerInlineDataPtr, mutableWalker->getInlineDataPointer());

    auto cmdBufferInlineDataPtr = ptrOffset(this->cmdBufferCpuPtr, inlineDataOffset);
    EXPECT_EQ(cmdBufferInlineDataPtr, mutableWalker->getHostMemoryInlineDataPointer());

    size_t inlineDataSize = sizeof(WalkerType::TheStructure.Common.InlineData);
    EXPECT_EQ(inlineDataSize, mutableWalker->getInlineDataSize());

    auto mutableWalkerHw = static_cast<L0::MCL::MutableComputeWalkerHw<FamilyType> *>(this->mutableWalker.get());
    EXPECT_EQ(sizeof(WalkerType), mutableWalkerHw->getCommandSize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableHwCommandTest,
            givenMutableComputeWalkerMatchesComputeWalkerWhenObjectIsCreatedAndCopiedFromCpuMemoryThenExactCopyIsExpected) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, false);

    mutableWalker->saveCpuBufferIntoGpuBuffer(false);

    EXPECT_EQ(0, memcmp(this->cmdBufferCpuPtr, this->cmdBufferGpuPtr, this->walkerCmdSize));

    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, false);
    auto cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);
    // dummy address to mutate in next step
    cpuBuffer->getPostSync().setDestinationAddress(0x2000);

    uint64_t postSyncAddress = 0x1000;
    mutableWalker->setPostSyncAddress(postSyncAddress, 0);

    memset(this->cmdBufferGpuPtr, 0, this->walkerCmdSize);

    // true parameter will not save post syncs to gpu buffer
    mutableWalker->saveCpuBufferIntoGpuBuffer(true);
    EXPECT_NE(0, memcmp(this->cmdBufferCpuPtr, this->cmdBufferGpuPtr, this->walkerCmdSize));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableHwCommandTest,
            givenMutableComputeWalkerWhenSettingLocalIdsWalkOrderParametersThenCorrectValueSet) {
    using WalkerType = typename FamilyType::PorWalkerType;
    constexpr uint32_t emitLocalValue = 0b111;

    auto walkerCmd = reinterpret_cast<WalkerType *>(this->cmdBufferGpuPtr);

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    uint32_t walkOrder = 1;
    bool generateLocalId = true;

    mutableWalker->setGenerateLocalId(generateLocalId, walkOrder, 3);
    EXPECT_EQ(emitLocalValue, cpuBuffer->getEmitLocalId());
    EXPECT_EQ(generateLocalId, cpuBuffer->getGenerateLocalId());
    EXPECT_EQ(walkOrder, cpuBuffer->getWalkOrder());

    EXPECT_EQ(0u, walkerCmd->getEmitLocalId());
    EXPECT_EQ(false, walkerCmd->getGenerateLocalId());
    EXPECT_EQ(0u, walkerCmd->getWalkOrder());

    generateLocalId = false;

    mutableWalker->setGenerateLocalId(generateLocalId, walkOrder, 3);
    EXPECT_EQ(0u, cpuBuffer->getEmitLocalId());
    EXPECT_EQ(generateLocalId, cpuBuffer->getGenerateLocalId());
    EXPECT_EQ(walkOrder, cpuBuffer->getWalkOrder());

    this->stageCommit = false;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    generateLocalId = true;

    mutableWalker->setGenerateLocalId(generateLocalId, walkOrder, 3);
    EXPECT_EQ(emitLocalValue, cpuBuffer->getEmitLocalId());
    EXPECT_EQ(generateLocalId, cpuBuffer->getGenerateLocalId());
    EXPECT_EQ(walkOrder, cpuBuffer->getWalkOrder());

    EXPECT_EQ(emitLocalValue, walkerCmd->getEmitLocalId());
    EXPECT_EQ(generateLocalId, walkerCmd->getGenerateLocalId());
    EXPECT_EQ(walkOrder, walkerCmd->getWalkOrder());

    // if cpu buffer is the same as argument, further update is stopped, verify it by cleaning gpu buffer
    memset(this->cmdBufferGpuPtr, 0, this->walkerCmdSize);

    mutableWalker->setGenerateLocalId(generateLocalId, walkOrder, 0);
    EXPECT_EQ(emitLocalValue, cpuBuffer->getEmitLocalId());
    EXPECT_EQ(generateLocalId, cpuBuffer->getGenerateLocalId());
    EXPECT_EQ(walkOrder, cpuBuffer->getWalkOrder());

    EXPECT_EQ(0u, walkerCmd->getEmitLocalId());
    EXPECT_EQ(false, walkerCmd->getGenerateLocalId());
    EXPECT_EQ(0u, walkerCmd->getWalkOrder());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableHwCommandTest,
            givenMutableComputeWalkerWith2LocalIdChannelsWhenSettingLocalIdsWalkOrderParametersThenCorrectValueSet) {
    using WalkerType = typename FamilyType::PorWalkerType;

    constexpr uint32_t emitLocalValue2 = 0b11;
    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    uint32_t walkOrder = 1;
    bool generateLocalId = true;

    mutableWalker->setGenerateLocalId(generateLocalId, walkOrder, 2);
    EXPECT_EQ(emitLocalValue2, cpuBuffer->getEmitLocalId());
    EXPECT_EQ(generateLocalId, cpuBuffer->getGenerateLocalId());
    EXPECT_EQ(walkOrder, cpuBuffer->getWalkOrder());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableHwCommandTest,
            givenMutableComputeWalkerWith1LocalIdChannelWhenSettingLocalIdsWalkOrderParametersThenCorrectValueSet) {
    using WalkerType = typename FamilyType::PorWalkerType;

    constexpr uint32_t emitLocalValue = 0b1;
    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    uint32_t walkOrder = 1;
    bool generateLocalId = true;

    mutableWalker->setGenerateLocalId(generateLocalId, walkOrder, 1);
    EXPECT_EQ(emitLocalValue, cpuBuffer->getEmitLocalId());
    EXPECT_EQ(generateLocalId, cpuBuffer->getGenerateLocalId());
    EXPECT_EQ(walkOrder, cpuBuffer->getWalkOrder());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableHwCommandTest,
            givenMutableComputeWalkerWithZeroLocalIdChannelsWhenSettingLocalIdsThenEmitLocalIdsIsNotSet) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    uint32_t walkOrder = 1;
    bool generateLocalId = true;

    mutableWalker->setGenerateLocalId(generateLocalId, walkOrder, 0);
    EXPECT_EQ(0u, cpuBuffer->getEmitLocalId());
    EXPECT_EQ(generateLocalId, cpuBuffer->getGenerateLocalId());
    EXPECT_EQ(walkOrder, cpuBuffer->getWalkOrder());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableHwCommandTest,
            givenMutableComputeWalkerWhenSettingThreadsPerGroupThenCorrectValueSet) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerCmd = reinterpret_cast<WalkerType *>(this->cmdBufferGpuPtr);

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    uint32_t numThreadPerThreadGroup = 4;

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->setNumberThreadsPerThreadGroup(numThreadPerThreadGroup);
    EXPECT_EQ(numThreadPerThreadGroup, cpuBuffer->getInterfaceDescriptor().getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_EQ(0u, walkerCmd->getInterfaceDescriptor().getNumberOfThreadsInGpgpuThreadGroup());

    this->stageCommit = false;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->setNumberThreadsPerThreadGroup(numThreadPerThreadGroup);
    EXPECT_EQ(numThreadPerThreadGroup, cpuBuffer->getInterfaceDescriptor().getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_EQ(numThreadPerThreadGroup, walkerCmd->getInterfaceDescriptor().getNumberOfThreadsInGpgpuThreadGroup());

    memset(this->cmdBufferGpuPtr, 0, this->walkerCmdSize);

    mutableWalker->setNumberThreadsPerThreadGroup(numThreadPerThreadGroup);
    EXPECT_EQ(numThreadPerThreadGroup, cpuBuffer->getInterfaceDescriptor().getNumberOfThreadsInGpgpuThreadGroup());
    EXPECT_EQ(0u, walkerCmd->getInterfaceDescriptor().getNumberOfThreadsInGpgpuThreadGroup());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableHwCommandTest,
            givenMutableComputeWalkerWhenSettingNumberOfWorkGroupsThenCorrectValueSet) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerCmd = reinterpret_cast<WalkerType *>(this->cmdBufferGpuPtr);

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    std::array<uint32_t, 3> numberWorkGroups = {4, 2, 2};

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->setNumberWorkGroups(numberWorkGroups);

    EXPECT_EQ(numberWorkGroups[0], cpuBuffer->getThreadGroupIdXDimension());
    EXPECT_EQ(numberWorkGroups[1], cpuBuffer->getThreadGroupIdYDimension());
    EXPECT_EQ(numberWorkGroups[2], cpuBuffer->getThreadGroupIdZDimension());
    EXPECT_EQ(0u, walkerCmd->getThreadGroupIdXDimension());
    EXPECT_EQ(0u, walkerCmd->getThreadGroupIdYDimension());
    EXPECT_EQ(0u, walkerCmd->getThreadGroupIdZDimension());

    this->stageCommit = false;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->setNumberWorkGroups(numberWorkGroups);

    EXPECT_EQ(numberWorkGroups[0], cpuBuffer->getThreadGroupIdXDimension());
    EXPECT_EQ(numberWorkGroups[1], cpuBuffer->getThreadGroupIdYDimension());
    EXPECT_EQ(numberWorkGroups[2], cpuBuffer->getThreadGroupIdZDimension());
    EXPECT_EQ(numberWorkGroups[0], walkerCmd->getThreadGroupIdXDimension());
    EXPECT_EQ(numberWorkGroups[1], walkerCmd->getThreadGroupIdYDimension());
    EXPECT_EQ(numberWorkGroups[2], walkerCmd->getThreadGroupIdZDimension());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableHwCommandTest,
            givenMutableComputeWalkerWhenSettingGroupSizeThenCorrectValueSet) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerCmd = reinterpret_cast<WalkerType *>(this->cmdBufferGpuPtr);

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    std::array<uint32_t, 3> workgroupSize = {4, 2, 2};

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->setWorkGroupSize(workgroupSize);

    EXPECT_EQ(workgroupSize[0] - 1, cpuBuffer->getLocalXMaximum());
    EXPECT_EQ(workgroupSize[1] - 1, cpuBuffer->getLocalYMaximum());
    EXPECT_EQ(workgroupSize[2] - 1, cpuBuffer->getLocalZMaximum());
    EXPECT_EQ(0u, walkerCmd->getLocalXMaximum());
    EXPECT_EQ(0u, walkerCmd->getLocalYMaximum());
    EXPECT_EQ(0u, walkerCmd->getLocalZMaximum());

    this->stageCommit = false;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->setWorkGroupSize(workgroupSize);

    EXPECT_EQ(workgroupSize[0] - 1, cpuBuffer->getLocalXMaximum());
    EXPECT_EQ(workgroupSize[1] - 1, cpuBuffer->getLocalYMaximum());
    EXPECT_EQ(workgroupSize[2] - 1, cpuBuffer->getLocalZMaximum());
    EXPECT_EQ(workgroupSize[0] - 1, walkerCmd->getLocalXMaximum());
    EXPECT_EQ(workgroupSize[1] - 1, walkerCmd->getLocalYMaximum());
    EXPECT_EQ(workgroupSize[2] - 1, walkerCmd->getLocalZMaximum());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableHwCommandTest,
            givenMutableComputeWalkerWhenSettingExecutionMaskThenCorrectValueSet) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerCmd = reinterpret_cast<WalkerType *>(this->cmdBufferGpuPtr);

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    uint32_t executionMask = 1;

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->setExecutionMask(executionMask);
    EXPECT_EQ(executionMask, cpuBuffer->getExecutionMask());
    EXPECT_EQ(0u, walkerCmd->getExecutionMask());

    this->stageCommit = false;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->setExecutionMask(executionMask);
    EXPECT_EQ(executionMask, cpuBuffer->getExecutionMask());
    EXPECT_EQ(executionMask, walkerCmd->getExecutionMask());

    memset(this->cmdBufferGpuPtr, 0, this->walkerCmdSize);

    mutableWalker->setExecutionMask(executionMask);
    EXPECT_EQ(executionMask, cpuBuffer->getExecutionMask());
    EXPECT_EQ(0u, walkerCmd->getExecutionMask());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableHwCommandTest,
            givenMutableComputeWalkerWhenUpdateSlmThenCorrectValueSet) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerCmd = reinterpret_cast<WalkerType *>(this->cmdBufferGpuPtr);

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    uint32_t slmSize = 4u * MemoryConstants::kiloByte;
    auto expectedSlmSizeValue = NEO::EncodeDispatchKernel<FamilyType>::computeSlmValues(neoDevice->getHardwareInfo(), slmSize, neoDevice->getReleaseHelper(), this->isHeapless);

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->updateSlmSize(*this->neoDevice, slmSize);

    EXPECT_EQ(expectedSlmSizeValue, cpuBuffer->getInterfaceDescriptor().getSharedLocalMemorySize());
    EXPECT_EQ(0u, walkerCmd->getInterfaceDescriptor().getSharedLocalMemorySize());

    this->stageCommit = false;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->updateSlmSize(*this->neoDevice, slmSize);
    EXPECT_EQ(expectedSlmSizeValue, cpuBuffer->getInterfaceDescriptor().getSharedLocalMemorySize());
    EXPECT_EQ(expectedSlmSizeValue, walkerCmd->getInterfaceDescriptor().getSharedLocalMemorySize());

    memset(this->cmdBufferGpuPtr, 0, this->walkerCmdSize);

    mutableWalker->updateSlmSize(*this->neoDevice, slmSize);
    EXPECT_EQ(expectedSlmSizeValue, cpuBuffer->getInterfaceDescriptor().getSharedLocalMemorySize());
    EXPECT_EQ(0u, walkerCmd->getInterfaceDescriptor().getSharedLocalMemorySize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableHwCommandTest,
            givenMutableComputeWalkerWhenDebugFlagOverridesSlmAndUpdateSlmCalledThenOverrideValueSet) {
    using WalkerType = typename FamilyType::PorWalkerType;
    DebugManagerStateRestore restorer;

    auto walkerCmd = reinterpret_cast<WalkerType *>(this->cmdBufferGpuPtr);

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    uint32_t slmSize = 4u * MemoryConstants::kiloByte;
    constexpr uint32_t overrideSlmSize = 16u * MemoryConstants::kiloByte;

    auto expectedSlmSizeValue = NEO::EncodeDispatchKernel<FamilyType>::computeSlmValues(neoDevice->getHardwareInfo(), overrideSlmSize, neoDevice->getReleaseHelper(), this->isHeapless);

    debugManager.flags.OverrideSlmAllocationSize.set(expectedSlmSizeValue);

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->updateSlmSize(*this->neoDevice, slmSize);

    EXPECT_EQ(expectedSlmSizeValue, cpuBuffer->getInterfaceDescriptor().getSharedLocalMemorySize());
    EXPECT_EQ(0u, walkerCmd->getInterfaceDescriptor().getSharedLocalMemorySize());

    this->stageCommit = false;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    cpuBuffer = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    mutableWalker->updateSlmSize(*this->neoDevice, slmSize);
    EXPECT_EQ(expectedSlmSizeValue, cpuBuffer->getInterfaceDescriptor().getSharedLocalMemorySize());
    EXPECT_EQ(expectedSlmSizeValue, walkerCmd->getInterfaceDescriptor().getSharedLocalMemorySize());

    memset(this->cmdBufferGpuPtr, 0, this->walkerCmdSize);

    mutableWalker->updateSlmSize(*this->neoDevice, slmSize);
    EXPECT_EQ(expectedSlmSizeValue, cpuBuffer->getInterfaceDescriptor().getSharedLocalMemorySize());
    EXPECT_EQ(0u, walkerCmd->getInterfaceDescriptor().getSharedLocalMemorySize());
}

HWTEST2_F(MutableHwCommandTest,
          givenMutableComputeWalkerWhenSettingThreadGroupDispatchFieldsFromEncodersThenCorrectFieldsAreSet,
          IsAtLeastXeHpcCore) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    alignas(8) uint8_t controlWalkerBuffer[sizeof(WalkerType)];
    auto controlWalker = reinterpret_cast<WalkerType *>(controlWalkerBuffer);
    *controlWalker = walkerTemplate;
    auto controlIdd = controlWalker->getInterfaceDescriptor();

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    auto walkerArgs = createMutableWalkerSpecificFieldsArguments();
    fillWalkerFields<WalkerType>(controlWalkerBuffer);

    NEO::EncodeDispatchKernel<FamilyType>::encodeThreadGroupDispatch(controlIdd, *neoDevice, neoDevice->getHardwareInfo(),
                                                                     walkerArgs.threadGroupDimensions, walkerArgs.threadGroupCount,
                                                                     walkerArgs.requiredThreadGroupDispatchSize, walkerArgs.grfCount,
                                                                     walkerArgs.threadsPerThreadGroup, *controlWalker);
    NEO::EncodeWalkerArgs encodeWalkerArgs{
        .kernelExecutionType = NEO::KernelExecutionType::defaultType,
        .requiredDispatchWalkOrder = NEO::RequiredDispatchWalkOrder::none,
        .localRegionSize = NEO::localRegionSizeParamNotSet,
        .maxFrontEndThreads = neoDevice->getDeviceInfo().maxFrontEndThreads,
        .requiredSystemFence = false,
        .hasSample = false};

    NEO::EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(*controlWalker, &controlIdd, neoDevice->getRootDeviceEnvironment(), encodeWalkerArgs);

    fillWalkerFields<WalkerType>(this->cmdBufferCpuPtr);
    walkerArgs.updateGroupCount = true;

    mutableWalker->updateSpecificFields(*neoDevice, walkerArgs);
    auto walkerCmdCpu = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);
    auto iddCpu = walkerCmdCpu->getInterfaceDescriptor();

    EXPECT_EQ(controlIdd.getThreadGroupDispatchSize(), iddCpu.getThreadGroupDispatchSize());
    EXPECT_EQ(controlWalker->getComputeDispatchAllWalkerEnable(), walkerCmdCpu->getComputeDispatchAllWalkerEnable());

    this->stageCommit = false;
    walkerArgs.updateGroupCount = false;
    walkerArgs.updateGroupSize = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    fillWalkerFields<WalkerType>(this->cmdBufferCpuPtr);

    mutableWalker->updateSpecificFields(*neoDevice, walkerArgs);
    auto walkerCmdGpu = reinterpret_cast<WalkerType *>(this->cmdBufferGpuPtr);
    auto iddGpu = walkerCmdGpu->getInterfaceDescriptor();

    EXPECT_EQ(controlIdd.getThreadGroupDispatchSize(), iddGpu.getThreadGroupDispatchSize());
    EXPECT_EQ(controlWalker->getComputeDispatchAllWalkerEnable(), walkerCmdGpu->getComputeDispatchAllWalkerEnable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableHwCommandTest,
            givenMutableComputeWalkerWhenSettingPreferredSlmSizeFieldsFromEncodersThenCorrectFieldsAreSet) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    alignas(8) uint8_t controlWalkerBuffer[sizeof(WalkerType)];
    auto controlWalker = reinterpret_cast<WalkerType *>(controlWalkerBuffer);
    *controlWalker = walkerTemplate;
    auto controlIdd = controlWalker->getInterfaceDescriptor();

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);

    this->slmTotalSize = 4 * MemoryConstants::kiloByte;
    auto walkerArgs = createMutableWalkerSpecificFieldsArguments();
    walkerArgs.isSlmKernel = true;
    walkerArgs.updateSlm = true;

    fillWalkerFields<WalkerType>(controlWalkerBuffer);

    NEO::EncodeDispatchKernel<FamilyType>::setupPreferredSlmSize(&controlIdd,
                                                                 neoDevice->getRootDeviceEnvironment(),
                                                                 walkerArgs.threadsPerThreadGroup,
                                                                 walkerArgs.slmTotalSize,
                                                                 static_cast<NEO::SlmPolicy>(walkerArgs.slmPolicy));

    fillWalkerFields<WalkerType>(this->cmdBufferCpuPtr);

    mutableWalker->updateSpecificFields(*neoDevice, walkerArgs);
    auto walkerCmdCpu = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);
    auto iddCpu = walkerCmdCpu->getInterfaceDescriptor();

    EXPECT_EQ(controlIdd.getPreferredSlmAllocationSize(), iddCpu.getPreferredSlmAllocationSize());

    this->stageCommit = false;
    walkerArgs.updateSlm = false;
    walkerArgs.updateGroupSize = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    fillWalkerFields<WalkerType>(this->cmdBufferCpuPtr);

    mutableWalker->updateSpecificFields(*neoDevice, walkerArgs);
    auto walkerCmdGpu = reinterpret_cast<WalkerType *>(this->cmdBufferGpuPtr);
    auto iddGpu = walkerCmdGpu->getInterfaceDescriptor();

    EXPECT_EQ(controlIdd.getPreferredSlmAllocationSize(), iddGpu.getPreferredSlmAllocationSize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableHwCommandTest,
            givenMutableComputeWalkerOnSlmKernelWhenSettingPreferredSlmSizeFieldsFromEncodersAndNoUpdateRequiredThenNoFieldsAreSet) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);

    this->slmTotalSize = 4 * MemoryConstants::kiloByte;
    auto walkerArgs = createMutableWalkerSpecificFieldsArguments();
    walkerArgs.isSlmKernel = true;
    walkerArgs.updateSlm = false;
    walkerArgs.updateGroupSize = false;

    fillWalkerFields<WalkerType>(this->cmdBufferCpuPtr);

    mutableWalker->updateSpecificFields(*neoDevice, walkerArgs);
    auto walkerCmdCpu = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);
    auto iddCpu = walkerCmdCpu->getInterfaceDescriptor();

    EXPECT_EQ(0u, iddCpu.getPreferredSlmAllocationSize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            MutableHwCommandTest,
            givenMutableComputeWalkerWhenSettingImplicitScalingFieldsFromEncodersThenCorrectFieldsAreSet) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto walkerTemplate = FamilyType::template getInitGpuWalker<WalkerType>();
    alignas(8) uint8_t controlWalkerBuffer[sizeof(WalkerType)];
    auto controlWalker = reinterpret_cast<WalkerType *>(controlWalkerBuffer);
    *controlWalker = walkerTemplate;

    this->stageCommit = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);

    auto walkerArgs = createMutableWalkerSpecificFieldsArguments();
    walkerArgs.updateGroupCount = true;
    walkerArgs.partitionCount = 2;
    neoDevice->deviceBitfield.reset();
    neoDevice->deviceBitfield.set(0b11);

    fillWalkerFields<WalkerType>(controlWalkerBuffer);

    NEO::ImplicitScalingDispatchCommandArgs implicitScalingArgs{
        0x1000,                          // workPartitionAllocationGpuVa
        neoDevice,                       // device
        nullptr,                         // outWalkerPtr
        walkerArgs.requiredPartitionDim, // requiredPartitionDim
        walkerArgs.partitionCount,       // partitionCount
        walkerArgs.totalWorkGroupSize,   // workgroupSize
        walkerArgs.threadGroupCount,     // threadGroupCount
        walkerArgs.maxWgCountPerTile,    // maxWgCountPerTile
        false,                           // useSecondaryBatchBuffer
        true,                            // apiSelfCleanup
        false,                           // dcFlush
        false,                           // forceExecutionOnSingleTile
        true,                            // blockDispatchToCommandBuffer
        false};                          // isRequiredDispatchWorkGroupOrder

    NEO::ImplicitScalingDispatch<FamilyType>::dispatchCommands(this->commandStream,
                                                               *controlWalker,
                                                               neoDevice->getDeviceBitfield(),
                                                               implicitScalingArgs);

    fillWalkerFields<WalkerType>(this->cmdBufferCpuPtr);

    mutableWalker->updateSpecificFields(*neoDevice, walkerArgs);
    auto walkerCmdCpu = reinterpret_cast<WalkerType *>(this->cmdBufferCpuPtr);

    EXPECT_EQ(controlWalker->getPartitionType(), walkerCmdCpu->getPartitionType());
    EXPECT_EQ(controlWalker->getPartitionSize(), walkerCmdCpu->getPartitionSize());

    this->stageCommit = false;
    walkerArgs.updateGroupCount = false;
    walkerArgs.updateGroupSize = true;
    createDefaultMutableWalker<FamilyType, WalkerType>(&walkerTemplate, true, true);
    fillWalkerFields<WalkerType>(this->cmdBufferCpuPtr);

    mutableWalker->updateSpecificFields(*neoDevice, walkerArgs);
    auto walkerCmdGpu = reinterpret_cast<WalkerType *>(this->cmdBufferGpuPtr);

    EXPECT_EQ(controlWalker->getPartitionType(), walkerCmdGpu->getPartitionType());
    EXPECT_EQ(controlWalker->getPartitionSize(), walkerCmdGpu->getPartitionSize());
}
} // namespace ult
} // namespace L0
