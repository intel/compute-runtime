/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/sources/mutable_cmdlist/fixtures/variable_fixture.h"

namespace L0 {
namespace ult {

using VariableTest = Test<VariableFixture>;
using VariableInOrderTest = Test<VariableInOrderFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenGroupCountVariableWhenSettingGroupCountValueThenValueIsRetained) {
    uint32_t groupCountValues[3] = {4, 2, 1};
    const void *argValue = &groupCountValues;

    createVariable(L0::MCL::VariableType::groupCount, true, -1, -1);

    auto ret = this->variable->setValue(kernelDispatchVariableSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_EQ(groupCountValues[0], this->variable->kernelDispatch.groupCount[0]);
    EXPECT_EQ(groupCountValues[1], this->variable->kernelDispatch.groupCount[1]);
    EXPECT_EQ(groupCountValues[2], this->variable->kernelDispatch.groupCount[2]);

    EXPECT_TRUE(this->variable->desc.commitRequired);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenGroupCountVariableDispatchWhenSettingGroupCountThenGroupCountIsPatched) {
    using WalkerType = typename FamilyType::PorWalkerType;
    createMutableComputeWalker<FamilyType, WalkerType>(0);
    createVariableDispatch(true, false, false, false);

    uint32_t groupCountValues[3] = {4, 2, 1};
    const void *argValue = &groupCountValues;

    Variable *groupCount = getVariable(L0::MCL::VariableType::groupCount);

    auto ret = groupCount->setValue(kernelDispatchVariableSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_TRUE(groupCount->desc.commitRequired);
    EXPECT_TRUE(this->variableDispatch->doCommitVariableDispatch());
    auto groupCountIt = std::find(mutableCommandList->stageCommitVariables.begin(), mutableCommandList->stageCommitVariables.end(), groupCount);
    EXPECT_NE(groupCountIt, mutableCommandList->stageCommitVariables.end());

    groupCount->commitVariable();

    WalkerType *walkerCmdCpuBuffer = reinterpret_cast<WalkerType *>(this->cpuWalkerBuffer);
    WalkerType *walkerCmdGpuBuffer = reinterpret_cast<WalkerType *>(this->walkerBuffer);

    EXPECT_EQ(groupCountValues[0], walkerCmdCpuBuffer->getThreadGroupIdXDimension());
    EXPECT_EQ(groupCountValues[1], walkerCmdCpuBuffer->getThreadGroupIdYDimension());
    EXPECT_EQ(groupCountValues[2], walkerCmdCpuBuffer->getThreadGroupIdZDimension());

    EXPECT_EQ(groupCountValues[0], walkerCmdGpuBuffer->getThreadGroupIdXDimension());
    EXPECT_EQ(groupCountValues[1], walkerCmdGpuBuffer->getThreadGroupIdYDimension());
    EXPECT_EQ(groupCountValues[2], walkerCmdGpuBuffer->getThreadGroupIdZDimension());

    uint32_t *numWorkGroupsPatch = reinterpret_cast<uint32_t *>(ptrOffset(this->crossThreadData.get(), this->offsets.numWorkGroups));
    EXPECT_EQ(groupCountValues[0], numWorkGroupsPatch[0]);
    EXPECT_EQ(groupCountValues[1], numWorkGroupsPatch[1]);
    EXPECT_EQ(groupCountValues[2], numWorkGroupsPatch[2]);

    uint32_t *globalWorkSizePatch = reinterpret_cast<uint32_t *>(ptrOffset(this->crossThreadData.get(), this->offsets.globalWorkSize));
    EXPECT_EQ(groupCountValues[0], globalWorkSizePatch[0]);
    EXPECT_EQ(groupCountValues[1], globalWorkSizePatch[1]);
    EXPECT_EQ(groupCountValues[2], globalWorkSizePatch[2]);

    uint32_t *workDimPatch = reinterpret_cast<uint32_t *>(ptrOffset(this->crossThreadData.get(), this->offsets.workDimensions));
    EXPECT_EQ(2u, *workDimPatch);

    EXPECT_FALSE(groupCount->desc.commitRequired);
    EXPECT_FALSE(this->variableDispatch->doCommitVariableDispatch());

    groupCountValues[1] = 4;
    ret = groupCount->setValue(kernelDispatchVariableSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_TRUE(groupCount->desc.commitRequired);
    EXPECT_TRUE(this->variableDispatch->doCommitVariableDispatch());

    groupCount->commitVariable();
    EXPECT_EQ(groupCountValues[1], walkerCmdCpuBuffer->getThreadGroupIdYDimension());
    EXPECT_EQ(groupCountValues[1], numWorkGroupsPatch[1]);
    EXPECT_EQ(groupCountValues[1], globalWorkSizePatch[1]);

    groupCountValues[2] = 4;
    ret = groupCount->setValue(kernelDispatchVariableSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_TRUE(groupCount->desc.commitRequired);
    EXPECT_TRUE(this->variableDispatch->doCommitVariableDispatch());

    groupCount->commitVariable();
    EXPECT_EQ(groupCountValues[2], walkerCmdCpuBuffer->getThreadGroupIdZDimension());
    EXPECT_EQ(groupCountValues[2], numWorkGroupsPatch[2]);
    EXPECT_EQ(groupCountValues[2], globalWorkSizePatch[2]);

    ret = groupCount->setValue(kernelDispatchVariableSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_FALSE(groupCount->desc.commitRequired);
    EXPECT_FALSE(this->variableDispatch->doCommitVariableDispatch());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenGroupCountVariableNonCommitDispatchWhenSettingGroupCountThenGroupCountIsPatchedImmediately) {
    using WalkerType = typename FamilyType::PorWalkerType;
    this->stageCommitMode = false;
    createMutableComputeWalker<FamilyType, WalkerType>(0);
    createVariableDispatch(true, false, false, false);

    uint32_t groupCountValues[3] = {4, 2, 1};
    const void *argValue = &groupCountValues;

    Variable *groupCount = getVariable(L0::MCL::VariableType::groupCount);

    auto ret = groupCount->setValue(kernelDispatchVariableSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_FALSE(groupCount->desc.commitRequired);

    WalkerType *walkerCmdCpuBuffer = reinterpret_cast<WalkerType *>(this->cpuWalkerBuffer);
    WalkerType *walkerCmdGpuBuffer = reinterpret_cast<WalkerType *>(this->walkerBuffer);

    EXPECT_EQ(groupCountValues[0], walkerCmdCpuBuffer->getThreadGroupIdXDimension());
    EXPECT_EQ(groupCountValues[1], walkerCmdCpuBuffer->getThreadGroupIdYDimension());
    EXPECT_EQ(groupCountValues[2], walkerCmdCpuBuffer->getThreadGroupIdZDimension());

    EXPECT_EQ(groupCountValues[0], walkerCmdGpuBuffer->getThreadGroupIdXDimension());
    EXPECT_EQ(groupCountValues[1], walkerCmdGpuBuffer->getThreadGroupIdYDimension());
    EXPECT_EQ(groupCountValues[2], walkerCmdGpuBuffer->getThreadGroupIdZDimension());

    uint32_t *numWorkGroupsPatch = reinterpret_cast<uint32_t *>(ptrOffset(this->crossThreadData.get(), this->offsets.numWorkGroups));
    EXPECT_EQ(groupCountValues[0], numWorkGroupsPatch[0]);
    EXPECT_EQ(groupCountValues[1], numWorkGroupsPatch[1]);
    EXPECT_EQ(groupCountValues[2], numWorkGroupsPatch[2]);

    uint32_t *globalWorkSizePatch = reinterpret_cast<uint32_t *>(ptrOffset(this->crossThreadData.get(), this->offsets.globalWorkSize));
    EXPECT_EQ(groupCountValues[0], globalWorkSizePatch[0]);
    EXPECT_EQ(groupCountValues[1], globalWorkSizePatch[1]);
    EXPECT_EQ(groupCountValues[2], globalWorkSizePatch[2]);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenGroupSizeVariableDispatchWhenSettingGroupSizeThenGroupSizeIsPatched) {
    using WalkerType = typename FamilyType::PorWalkerType;
    createMutableComputeWalker<FamilyType, WalkerType>(0);
    createVariableDispatch(false, true, false, false);
    this->kernelDispatch->kernelData->numLocalIdChannels = 3;

    uint32_t groupSizeValues[3] = {4, 2, 1};
    const void *argValue = &groupSizeValues;

    Variable *groupSize = getVariable(L0::MCL::VariableType::groupSize);

    auto ret = groupSize->setValue(kernelDispatchVariableSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_TRUE(groupSize->desc.commitRequired);
    EXPECT_TRUE(this->variableDispatch->doCommitVariableDispatch());
    auto groupSizeIt = std::find(mutableCommandList->stageCommitVariables.begin(), mutableCommandList->stageCommitVariables.end(), groupSize);
    EXPECT_NE(groupSizeIt, mutableCommandList->stageCommitVariables.end());

    groupSize->commitVariable();

    WalkerType *walkerCmdCpuBuffer = reinterpret_cast<WalkerType *>(this->cpuWalkerBuffer);
    WalkerType *walkerCmdGpuBuffer = reinterpret_cast<WalkerType *>(this->walkerBuffer);

    EXPECT_EQ(groupSizeValues[0] - 1, walkerCmdCpuBuffer->getLocalXMaximum());
    EXPECT_EQ(groupSizeValues[1] - 1, walkerCmdCpuBuffer->getLocalYMaximum());
    EXPECT_EQ(groupSizeValues[2] - 1, walkerCmdCpuBuffer->getLocalZMaximum());

    EXPECT_EQ(groupSizeValues[0] - 1, walkerCmdGpuBuffer->getLocalXMaximum());
    EXPECT_EQ(groupSizeValues[1] - 1, walkerCmdGpuBuffer->getLocalYMaximum());
    EXPECT_EQ(groupSizeValues[2] - 1, walkerCmdGpuBuffer->getLocalZMaximum());

    uint32_t *localWorkSizePatch = reinterpret_cast<uint32_t *>(ptrOffset(this->crossThreadData.get(), this->offsets.localWorkSize));
    EXPECT_EQ(groupSizeValues[0], localWorkSizePatch[0]);
    EXPECT_EQ(groupSizeValues[1], localWorkSizePatch[1]);
    EXPECT_EQ(groupSizeValues[2], localWorkSizePatch[2]);

    uint32_t *localWorkSize2Patch = reinterpret_cast<uint32_t *>(ptrOffset(this->crossThreadData.get(), this->offsets.localWorkSize2));
    EXPECT_EQ(groupSizeValues[0], localWorkSize2Patch[0]);
    EXPECT_EQ(groupSizeValues[1], localWorkSize2Patch[1]);
    EXPECT_EQ(groupSizeValues[2], localWorkSize2Patch[2]);

    uint32_t *enqLocalWorkSizePatch = reinterpret_cast<uint32_t *>(ptrOffset(this->crossThreadData.get(), this->offsets.enqLocalWorkSize));
    EXPECT_EQ(groupSizeValues[0], enqLocalWorkSizePatch[0]);
    EXPECT_EQ(groupSizeValues[1], enqLocalWorkSizePatch[1]);
    EXPECT_EQ(groupSizeValues[2], enqLocalWorkSizePatch[2]);

    EXPECT_FALSE(groupSize->desc.commitRequired);
    EXPECT_FALSE(this->variableDispatch->doCommitVariableDispatch());

    groupSizeValues[1] = 4;
    ret = groupSize->setValue(kernelDispatchVariableSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_TRUE(groupSize->desc.commitRequired);
    EXPECT_TRUE(this->variableDispatch->doCommitVariableDispatch());

    groupSize->commitVariable();
    EXPECT_EQ(groupSizeValues[1] - 1, walkerCmdCpuBuffer->getLocalYMaximum());
    EXPECT_EQ(groupSizeValues[1], localWorkSizePatch[1]);
    EXPECT_EQ(groupSizeValues[1], localWorkSize2Patch[1]);
    EXPECT_EQ(groupSizeValues[1], enqLocalWorkSizePatch[1]);

    groupSizeValues[2] = 4;
    ret = groupSize->setValue(kernelDispatchVariableSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_TRUE(groupSize->desc.commitRequired);
    EXPECT_TRUE(this->variableDispatch->doCommitVariableDispatch());

    groupSize->commitVariable();
    EXPECT_EQ(groupSizeValues[2] - 1, walkerCmdCpuBuffer->getLocalZMaximum());
    EXPECT_EQ(groupSizeValues[2], localWorkSizePatch[2]);
    EXPECT_EQ(groupSizeValues[2], localWorkSize2Patch[2]);
    EXPECT_EQ(groupSizeValues[2], enqLocalWorkSizePatch[2]);

    ret = groupSize->setValue(kernelDispatchVariableSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_FALSE(groupSize->desc.commitRequired);
    EXPECT_FALSE(this->variableDispatch->doCommitVariableDispatch());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenGroupSizeVariableNonCommitDispatchWhenSettingGroupSizeThenGroupSizeIsPatchedImmediately) {
    using WalkerType = typename FamilyType::PorWalkerType;
    this->stageCommitMode = false;
    createMutableComputeWalker<FamilyType, WalkerType>(0);
    createVariableDispatch(false, true, false, false);
    this->kernelDispatch->kernelData->numLocalIdChannels = 3;

    uint32_t groupSizeValues[3] = {4, 2, 1};
    const void *argValue = &groupSizeValues;

    Variable *groupSize = getVariable(L0::MCL::VariableType::groupSize);

    auto ret = groupSize->setValue(kernelDispatchVariableSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_FALSE(groupSize->desc.commitRequired);

    WalkerType *walkerCmdCpuBuffer = reinterpret_cast<WalkerType *>(this->cpuWalkerBuffer);
    WalkerType *walkerCmdGpuBuffer = reinterpret_cast<WalkerType *>(this->walkerBuffer);

    EXPECT_EQ(groupSizeValues[0] - 1, walkerCmdCpuBuffer->getLocalXMaximum());
    EXPECT_EQ(groupSizeValues[1] - 1, walkerCmdCpuBuffer->getLocalYMaximum());
    EXPECT_EQ(groupSizeValues[2] - 1, walkerCmdCpuBuffer->getLocalZMaximum());

    EXPECT_EQ(groupSizeValues[0] - 1, walkerCmdGpuBuffer->getLocalXMaximum());
    EXPECT_EQ(groupSizeValues[1] - 1, walkerCmdGpuBuffer->getLocalYMaximum());
    EXPECT_EQ(groupSizeValues[2] - 1, walkerCmdGpuBuffer->getLocalZMaximum());

    uint32_t *localWorkSizePatch = reinterpret_cast<uint32_t *>(ptrOffset(this->crossThreadData.get(), this->offsets.localWorkSize));
    EXPECT_EQ(groupSizeValues[0], localWorkSizePatch[0]);
    EXPECT_EQ(groupSizeValues[1], localWorkSizePatch[1]);
    EXPECT_EQ(groupSizeValues[2], localWorkSizePatch[2]);

    uint32_t *localWorkSize2Patch = reinterpret_cast<uint32_t *>(ptrOffset(this->crossThreadData.get(), this->offsets.localWorkSize2));
    EXPECT_EQ(groupSizeValues[0], localWorkSize2Patch[0]);
    EXPECT_EQ(groupSizeValues[1], localWorkSize2Patch[1]);
    EXPECT_EQ(groupSizeValues[2], localWorkSize2Patch[2]);

    uint32_t *enqLocalWorkSizePatch = reinterpret_cast<uint32_t *>(ptrOffset(this->crossThreadData.get(), this->offsets.enqLocalWorkSize));
    EXPECT_EQ(groupSizeValues[0], enqLocalWorkSizePatch[0]);
    EXPECT_EQ(groupSizeValues[1], enqLocalWorkSizePatch[1]);
    EXPECT_EQ(groupSizeValues[2], enqLocalWorkSizePatch[2]);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenKernelChannelsZeroAndGroupSizeVariableDispatchWhenSettingGroupSizeThenGenerateLocalIdsDisabledAndKernelStartOffsetAdded) {
    using WalkerType = typename FamilyType::PorWalkerType;
    createMutableComputeWalker<FamilyType, WalkerType>(0);
    createVariableDispatch(false, true, false, false);
    this->kernelDispatch->kernelData->numLocalIdChannels = 0;
    this->kernelDispatch->kernelData->kernelStartAddress = 0x4000;
    this->kernelDispatch->kernelData->skipPerThreadDataLoad = 0x100;
    uint64_t kernelStartAddress = this->kernelDispatch->kernelData->kernelStartAddress + this->kernelDispatch->kernelData->skipPerThreadDataLoad;

    uint32_t groupSizeValues[3] = {4, 2, 1};
    const void *argValue = &groupSizeValues;

    Variable *groupSize = getVariable(L0::MCL::VariableType::groupSize);

    auto ret = groupSize->setValue(kernelDispatchVariableSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    groupSize->commitVariable();

    WalkerType *walkerCmdCpuBuffer = reinterpret_cast<WalkerType *>(this->cpuWalkerBuffer);
    WalkerType *walkerCmdGpuBuffer = reinterpret_cast<WalkerType *>(this->walkerBuffer);

    EXPECT_EQ(0u, walkerCmdCpuBuffer->getLocalXMaximum());
    EXPECT_EQ(0u, walkerCmdCpuBuffer->getLocalYMaximum());
    EXPECT_EQ(0u, walkerCmdCpuBuffer->getLocalZMaximum());

    EXPECT_EQ(0u, walkerCmdCpuBuffer->getEmitLocalId());
    EXPECT_EQ(false, walkerCmdCpuBuffer->getGenerateLocalId());
    EXPECT_EQ(kernelStartAddress, walkerCmdCpuBuffer->getInterfaceDescriptor().getKernelStartPointer());

    EXPECT_EQ(0u, walkerCmdGpuBuffer->getLocalXMaximum());
    EXPECT_EQ(0u, walkerCmdGpuBuffer->getLocalYMaximum());
    EXPECT_EQ(0u, walkerCmdGpuBuffer->getLocalZMaximum());

    EXPECT_EQ(0u, walkerCmdGpuBuffer->getEmitLocalId());
    EXPECT_EQ(false, walkerCmdGpuBuffer->getGenerateLocalId());
    EXPECT_EQ(kernelStartAddress, walkerCmdCpuBuffer->getInterfaceDescriptor().getKernelStartPointer());

    EXPECT_FALSE(variableDispatch->localIdGenerationByRuntime);
    EXPECT_EQ(0u, variableDispatch->perThreadDataSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenSwLocalIdGenerationForcedAndGroupSizeVariableDispatchWhenSettingGroupSizeThenGenerateLocalIdsDisabledAndKernelStartOffsetNotAdded) {
    using WalkerType = typename FamilyType::PorWalkerType;

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableHwGenerationLocalIds.set(false);

    this->usePerThread = true;
    createMutableComputeWalker<FamilyType, WalkerType>(0);
    createVariableDispatch(false, true, false, false);
    this->kernelDispatch->kernelData->numLocalIdChannels = 3;
    this->kernelDispatch->kernelData->kernelStartAddress = 0x4000;
    this->kernelDispatch->kernelData->skipPerThreadDataLoad = 0x100;
    uint64_t kernelStartAddress = this->kernelDispatch->kernelData->kernelStartAddress;

    uint32_t groupSizeValues[3] = {4, 2, 1};
    const void *argValue = &groupSizeValues;

    Variable *groupSize = getVariable(L0::MCL::VariableType::groupSize);

    auto ret = groupSize->setValue(kernelDispatchVariableSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    groupSize->commitVariable();

    WalkerType *walkerCmdCpuBuffer = reinterpret_cast<WalkerType *>(this->cpuWalkerBuffer);
    WalkerType *walkerCmdGpuBuffer = reinterpret_cast<WalkerType *>(this->walkerBuffer);

    EXPECT_EQ(0u, walkerCmdCpuBuffer->getLocalXMaximum());
    EXPECT_EQ(0u, walkerCmdCpuBuffer->getLocalYMaximum());
    EXPECT_EQ(0u, walkerCmdCpuBuffer->getLocalZMaximum());

    EXPECT_EQ(0u, walkerCmdCpuBuffer->getEmitLocalId());
    EXPECT_EQ(false, walkerCmdCpuBuffer->getGenerateLocalId());
    EXPECT_EQ(kernelStartAddress, walkerCmdCpuBuffer->getInterfaceDescriptor().getKernelStartPointer());

    EXPECT_EQ(0u, walkerCmdGpuBuffer->getLocalXMaximum());
    EXPECT_EQ(0u, walkerCmdGpuBuffer->getLocalYMaximum());
    EXPECT_EQ(0u, walkerCmdGpuBuffer->getLocalZMaximum());

    EXPECT_EQ(0u, walkerCmdGpuBuffer->getEmitLocalId());
    EXPECT_EQ(false, walkerCmdGpuBuffer->getGenerateLocalId());

    EXPECT_EQ(kernelStartAddress, walkerCmdCpuBuffer->getInterfaceDescriptor().getKernelStartPointer());

    EXPECT_TRUE(variableDispatch->localIdGenerationByRuntime);
    EXPECT_NE(0u, variableDispatch->perThreadDataSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenGlobalOffsetVariableDispatchWhenSettingGlobalOffsetThenGlobalOffsetIsPatched) {
    using WalkerType = typename FamilyType::PorWalkerType;
    createMutableComputeWalker<FamilyType, WalkerType>(0);
    createVariableDispatch(false, false, true, false);

    uint32_t globalOffsetValues[3] = {4, 2, 1};
    const void *argValue = &globalOffsetValues;

    Variable *globalOffset = getVariable(L0::MCL::VariableType::globalOffset);

    auto ret = globalOffset->setValue(kernelDispatchVariableSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_FALSE(globalOffset->desc.commitRequired);
    EXPECT_FALSE(this->variableDispatch->doCommitVariableDispatch());
    auto globalOffsetIt = std::find(mutableCommandList->stageCommitVariables.begin(), mutableCommandList->stageCommitVariables.end(), globalOffset);
    EXPECT_EQ(globalOffsetIt, mutableCommandList->stageCommitVariables.end());

    uint32_t *globalOffsetPatch = reinterpret_cast<uint32_t *>(ptrOffset(this->crossThreadData.get(), this->offsets.globalWorkOffset));
    EXPECT_EQ(globalOffsetValues[0], globalOffsetPatch[0]);
    EXPECT_EQ(globalOffsetValues[1], globalOffsetPatch[1]);
    EXPECT_EQ(globalOffsetValues[2], globalOffsetPatch[2]);

    EXPECT_FALSE(globalOffset->desc.commitRequired);
    EXPECT_FALSE(this->variableDispatch->doCommitVariableDispatch());

    globalOffsetValues[1] = 4;
    ret = globalOffset->setValue(kernelDispatchVariableSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(globalOffsetValues[1], globalOffsetPatch[1]);

    globalOffsetValues[2] = 4;
    ret = globalOffset->setValue(kernelDispatchVariableSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(globalOffsetValues[2], globalOffsetPatch[2]);

    ret = globalOffset->setValue(kernelDispatchVariableSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_FALSE(globalOffset->desc.commitRequired);
    EXPECT_FALSE(this->variableDispatch->doCommitVariableDispatch());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenBufferVariableAndInlineEnabledWhenBufferPatchOffsetInsideInlineThenUseInline) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto svmPtr = allocateUsm(4096);
    uint64_t svmGpuAddress = reinterpret_cast<uint64_t>(svmPtr);

    createMutableComputeWalker<FamilyType, WalkerType>(0);

    auto &arg = this->kernelArgPtr.as<NEO::ArgDescPointer>();
    auto qwordAlignment = alignUp(this->mutableComputeWalker->getInlineDataOffset(), sizeof(uint64_t));
    CrossThreadDataOffset offsetWithinInline = 16 + static_cast<CrossThreadDataOffset>(qwordAlignment - this->mutableComputeWalker->getInlineDataOffset());
    arg.stateless = offsetWithinInline;

    this->inlineData = true;
    createVariable(L0::MCL::VariableType::buffer, true, -1, -1);
    uint64_t *gpuAddressPatch = reinterpret_cast<uint64_t *>(ptrOffset(this->mutableComputeWalker->getInlineDataPointer(), offsetWithinInline));
    *gpuAddressPatch = 0;

    auto ret = this->variable->setValue(kernelArgVariableSize, 0, &svmPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_EQ(svmGpuAddress, *gpuAddressPatch);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenBufferVariableAndInlineEnabledWhenBufferPatchOffsetOutsideInlineThenUseIndirectHeap) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto svmPtr = allocateUsm(4096);
    uint64_t svmGpuAddress = reinterpret_cast<uint64_t>(svmPtr);

    createMutableComputeWalker<FamilyType, WalkerType>(0);

    auto &arg = this->kernelArgPtr.as<NEO::ArgDescPointer>();
    CrossThreadDataOffset offsetOutsideInline = static_cast<CrossThreadDataOffset>(mutableComputeWalker->getInlineDataSize() * 2);
    arg.stateless = offsetOutsideInline;

    CrossThreadDataOffset actualIndirectHeapOffset = offsetOutsideInline - static_cast<CrossThreadDataOffset>(mutableComputeWalker->getInlineDataSize());

    this->inlineData = true;
    createVariable(L0::MCL::VariableType::buffer, true, -1, -1);
    uint64_t *gpuAddressPatch = reinterpret_cast<uint64_t *>(ptrOffset(this->cmdListIndirectCpuBase, actualIndirectHeapOffset));
    *gpuAddressPatch = 0;

    auto ret = this->variable->setValue(kernelArgVariableSize, 0, &svmPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_EQ(svmGpuAddress, *gpuAddressPatch);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenValueVariableAndInlineEnabledWhenMutatingArgumentInsideInlineThenPayloadIsPatchedInInline) {
    using WalkerType = typename FamilyType::PorWalkerType;

    createMutableComputeWalker<FamilyType, WalkerType>(0);

    constexpr size_t arraySize = defaultValueStatelessSize;
    alignas(8) uint8_t immediateValue[arraySize];
    const void *argValue = &immediateValue;

    CrossThreadDataOffset offsetWithinInline = 16;
    auto &argValueDesc = this->kernelArgValue.as<NEO::ArgDescValue>();
    argValueDesc.elements[0].offset = offsetWithinInline;

    this->inlineData = true;
    createVariable(L0::MCL::VariableType::value, true, -1, -1);
    uint8_t *valuesPatch = reinterpret_cast<uint8_t *>(ptrOffset(this->mutableComputeWalker->getInlineDataPointer(), offsetWithinInline));
    // invalidate memory
    memset(valuesPatch, 0, this->defaultValueStatelessSize);

    for (uint32_t i = 0; i < this->defaultValueStatelessSize; i++) {
        immediateValue[i] = static_cast<uint8_t>(i);
    }
    auto ret = this->variable->setValue(this->defaultValueStatelessSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    for (uint32_t i = 0; i < this->defaultValueStatelessSize; i++) {
        EXPECT_EQ(*(valuesPatch + i), immediateValue[i]);
    }

    for (uint32_t i = 0; i < this->defaultValueStatelessSize; i++) {
        immediateValue[i] = static_cast<uint8_t>(this->defaultValueStatelessSize - i);
    }
    ret = this->variable->setValue(this->defaultValueStatelessSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    for (uint32_t i = 0; i < this->defaultValueStatelessSize; i++) {
        EXPECT_EQ(*(valuesPatch + i), immediateValue[i]);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenValueVariableAndInlineEnabledWhenMutatingArgumentOutsudeInlineThenPayloadIsPatchedInIndirectHeap) {
    using WalkerType = typename FamilyType::PorWalkerType;

    createMutableComputeWalker<FamilyType, WalkerType>(0);
    CrossThreadDataOffset offsetOutsideInline = static_cast<CrossThreadDataOffset>(mutableComputeWalker->getInlineDataSize() * 2);

    constexpr size_t arraySize = defaultValueStatelessSize;
    alignas(8) uint8_t immediateValue[arraySize];
    const void *argValue = &immediateValue;

    auto &argValueDesc = this->kernelArgValue.as<NEO::ArgDescValue>();
    argValueDesc.elements[0].offset = offsetOutsideInline;

    CrossThreadDataOffset actualIndirectHeapOffset = offsetOutsideInline - static_cast<CrossThreadDataOffset>(mutableComputeWalker->getInlineDataSize());

    this->inlineData = true;
    createVariable(L0::MCL::VariableType::value, true, -1, -1);
    uint8_t *valuesPatch = reinterpret_cast<uint8_t *>(ptrOffset(this->cmdListIndirectCpuBase, actualIndirectHeapOffset));
    // invalidate memory
    memset(valuesPatch, 0, this->defaultValueStatelessSize);

    for (uint32_t i = 0; i < this->defaultValueStatelessSize; i++) {
        immediateValue[i] = static_cast<uint8_t>(i);
    }
    auto ret = this->variable->setValue(this->defaultValueStatelessSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    for (uint32_t i = 0; i < this->defaultValueStatelessSize; i++) {
        EXPECT_EQ(*(valuesPatch + i), immediateValue[i]);
    }

    for (uint32_t i = 0; i < this->defaultValueStatelessSize; i++) {
        immediateValue[i] = static_cast<uint8_t>(this->defaultValueStatelessSize - i);
    }
    ret = this->variable->setValue(this->defaultValueStatelessSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    for (uint32_t i = 0; i < this->defaultValueStatelessSize; i++) {
        EXPECT_EQ(*(valuesPatch + i), immediateValue[i]);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenValueVariableAndInlineEnabledWhenMutatingArgumentSplitBetweenInlineAndHeapThenPayloadIsPatchedInBoth) {
    using WalkerType = typename FamilyType::PorWalkerType;

    size_t splitOffset = this->defaultValueStatelessSize / 2;

    createMutableComputeWalker<FamilyType, WalkerType>(0);
    CrossThreadDataOffset baseInlineOffset = static_cast<CrossThreadDataOffset>(mutableComputeWalker->getInlineDataSize() - splitOffset);

    constexpr size_t arraySize = defaultValueStatelessSize;
    alignas(8) uint8_t immediateValue[arraySize];
    const void *argValue = &immediateValue;

    auto &argValueDesc = this->kernelArgValue.as<NEO::ArgDescValue>();
    argValueDesc.elements[0].offset = baseInlineOffset;

    CrossThreadDataOffset actualIndirectHeapOffset = 0; // it starts from the beginning of the indirect heap

    this->inlineData = true;
    createVariable(L0::MCL::VariableType::value, true, -1, -1);
    uint8_t *valuesPatchInline = reinterpret_cast<uint8_t *>(ptrOffset(this->mutableComputeWalker->getInlineDataPointer(), baseInlineOffset));
    uint8_t *valuesPatchHeap = reinterpret_cast<uint8_t *>(ptrOffset(this->cmdListIndirectCpuBase, actualIndirectHeapOffset));

    // invalidate memory
    memset(valuesPatchInline, 0, splitOffset);
    memset(valuesPatchHeap, 0, splitOffset);

    for (uint32_t i = 0; i < this->defaultValueStatelessSize; i++) {
        immediateValue[i] = static_cast<uint8_t>(i);
    }
    auto ret = this->variable->setValue(this->defaultValueStatelessSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    for (uint32_t i = 0; i < splitOffset; i++) {
        EXPECT_EQ(*(valuesPatchInline + i), immediateValue[i]);
        EXPECT_EQ(*(valuesPatchHeap + i), immediateValue[i + splitOffset]);
    }

    for (uint32_t i = 0; i < this->defaultValueStatelessSize; i++) {
        immediateValue[i] = static_cast<uint8_t>(this->defaultValueStatelessSize - i);
    }
    ret = this->variable->setValue(this->defaultValueStatelessSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    for (uint32_t i = 0; i < splitOffset; i++) {
        EXPECT_EQ(*(valuesPatchInline + i), immediateValue[i]);
        EXPECT_EQ(*(valuesPatchHeap + i), immediateValue[i + splitOffset]);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenSingleSlmVariableWhenMutatingSlmArgumentThenSlmSizeChanged) {
    using WalkerType = typename FamilyType::PorWalkerType;

    createMutableComputeWalker<FamilyType, WalkerType>(0);

    uint32_t slmSize = 1 * MemoryConstants::kiloByte;

    createVariableDispatch(false, false, false, true);
    Variable *slmArgument = getVariable(L0::MCL::VariableType::slmBuffer);
    EXPECT_EQ(nullptr, slmArgument->slmValue.nextSlmVariable);
    EXPECT_EQ(0u, slmArgument->slmValue.slmOffsetValue);

    auto ret = slmArgument->setValue(slmSize, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_TRUE(slmArgument->desc.commitRequired);
    EXPECT_EQ(slmSize, slmArgument->slmValue.slmSize);

    slmArgument->commitVariable();
    EXPECT_EQ(slmSize, this->kernelDispatch->slmTotalSize);
    EXPECT_FALSE(slmArgument->desc.commitRequired);

    ret = slmArgument->setValue(slmSize, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_FALSE(slmArgument->desc.commitRequired);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenSingleSlmVariableInNonCommitModeWhenMutatingSlmArgumentThenSlmSizeChangedImmediately) {
    using WalkerType = typename FamilyType::PorWalkerType;

    createMutableComputeWalker<FamilyType, WalkerType>(0);

    uint32_t slmSize = 1 * MemoryConstants::kiloByte;

    this->stageCommitMode = false;
    createVariableDispatch(false, false, false, true);
    Variable *slmArgument = getVariable(L0::MCL::VariableType::slmBuffer);
    EXPECT_EQ(nullptr, slmArgument->slmValue.nextSlmVariable);
    EXPECT_EQ(0u, slmArgument->slmValue.slmOffsetValue);

    auto ret = slmArgument->setValue(slmSize, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_FALSE(slmArgument->desc.commitRequired);
    EXPECT_EQ(slmSize, slmArgument->slmValue.slmSize);
    EXPECT_EQ(slmSize, this->kernelDispatch->slmTotalSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenSingleSlmVariableWhenMutatingMisalignedSlmValueThenSlmSizeChangedWithAlignedValues) {
    using WalkerType = typename FamilyType::PorWalkerType;

    createMutableComputeWalker<FamilyType, WalkerType>(0);

    uint32_t slmSize = 1;
    uint32_t expectedSlmSize = alignUp(slmSize, totalSlmAlignment);

    createVariableDispatch(false, false, false, true);
    Variable *slmArgument = getVariable(L0::MCL::VariableType::slmBuffer);
    EXPECT_EQ(nullptr, slmArgument->slmValue.nextSlmVariable);
    EXPECT_EQ(0u, slmArgument->slmValue.slmOffsetValue);

    auto ret = slmArgument->setValue(slmSize, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_TRUE(slmArgument->desc.commitRequired);
    EXPECT_EQ(slmSize, slmArgument->slmValue.slmSize);

    slmArgument->commitVariable();
    EXPECT_EQ(expectedSlmSize, this->kernelDispatch->slmTotalSize);
    EXPECT_FALSE(slmArgument->desc.commitRequired);

    slmSize = 2;
    expectedSlmSize = alignUp(slmSize, totalSlmAlignment);
    ret = slmArgument->setValue(slmSize, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(slmSize, slmArgument->slmValue.slmSize);

    slmArgument->commitVariable();
    EXPECT_EQ(expectedSlmSize, this->kernelDispatch->slmTotalSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenSingleSlmArgumentAndInlineSlmWhenMutatingSlmArgumentThenSlmSizeChangedAndInlineAdded) {
    using WalkerType = typename FamilyType::PorWalkerType;

    createMutableComputeWalker<FamilyType, WalkerType>(0);

    uint32_t slmSize = 1 * MemoryConstants::kiloByte;
    this->slmInlineSize = 1 * MemoryConstants::kiloByte;

    createVariableDispatch(false, false, false, true);
    Variable *slmArgument = getVariable(L0::MCL::VariableType::slmBuffer);

    auto ret = slmArgument->setValue(slmSize, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    slmArgument->commitVariable();
    EXPECT_EQ(slmSize + this->slmInlineSize, this->kernelDispatch->slmTotalSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenTwoSlmArgumentsWhenMutatingSlmArgumentsStartingFirstThenTotalSlmSizeChanged) {
    using WalkerType = typename FamilyType::PorWalkerType;

    createMutableComputeWalker<FamilyType, WalkerType>(0);

    uint32_t slmFirstSize = 1 * MemoryConstants::kiloByte;
    uint32_t slmSecondSize = 2 * MemoryConstants::kiloByte;

    createVariableDispatch(false, false, false, true);
    Variable *slmLastArgument = getVariable(L0::MCL::VariableType::slmBuffer);
    createVariable(L0::MCL::VariableType::slmBuffer, true, -1, -1);
    Variable *slmFirstArgument = this->variable.get();

    slmFirstArgument->slmValue.nextSlmVariable = slmLastArgument;

    uint32_t *slmOffsetPatch = reinterpret_cast<uint32_t *>(ptrOffset(this->cmdListIndirectCpuBase, this->defaultSlmSecondOffset));
    memset(slmOffsetPatch, 0, sizeof(uint32_t));

    auto ret = slmFirstArgument->setValue(slmFirstSize, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(slmFirstSize, slmFirstArgument->slmValue.slmSize);
    EXPECT_EQ(slmFirstSize, slmLastArgument->slmValue.slmOffsetValue);
    EXPECT_EQ(slmFirstSize, *slmOffsetPatch);

    EXPECT_FALSE(slmFirstArgument->desc.commitRequired);
    EXPECT_TRUE(slmLastArgument->desc.commitRequired);

    slmLastArgument->commitVariable();
    EXPECT_EQ(slmFirstSize, this->kernelDispatch->slmTotalSize);

    ret = slmLastArgument->setValue(slmSecondSize, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(slmSecondSize, slmLastArgument->slmValue.slmSize);
    EXPECT_EQ(slmFirstSize, slmLastArgument->slmValue.slmOffsetValue);
    EXPECT_EQ(slmFirstSize, *slmOffsetPatch);

    EXPECT_FALSE(slmFirstArgument->desc.commitRequired);
    EXPECT_TRUE(slmLastArgument->desc.commitRequired);

    slmLastArgument->commitVariable();
    EXPECT_EQ(slmFirstSize + slmSecondSize, this->kernelDispatch->slmTotalSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenTwoSlmArgumentsWhenMutatingSlmArgumentsStartingSecondThenTotalSlmSizeChanged) {
    using WalkerType = typename FamilyType::PorWalkerType;

    createMutableComputeWalker<FamilyType, WalkerType>(0);

    uint32_t slmFirstSize = 1 * MemoryConstants::kiloByte;
    uint32_t slmSecondSize = 2 * MemoryConstants::kiloByte;

    createVariableDispatch(false, false, false, true);
    Variable *slmLastArgument = getVariable(L0::MCL::VariableType::slmBuffer);
    createVariable(L0::MCL::VariableType::slmBuffer, true, -1, -1);
    Variable *slmFirstArgument = this->variable.get();

    slmFirstArgument->slmValue.nextSlmVariable = slmLastArgument;

    uint32_t *slmOffsetPatch = reinterpret_cast<uint32_t *>(ptrOffset(this->cmdListIndirectCpuBase, this->defaultSlmSecondOffset));
    memset(slmOffsetPatch, 0, sizeof(uint32_t));

    auto ret = slmLastArgument->setValue(slmSecondSize, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(0u, slmFirstArgument->slmValue.slmSize);
    EXPECT_EQ(slmSecondSize, slmLastArgument->slmValue.slmSize);
    EXPECT_EQ(0u, slmLastArgument->slmValue.slmOffsetValue);
    EXPECT_EQ(0u, *slmOffsetPatch);

    EXPECT_FALSE(slmFirstArgument->desc.commitRequired);
    EXPECT_TRUE(slmLastArgument->desc.commitRequired);

    slmLastArgument->commitVariable();
    EXPECT_EQ(slmSecondSize, this->kernelDispatch->slmTotalSize);

    ret = slmFirstArgument->setValue(slmFirstSize, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(slmFirstSize, slmFirstArgument->slmValue.slmSize);
    EXPECT_EQ(slmSecondSize, slmLastArgument->slmValue.slmSize);
    EXPECT_EQ(slmFirstSize, slmLastArgument->slmValue.slmOffsetValue);
    EXPECT_EQ(slmFirstSize, *slmOffsetPatch);

    EXPECT_FALSE(slmFirstArgument->desc.commitRequired);
    EXPECT_TRUE(slmLastArgument->desc.commitRequired);

    slmLastArgument->commitVariable();
    EXPECT_EQ(slmFirstSize + slmSecondSize, this->kernelDispatch->slmTotalSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenTwoSlmArgumentsInlineModeEnabledWhenMutatingSlmArgumentsInsideInlineThenPatchInlineMemory) {
    using WalkerType = typename FamilyType::PorWalkerType;

    createMutableComputeWalker<FamilyType, WalkerType>(0);

    uint32_t slmFirstSize = 1 * MemoryConstants::kiloByte;
    uint32_t slmSecondSize = 2 * MemoryConstants::kiloByte;

    CrossThreadDataOffset slmInlineOffset = 16;

    this->kernelArgSlmSecond.as<NEO::ArgDescPointer>().slmOffset = slmInlineOffset;

    this->inlineData = true;
    createVariableDispatch(false, false, false, true);
    Variable *slmLastArgument = getVariable(L0::MCL::VariableType::slmBuffer);
    createVariable(L0::MCL::VariableType::slmBuffer, true, -1, -1);
    Variable *slmFirstArgument = this->variable.get();

    slmFirstArgument->slmValue.nextSlmVariable = slmLastArgument;

    uint32_t *slmOffsetPatch = reinterpret_cast<uint32_t *>(ptrOffset(this->mutableComputeWalker->getInlineDataPointer(), slmInlineOffset));
    memset(slmOffsetPatch, 0, sizeof(uint32_t));

    auto ret = slmFirstArgument->setValue(slmFirstSize, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(slmFirstSize, slmFirstArgument->slmValue.slmSize);
    EXPECT_EQ(slmFirstSize, slmLastArgument->slmValue.slmOffsetValue);
    EXPECT_EQ(slmFirstSize, *slmOffsetPatch);

    EXPECT_FALSE(slmFirstArgument->desc.commitRequired);
    EXPECT_TRUE(slmLastArgument->desc.commitRequired);

    ret = slmLastArgument->setValue(slmSecondSize, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(slmSecondSize, slmLastArgument->slmValue.slmSize);
    EXPECT_EQ(slmFirstSize, slmLastArgument->slmValue.slmOffsetValue);
    EXPECT_EQ(slmFirstSize, *slmOffsetPatch);

    EXPECT_FALSE(slmFirstArgument->desc.commitRequired);
    EXPECT_TRUE(slmLastArgument->desc.commitRequired);

    slmLastArgument->commitVariable();
    EXPECT_EQ(slmFirstSize + slmSecondSize, this->kernelDispatch->slmTotalSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenTwoSlmArgumentsInlineModeEnabledWhenMutatingSlmArgumentsOutsideInlineThenPatchIndirectMemory) {
    using WalkerType = typename FamilyType::PorWalkerType;

    createMutableComputeWalker<FamilyType, WalkerType>(0);

    uint32_t slmFirstSize = 1 * MemoryConstants::kiloByte;
    uint32_t slmSecondSize = 2 * MemoryConstants::kiloByte;

    CrossThreadDataOffset slmOutsideInlineOffset = static_cast<CrossThreadDataOffset>(this->mutableComputeWalker->getInlineDataSize() + 16);

    this->kernelArgSlmSecond.as<NEO::ArgDescPointer>().slmOffset = slmOutsideInlineOffset;
    CrossThreadDataOffset actualHeapOffset = slmOutsideInlineOffset - static_cast<CrossThreadDataOffset>(this->mutableComputeWalker->getInlineDataSize());

    this->inlineData = true;
    createVariableDispatch(false, false, false, true);
    Variable *slmLastArgument = getVariable(L0::MCL::VariableType::slmBuffer);
    createVariable(L0::MCL::VariableType::slmBuffer, true, -1, -1);
    Variable *slmFirstArgument = this->variable.get();

    slmFirstArgument->slmValue.nextSlmVariable = slmLastArgument;

    uint32_t *slmOffsetPatch = reinterpret_cast<uint32_t *>(ptrOffset(this->cmdListIndirectCpuBase, actualHeapOffset));
    memset(slmOffsetPatch, 0, sizeof(uint32_t));

    auto ret = slmFirstArgument->setValue(slmFirstSize, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(slmFirstSize, slmFirstArgument->slmValue.slmSize);
    EXPECT_EQ(slmFirstSize, slmLastArgument->slmValue.slmOffsetValue);
    EXPECT_EQ(slmFirstSize, *slmOffsetPatch);

    EXPECT_FALSE(slmFirstArgument->desc.commitRequired);
    EXPECT_TRUE(slmLastArgument->desc.commitRequired);

    ret = slmLastArgument->setValue(slmSecondSize, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(slmSecondSize, slmLastArgument->slmValue.slmSize);
    EXPECT_EQ(slmFirstSize, slmLastArgument->slmValue.slmOffsetValue);
    EXPECT_EQ(slmFirstSize, *slmOffsetPatch);

    EXPECT_FALSE(slmFirstArgument->desc.commitRequired);
    EXPECT_TRUE(slmLastArgument->desc.commitRequired);

    slmLastArgument->commitVariable();
    EXPECT_EQ(slmFirstSize + slmSecondSize, this->kernelDispatch->slmTotalSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenBufferVariableWhenMutatingUsmPointerThenPayloadIsPatched) {
    auto svmPtr = allocateUsm(4096);
    uint64_t svmGpuAddress = reinterpret_cast<uint64_t>(svmPtr);

    createVariable(L0::MCL::VariableType::buffer, true, -1, -1);
    uint64_t *gpuAddressPatch = reinterpret_cast<uint64_t *>(ptrOffset(this->cmdListIndirectCpuBase, this->defaultBufferStatelessOffset));
    // invalidate memory
    *gpuAddressPatch = 0;

    auto ret = this->variable->setValue(kernelArgVariableSize, 0, &svmPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_EQ(svmGpuAddress, *gpuAddressPatch);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenBufferVariableWhenMutatingTwoUsmPointersThenPayloadIsPatchedTwice) {
    auto svmPtr = allocateUsm(4096);
    uint64_t svmGpuAddress = reinterpret_cast<uint64_t>(svmPtr);

    auto svmPtr2 = allocateUsm(4096);
    uint64_t svmGpuAddress2 = reinterpret_cast<uint64_t>(svmPtr2);

    createVariable(L0::MCL::VariableType::buffer, true, -1, -1);
    uint64_t *gpuAddressPatch = reinterpret_cast<uint64_t *>(ptrOffset(this->cmdListIndirectCpuBase, this->defaultBufferStatelessOffset));
    // invalidate memory
    *gpuAddressPatch = 0;

    auto ret = this->variable->setValue(kernelArgVariableSize, 0, &svmPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_EQ(svmGpuAddress, *gpuAddressPatch);

    ret = this->variable->setValue(kernelArgVariableSize, 0, &svmPtr2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_EQ(svmGpuAddress2, *gpuAddressPatch);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenValueVariableWhenMutatingArgumentThenPayloadIsPatched) {
    constexpr size_t arraySize = defaultValueStatelessSize;
    alignas(8) uint8_t immediateValue[arraySize];
    const void *argValue = &immediateValue;

    createVariable(L0::MCL::VariableType::value, true, -1, -1);
    uint8_t *valuesPatch = reinterpret_cast<uint8_t *>(ptrOffset(this->cmdListIndirectCpuBase, this->defaultValueStatelessOffset));
    // invalidate memory
    memset(valuesPatch, 0, this->defaultValueStatelessSize);

    for (uint32_t i = 0; i < this->defaultValueStatelessSize; i++) {
        immediateValue[i] = static_cast<uint8_t>(i);
    }
    auto ret = this->variable->setValue(this->defaultValueStatelessSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    for (uint32_t i = 0; i < this->defaultValueStatelessSize; i++) {
        EXPECT_EQ(*(valuesPatch + i), immediateValue[i]);
    }

    for (uint32_t i = 0; i < this->defaultValueStatelessSize; i++) {
        immediateValue[i] = static_cast<uint8_t>(this->defaultValueStatelessSize - i);
    }
    ret = this->variable->setValue(this->defaultValueStatelessSize, 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    for (uint32_t i = 0; i < this->defaultValueStatelessSize; i++) {
        EXPECT_EQ(*(valuesPatch + i), immediateValue[i]);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenChunkedValueVariableWhenMutatingArgumentThenPayloadIsPatched) {
    struct alignas(8) TestStruct {
        bool flag = false;
        uint64_t value = 0;
    };
    TestStruct immediateValue;
    memset(reinterpret_cast<void *>(&immediateValue), 0, sizeof(TestStruct));
    const void *argValue = &immediateValue;

    auto &argValueDesc = this->kernelArgValue.as<NEO::ArgDescValue>();
    argValueDesc.elements[0].size = 1; // size of bool

    NEO::ArgDescValue::Element elementQword{};
    elementQword.offset = this->defaultValueStatelessOffset + 8; // offset within heap
    elementQword.size = 8;                                       // size of element
    elementQword.sourceOffset = 8;                               // offset within structure
    argValueDesc.elements.push_back(elementQword);

    createVariable(L0::MCL::VariableType::value, true, -1, -1);
    void *memoryPatch = ptrOffset(this->cmdListIndirectCpuBase, this->defaultValueStatelessOffset);
    // invalidate memory
    memset(memoryPatch, 0, sizeof(TestStruct));

    immediateValue.flag = true;
    immediateValue.value = 0xFED5;
    auto ret = this->variable->setValue(sizeof(TestStruct), 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(0, memcmp(memoryPatch, &immediateValue, sizeof(TestStruct)));

    immediateValue.flag = false;
    immediateValue.value = 0xFAAC10DDFF;
    ret = this->variable->setValue(sizeof(TestStruct), 0, argValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(0, memcmp(memoryPatch, &immediateValue, sizeof(TestStruct)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenBufferVariableWhenMutatingUsmSharedSystemThenPayloadIsPatched) {
    uint64_t svmGpuAddress = 0x12340000;
    void *svmPtr = reinterpret_cast<void *>(svmGpuAddress);

    createVariable(L0::MCL::VariableType::buffer, true, -1, -1);
    uint64_t *gpuAddressPatch = reinterpret_cast<uint64_t *>(ptrOffset(this->cmdListIndirectCpuBase, this->defaultBufferStatelessOffset));
    // invalidate memory
    *gpuAddressPatch = 0;

    auto ret = this->variable->setValue(kernelArgVariableSize, 0, &svmPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    EXPECT_EQ(svmGpuAddress, *gpuAddressPatch);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenBufferVariableWithDisabledSystemPointerWhenMutatingUsmSharedSystemThenPayloadIsNotPatched) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DisableSystemPointerKernelArgument.set(1);

    uint64_t svmGpuAddress = 0x12320000;
    void *svmPtr = reinterpret_cast<void *>(svmGpuAddress);

    createVariable(L0::MCL::VariableType::buffer, true, -1, -1);
    uint64_t *gpuAddressPatch = reinterpret_cast<uint64_t *>(ptrOffset(this->cmdListIndirectCpuBase, this->defaultBufferStatelessOffset));
    // invalidate memory
    *gpuAddressPatch = 0;

    auto ret = this->variable->setValue(kernelArgVariableSize, 0, &svmPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, ret);

    EXPECT_EQ(0u, *gpuAddressPatch);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenSignalEventRegularNoCounterBasedNoSignalScopeNoTimestampWhenMutatingVariableThenNewPostSyncAddressSet) {
    using WalkerType = typename FamilyType::PorWalkerType;

    auto event = this->createTestEvent(false, false, false, false);
    ASSERT_NE(nullptr, event);
    auto postSyncAddress = event->getGpuAddress(device);
    createMutableComputeWalker<FamilyType, WalkerType>(postSyncAddress);

    createVariable(L0::MCL::VariableType::signalEvent, true, -1, -1);
    auto ret = this->variable->setAsSignalEvent(event, this->mutableComputeWalker.get(), nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, event);

    auto newEvent = this->createTestEvent(false, false, false, false);
    ASSERT_NE(nullptr, newEvent);
    auto newPostSyncAddress = newEvent->getGpuAddress(device);

    ret = this->variable->setValue(0, 0, newEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, newEvent);

    auto walkerCmdCpu = reinterpret_cast<WalkerType *>(this->cpuWalkerBuffer);
    auto testPostSyncAddress = walkerCmdCpu->getPostSync().getDestinationAddress();
    EXPECT_EQ(newPostSyncAddress, testPostSyncAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenSignalEventRegularNoCounterBasedSignalScopeNoTimestampWhenMutatingVariableThenNewPostSyncAddressSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto event = this->createTestEvent(false, true, false, false);
    ASSERT_NE(nullptr, event);
    createMutablePipeControl<FamilyType>();

    createVariable(L0::MCL::VariableType::signalEvent, true, -1, -1);
    auto ret = this->variable->setAsSignalEvent(event, nullptr, this->mutablePipeControl.get());
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, event);

    auto newEvent = this->createTestEvent(false, true, false, false);
    ASSERT_NE(nullptr, newEvent);
    auto newPostSyncAddress = newEvent->getGpuAddress(device);

    ret = this->variable->setValue(0, 0, newEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, newEvent);

    auto pipeControlCmd = reinterpret_cast<PIPE_CONTROL *>(this->pipeControlBuffer);
    auto testPostSyncAddress = NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd);
    EXPECT_EQ(newPostSyncAddress, testPostSyncAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenSignalEventRegularNoCounterBasedSignalScopeTimestampWhenMutatingVariableThenNewPostSyncAddressSet) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    auto event = this->createTestEvent(false, true, true, false);
    ASSERT_NE(nullptr, event);
    size_t offset = 0x10;
    createMutableStoreRegisterMem<FamilyType>(offset);

    createVariable(L0::MCL::VariableType::signalEvent, true, -1, -1);
    auto ret = this->variable->setAsSignalEvent(event, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, event);
    this->variable->getStoreRegMemList().push_back(this->mutableStoreRegisterMem.get());

    auto newEvent = this->createTestEvent(false, true, true, false);
    ASSERT_NE(nullptr, newEvent);
    auto expectedPostSyncAddress = newEvent->getGpuAddress(device) + offset;

    ret = this->variable->setValue(0, 0, newEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, newEvent);

    auto storeRegisterMemCmd = reinterpret_cast<MI_STORE_REGISTER_MEM *>(this->storeRegisterMemBuffer);
    auto testPostSyncAddress = storeRegisterMemCmd->getMemoryAddress();
    EXPECT_EQ(expectedPostSyncAddress, testPostSyncAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenWaitEventRegularNoCounterBasedWhenMutatingVariableThenNewWaitAddressSet) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto event = this->createTestEvent(false, false, false, false);
    ASSERT_NE(nullptr, event);
    size_t offset = 0x10;
    createMutableSemaphoreWait<FamilyType>(offset, 0, L0::MCL::MutableSemaphoreWait::Type::regularEventWait, false);

    createVariable(L0::MCL::VariableType::waitEvent, true, -1, -1);
    auto ret = this->variable->setAsWaitEvent(event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, event);
    this->variable->getSemWaitList().push_back(this->mutableSemaphoreWait.get());
    EXPECT_FALSE(this->variable->eventValue.counterBasedEvent);

    auto newEvent = this->createTestEvent(false, false, false, false);
    ASSERT_NE(nullptr, newEvent);
    auto expectedWaitAddress = newEvent->getGpuAddress(device) + offset;

    ret = this->variable->setValue(0, 0, newEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, newEvent);

    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(this->semaphoreWaitBuffer);
    auto testWaitAddress = semWaitCmd->getSemaphoreGraphicsAddress();
    EXPECT_EQ(expectedWaitAddress, testWaitAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenWaitEventRegularNoCounterBasedWhenNoopingAndRestoringVariableThenWaitCommandIsNoopedAndRestored) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    alignas(sizeof(uint32_t)) uint8_t noopSemaphoreSpace[sizeof(MI_SEMAPHORE_WAIT)] = {};
    memset(noopSemaphoreSpace, 0, sizeof(MI_SEMAPHORE_WAIT));

    auto event = this->createTestEvent(false, false, false, false);
    ASSERT_NE(nullptr, event);
    size_t offset = 0x10;
    createMutableSemaphoreWait<FamilyType>(offset, 0, L0::MCL::MutableSemaphoreWait::Type::regularEventWait, false);

    createVariable(L0::MCL::VariableType::waitEvent, true, -1, -1);
    auto ret = this->variable->setAsWaitEvent(event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, event);
    this->variable->getSemWaitList().push_back(this->mutableSemaphoreWait.get());
    EXPECT_FALSE(this->variable->eventValue.counterBasedEvent);
    auto expectedWaitAddress = event->getGpuAddress(device) + offset;

    auto newEvent = nullptr;

    ret = this->variable->setValue(0, 0, newEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, newEvent);
    EXPECT_TRUE(this->variable->eventValue.noopState);

    EXPECT_EQ(0, memcmp(noopSemaphoreSpace, this->semaphoreWaitBuffer, sizeof(MI_SEMAPHORE_WAIT)));

    ret = this->variable->setValue(0, 0, event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, event);
    EXPECT_FALSE(this->variable->eventValue.noopState);

    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(this->semaphoreWaitBuffer);
    auto testWaitAddress = semWaitCmd->getSemaphoreGraphicsAddress();
    EXPECT_EQ(expectedWaitAddress, testWaitAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableTest,
            givenWaitEventVariableWhenSavingDeviceCounterAllocationThenPeerCounterAllocIsSaved) {
    auto event = this->createTestEvent(true, false, false, false);
    ASSERT_NE(nullptr, event);

    uint32_t peerDeviceIndex = this->device->getRootDeviceIndex() + 1;
    MockGraphicsAllocation peerCounterDeviceAlloc(peerDeviceIndex, reinterpret_cast<void *>(0x1234), 0x0u);
    auto inOrderExecInfo = NEO::InOrderExecInfo::createFromExternalAllocation(*device->getNEODevice(), &peerCounterDeviceAlloc, 0x1, &peerCounterDeviceAlloc, 0, 1, 1, 1);

    event->updateInOrderExecState(inOrderExecInfo, 1, 0);

    createVariable(L0::MCL::VariableType::waitEvent, true, -1, -1);
    auto ret = this->variable->setAsWaitEvent(event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(peerCounterDeviceAlloc.getGpuAddress(), this->variable->eventValue.cbEventDeviceCounterAllocation->getGpuAddress());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableInOrderTest,
            givenSignalEventRegularNoCounterBasedSignalScopeTimestampWhenMutatingVariableThenNewPostSyncAddressSet) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    auto event = this->createTestEvent(false, true, true, false);
    size_t offset = 0x10;
    createMutableStoreDataImm<FamilyType>(offset, false);

    createVariable(L0::MCL::VariableType::signalEvent, true, -1, -1);
    auto ret = this->variable->setAsSignalEvent(event, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, event);
    this->variable->getStoreDataImmList().push_back(this->mutableStoreDataImm.get());

    auto newEvent = this->createTestEvent(false, true, true, false);
    ASSERT_NE(nullptr, newEvent);
    auto expectedPostSyncAddress = newEvent->getGpuAddress(device) + offset;

    ret = this->variable->setValue(0, 0, newEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, newEvent);

    auto storeDataImmCmd = reinterpret_cast<MI_STORE_DATA_IMM *>(this->storeDataImmBuffer);
    auto testPostSyncAddress = storeDataImmCmd->getAddress();
    EXPECT_EQ(expectedPostSyncAddress, testPostSyncAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableInOrderTest,
            givenCbSignalEventWhenMutatingEventThenDataIsUpdated) {
    auto event = this->createTestEvent(true, false, false, false);
    ASSERT_NE(nullptr, event);

    this->attachCbEvent(event);

    createVariable(L0::MCL::VariableType::signalEvent, true, -1, -1);
    auto ret = this->variable->setAsSignalEvent(event, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, event);
    EXPECT_EQ(this->cmdListInOrderAllocationOffset, this->variable->eventValue.inOrderAllocationOffset);
    EXPECT_EQ(this->cmdListInOrderCounterValue, this->variable->eventValue.inOrderExecBaseSignalValue);

    auto newEvent = this->createTestEvent(true, false, false, false);
    ASSERT_NE(nullptr, newEvent);

    ret = this->variable->setValue(0, 0, newEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, newEvent);
    EXPECT_EQ(this->cmdListInOrderAllocationOffset, newEvent->getInOrderAllocationOffset());
    EXPECT_EQ(this->cmdListInOrderCounterValue, newEvent->getInOrderExecBaseSignalValue());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableInOrderTest,
            givenCbSignalTimestampEventWhenMutatingEventThenDataIsUpdatedAndTimestampPropertySet) {
    auto event = this->createTestEvent(true, false, true, false);
    ASSERT_NE(nullptr, event);

    this->attachCbEvent(event);

    createVariable(L0::MCL::VariableType::signalEvent, true, -1, -1);
    auto ret = this->variable->setAsSignalEvent(event, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, event);
    EXPECT_EQ(this->cmdListInOrderAllocationOffset, this->variable->eventValue.inOrderAllocationOffset);
    EXPECT_EQ(this->cmdListInOrderCounterValue, this->variable->eventValue.inOrderExecBaseSignalValue);
    EXPECT_TRUE(this->variable->eventValue.hasStandaloneProfilingNode);

    auto newEvent = this->createTestEvent(true, false, true, false);
    ASSERT_NE(nullptr, newEvent);

    ret = this->variable->setValue(0, 0, newEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, newEvent);
    EXPECT_EQ(this->cmdListInOrderAllocationOffset, newEvent->getInOrderAllocationOffset());
    EXPECT_EQ(this->cmdListInOrderCounterValue, newEvent->getInOrderExecBaseSignalValue());
    EXPECT_TRUE(newEvent->hasInOrderTimestampNode());
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableInOrderTest,
            givenCbSignalExternalEventWhenMutatingEventThenDataIsUpdatedAndExternalPropertySet) {
    auto event = this->createTestEvent(true, false, false, true);
    ASSERT_NE(nullptr, event);

    createVariable(L0::MCL::VariableType::signalEvent, true, -1, -1);
    auto ret = this->variable->setAsSignalEvent(event, this->mutableComputeWalker.get(), nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, event);
    EXPECT_EQ(this->externalEventCounterValue, this->variable->eventValue.inOrderExecBaseSignalValue);

    auto newEvent = this->createTestEvent(true, false, false, true);
    ASSERT_NE(nullptr, newEvent);

    ret = this->variable->setValue(0, 0, newEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, newEvent);
    EXPECT_EQ(this->externalEventCounterValue * 2, newEvent->getInOrderExecBaseSignalValue());
    EXPECT_EQ(this->externalEventCounterValue * 2, this->variable->eventValue.inOrderExecBaseSignalValue);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableInOrderTest,
            givenCounterBasedWaitEventBelongingToVariableMclWhenMutatingIntoEventBelongingToSameMclThenStateIsPreserved) {
    auto event = this->createTestEvent(true, false, false, false);
    ASSERT_NE(nullptr, event);

    this->attachCbEvent(event);

    auto &inOrderPatchCmds = prepareInOrderWaitCommands<FamilyType>(this->mutableCommandList.get()->base, true, true);

    createVariable(L0::MCL::VariableType::waitEvent, true, -1, -1);
    auto ret = this->variable->setAsWaitEvent(event);
    if (this->qwordIndirect) {
        this->variable->getLoadRegImmList().push_back(this->mutableLoadRegisterImms[0].get());
        this->variable->getLoadRegImmList().push_back(this->mutableLoadRegisterImms[1].get());
    }
    this->variable->getSemWaitList().push_back(this->mutableSemaphoreWait.get());

    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, event);
    EXPECT_TRUE(this->variable->eventValue.counterBasedEvent);
    EXPECT_TRUE(this->variable->eventValue.noopState);
    EXPECT_TRUE(this->variable->eventValue.isCbEventBoundToCmdList);

    auto newEvent = this->createTestEvent(true, false, false, false);
    ASSERT_NE(nullptr, newEvent);

    this->attachCbEvent(newEvent);
    ret = this->variable->setValue(0, 0, newEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, newEvent);
    EXPECT_TRUE(this->variable->eventValue.noopState);
    EXPECT_TRUE(this->variable->eventValue.isCbEventBoundToCmdList);
    EXPECT_EQ(nullptr, this->variable->eventValue.cbEventDeviceCounterAllocation);

    EXPECT_TRUE(inOrderPatchCmds[0].skipPatching);
    if (this->qwordIndirect) {
        EXPECT_TRUE(inOrderPatchCmds[1].skipPatching);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableInOrderTest,
            givenCounterBasedWaitEventBelongingToVariableMclWhenMutatingIntoEventBelongingToDifferentMclThenWaitIsUpdated) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto event = this->createTestEvent(true, false, false, false);
    ASSERT_NE(nullptr, event);

    this->attachCbEvent(event);

    auto &inOrderPatchCmds = prepareInOrderWaitCommands<FamilyType>(this->mutableCommandList.get()->base, true, true);

    createVariable(L0::MCL::VariableType::waitEvent, true, -1, -1);
    auto ret = this->variable->setAsWaitEvent(event);
    if (this->qwordIndirect) {
        this->variable->getLoadRegImmList().push_back(this->mutableLoadRegisterImms[0].get());
        this->variable->getLoadRegImmList().push_back(this->mutableLoadRegisterImms[1].get());
    }
    this->variable->getSemWaitList().push_back(this->mutableSemaphoreWait.get());

    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, event);
    EXPECT_TRUE(this->variable->eventValue.counterBasedEvent);
    EXPECT_TRUE(this->variable->eventValue.noopState);
    EXPECT_TRUE(this->variable->eventValue.isCbEventBoundToCmdList);

    std::unique_ptr<MutableCommandList> differentCmdList = createMutableCmdList();
    auto newEvent = this->createTestEvent(true, false, false, false);
    ASSERT_NE(nullptr, newEvent);

    this->attachCbEvent(newEvent, static_cast<L0::ult::MockCommandList *>(differentCmdList->base));

    ret = this->variable->setValue(0, 0, newEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, newEvent);
    EXPECT_FALSE(this->variable->eventValue.noopState);
    EXPECT_FALSE(this->variable->eventValue.isCbEventBoundToCmdList);
    EXPECT_NE(nullptr, this->variable->eventValue.cbEventDeviceCounterAllocation);

    auto expectedWaitAddress = newEvent->getInOrderExecInfo()->getBaseDeviceAddress() + newEvent->getInOrderAllocationOffset() + this->semWaitOffset;

    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(this->semaphoreWaitBuffer);
    auto testWaitAddress = semWaitCmd->getSemaphoreGraphicsAddress();
    EXPECT_EQ(expectedWaitAddress, testWaitAddress);

    EXPECT_FALSE(inOrderPatchCmds[0].skipPatching);
    if (this->qwordIndirect) {
        EXPECT_FALSE(inOrderPatchCmds[1].skipPatching);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableInOrderTest,
            givenCounterBasedWaitEventBelongingToDifferentMclWhenMutatingIntoEventBelongingToOtherDifferentMclThenWaitIsUpdated) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    std::unique_ptr<MutableCommandList> differentCmdList = createMutableCmdList();
    auto event = this->createTestEvent(true, false, false, false);
    ASSERT_NE(nullptr, event);

    this->attachCbEvent(event, static_cast<L0::ult::MockCommandList *>(differentCmdList->base));

    auto &inOrderPatchCmds = prepareInOrderWaitCommands<FamilyType>(this->mutableCommandList.get()->base, false, true);

    createVariable(L0::MCL::VariableType::waitEvent, true, -1, -1);
    auto ret = this->variable->setAsWaitEvent(event);
    if (this->qwordIndirect) {
        this->variable->getLoadRegImmList().push_back(this->mutableLoadRegisterImms[0].get());
        this->variable->getLoadRegImmList().push_back(this->mutableLoadRegisterImms[1].get());
    }
    this->variable->getSemWaitList().push_back(this->mutableSemaphoreWait.get());

    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, event);
    EXPECT_TRUE(this->variable->eventValue.counterBasedEvent);
    EXPECT_FALSE(this->variable->eventValue.noopState);
    EXPECT_FALSE(this->variable->eventValue.isCbEventBoundToCmdList);

    std::unique_ptr<MutableCommandList> otherDifferentCmdList = createMutableCmdList();
    auto newEvent = this->createTestEvent(true, false, false, false);
    ASSERT_NE(nullptr, newEvent);

    this->attachCbEvent(newEvent, static_cast<L0::ult::MockCommandList *>(otherDifferentCmdList->base));

    ret = this->variable->setValue(0, 0, newEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, newEvent);
    EXPECT_FALSE(this->variable->eventValue.noopState);
    EXPECT_FALSE(this->variable->eventValue.isCbEventBoundToCmdList);
    EXPECT_NE(nullptr, this->variable->eventValue.cbEventDeviceCounterAllocation);

    auto expectedWaitAddress = newEvent->getInOrderExecInfo()->getBaseDeviceAddress() + newEvent->getInOrderAllocationOffset() + this->semWaitOffset;

    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(this->semaphoreWaitBuffer);
    auto testWaitAddress = semWaitCmd->getSemaphoreGraphicsAddress();
    EXPECT_EQ(expectedWaitAddress, testWaitAddress);

    EXPECT_FALSE(inOrderPatchCmds[0].skipPatching);
    if (this->qwordIndirect) {
        EXPECT_FALSE(inOrderPatchCmds[1].skipPatching);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableInOrderTest,
            givenCounterBasedWaitEventBelongingToDifferentMclWhenNoopingAndRestoringEventThenWaitIsNoopedAndRestored) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    alignas(sizeof(uint32_t)) uint8_t noopSemaphoreSpace[sizeof(MI_SEMAPHORE_WAIT)] = {};
    memset(noopSemaphoreSpace, 0, sizeof(MI_SEMAPHORE_WAIT));

    std::unique_ptr<MutableCommandList> differentCmdList = createMutableCmdList();
    auto event = this->createTestEvent(true, false, false, false);
    ASSERT_NE(nullptr, event);

    this->attachCbEvent(event, static_cast<L0::ult::MockCommandList *>(differentCmdList->base));
    auto expectedWaitAddress = event->getInOrderExecInfo()->getBaseDeviceAddress() + event->getInOrderAllocationOffset() + this->semWaitOffset;

    auto &inOrderPatchCmds = prepareInOrderWaitCommands<FamilyType>(this->mutableCommandList.get()->base, false, true);

    createVariable(L0::MCL::VariableType::waitEvent, true, -1, -1);
    auto ret = this->variable->setAsWaitEvent(event);
    if (this->qwordIndirect) {
        this->variable->getLoadRegImmList().push_back(this->mutableLoadRegisterImms[0].get());
        this->variable->getLoadRegImmList().push_back(this->mutableLoadRegisterImms[1].get());
    }
    this->variable->getSemWaitList().push_back(this->mutableSemaphoreWait.get());

    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, event);
    EXPECT_TRUE(this->variable->eventValue.counterBasedEvent);
    EXPECT_FALSE(this->variable->eventValue.noopState);
    EXPECT_FALSE(this->variable->eventValue.isCbEventBoundToCmdList);

    auto newEvent = nullptr;

    ret = this->variable->setValue(0, 0, newEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, newEvent);
    EXPECT_TRUE(this->variable->eventValue.noopState);
    EXPECT_EQ(nullptr, this->variable->eventValue.cbEventDeviceCounterAllocation);

    EXPECT_EQ(0, memcmp(noopSemaphoreSpace, this->semaphoreWaitBuffer, sizeof(MI_SEMAPHORE_WAIT)));
    EXPECT_TRUE(inOrderPatchCmds[0].skipPatching);
    if (this->qwordIndirect) {
        EXPECT_TRUE(inOrderPatchCmds[1].skipPatching);
    }

    ret = this->variable->setValue(0, 0, event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, event);
    EXPECT_FALSE(this->variable->eventValue.noopState);
    EXPECT_NE(nullptr, this->variable->eventValue.cbEventDeviceCounterAllocation);

    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(this->semaphoreWaitBuffer);
    auto testWaitAddress = semWaitCmd->getSemaphoreGraphicsAddress();
    EXPECT_EQ(expectedWaitAddress, testWaitAddress);

    EXPECT_FALSE(inOrderPatchCmds[0].skipPatching);
    if (this->qwordIndirect) {
        EXPECT_FALSE(inOrderPatchCmds[1].skipPatching);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableInOrderTest,
            givenExternalCounterBasedWaitEventWhenMutatingIntoOtherExternalCounterBasedEventThenWaitIsUpdated) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto event = this->createTestEvent(true, false, false, true);
    ASSERT_NE(nullptr, event);

    prepareInOrderWaitCommands<FamilyType>(this->mutableCommandList.get()->base, false, false);

    createVariable(L0::MCL::VariableType::waitEvent, true, -1, -1);
    auto ret = this->variable->setAsWaitEvent(event);
    if (this->qwordIndirect) {
        this->variable->getLoadRegImmList().push_back(this->mutableLoadRegisterImms[0].get());
        this->variable->getLoadRegImmList().push_back(this->mutableLoadRegisterImms[1].get());
    }
    this->variable->getSemWaitList().push_back(this->mutableSemaphoreWait.get());

    auto newEvent = this->createTestEvent(true, false, false, true);
    ASSERT_NE(nullptr, newEvent);

    ret = this->variable->setValue(0, 0, newEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, newEvent);
    EXPECT_FALSE(this->variable->eventValue.noopState);
    EXPECT_FALSE(this->variable->eventValue.isCbEventBoundToCmdList);
    EXPECT_NE(nullptr, this->variable->eventValue.cbEventDeviceCounterAllocation);

    auto expectedWaitAddress = newEvent->getInOrderExecInfo()->getBaseDeviceAddress() + newEvent->getInOrderAllocationOffset() + this->semWaitOffset;

    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(this->semaphoreWaitBuffer);
    auto testWaitAddress = semWaitCmd->getSemaphoreGraphicsAddress();
    EXPECT_EQ(expectedWaitAddress, testWaitAddress);
}

HWCMDTEST_F(IGFX_XE_HP_CORE,
            VariableInOrderTest,
            givenExternalCounterBasedWaitEventWhenWhenNoopingAndRestoringEventThenWaitIsNoopedAndRestored) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    alignas(sizeof(uint32_t)) uint8_t noopSemaphoreSpace[sizeof(MI_SEMAPHORE_WAIT)] = {};
    memset(noopSemaphoreSpace, 0, sizeof(MI_SEMAPHORE_WAIT));

    auto event = this->createTestEvent(true, false, false, true);
    ASSERT_NE(nullptr, event);

    prepareInOrderWaitCommands<FamilyType>(this->mutableCommandList.get()->base, false, false);

    createVariable(L0::MCL::VariableType::waitEvent, true, -1, -1);
    auto ret = this->variable->setAsWaitEvent(event);
    if (this->qwordIndirect) {
        this->variable->getLoadRegImmList().push_back(this->mutableLoadRegisterImms[0].get());
        this->variable->getLoadRegImmList().push_back(this->mutableLoadRegisterImms[1].get());
    }
    this->variable->getSemWaitList().push_back(this->mutableSemaphoreWait.get());

    auto expectedWaitAddress = event->getInOrderExecInfo()->getBaseDeviceAddress() + event->getInOrderAllocationOffset() + this->semWaitOffset;

    auto newEvent = nullptr;

    ret = this->variable->setValue(0, 0, newEvent);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, newEvent);
    EXPECT_TRUE(this->variable->eventValue.noopState);
    EXPECT_EQ(nullptr, this->variable->eventValue.cbEventDeviceCounterAllocation);

    EXPECT_EQ(0, memcmp(noopSemaphoreSpace, this->semaphoreWaitBuffer, sizeof(MI_SEMAPHORE_WAIT)));

    ret = this->variable->setValue(0, 0, event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    EXPECT_EQ(this->variable->eventValue.event, event);
    EXPECT_FALSE(this->variable->eventValue.noopState);
    EXPECT_NE(nullptr, this->variable->eventValue.cbEventDeviceCounterAllocation);

    auto semWaitCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(this->semaphoreWaitBuffer);
    auto testWaitAddress = semWaitCmd->getSemaphoreGraphicsAddress();
    EXPECT_EQ(expectedWaitAddress, testWaitAddress);
}

} // namespace ult
} // namespace L0
