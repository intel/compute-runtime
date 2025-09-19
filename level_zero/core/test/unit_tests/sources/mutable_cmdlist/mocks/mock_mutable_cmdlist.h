/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_hw.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

template <GFXCORE_FAMILY gfxCoreFamily>
struct WhiteBox<::L0::MCL::MutableCommandListCoreFamily<gfxCoreFamily>>
    : public ::L0::MCL::MutableCommandListCoreFamily<gfxCoreFamily> {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    using BaseClass = ::L0::MCL::MutableCommandListCoreFamily<gfxCoreFamily>;
    using BaseClass::addCmdForPatching;
    using BaseClass::addVariableDispatch;
    using BaseClass::allocs;
    using BaseClass::allowCbWaitEventsNoopDispatch;
    using BaseClass::appendCmdsToPatch;
    using BaseClass::appendKernelMutableComputeWalker;
    using BaseClass::base;
    using BaseClass::baseCmdListClosed;
    using BaseClass::clearCommandsToPatch;
    using BaseClass::commandContainer;
    using BaseClass::commandListPerThreadScratchSize;
    using BaseClass::commandsToPatch;
    using BaseClass::containsAnyKernel;
    using BaseClass::containsCooperativeKernelsFlag;
    using BaseClass::dispatches;
    using BaseClass::engineGroupType;
    using BaseClass::eventMutations;
    using BaseClass::getAllocationFromHostPtrMap;
    using BaseClass::getInOrderIncrementValue;
    using BaseClass::getKernelData;
    using BaseClass::getVariableDescriptorContainer;
    using BaseClass::hasStageCommitVariables;
    using BaseClass::initialize;
    using BaseClass::inlineDataSize;
    using BaseClass::inOrderExecInfo;
    using BaseClass::inOrderPatchCmds;
    using BaseClass::iohAlignment;
    using BaseClass::isQwordInOrderCounter;
    using BaseClass::isSyncModeQueue;
    using BaseClass::kernelData;
    using BaseClass::kernelMutations;
    using BaseClass::maxPerThreadDataSize;
    using BaseClass::mutableAllocations;
    using BaseClass::mutableKernelGroups;
    using BaseClass::mutableLoadRegisterImmCmds;
    using BaseClass::mutablePipeControlCmds;
    using BaseClass::mutableSemaphoreWaitCmds;
    using BaseClass::mutableStoreDataImmCmds;
    using BaseClass::mutableStoreRegMemCmds;
    using BaseClass::mutableWalkerCmds;
    using BaseClass::nextAppendKernelMutable;
    using BaseClass::nextCommandId;
    using BaseClass::nextMutationFlags;
    using BaseClass::parseDispatchedKernel;
    using BaseClass::partitionCount;
    using BaseClass::stageCommitVariables;
    using BaseClass::updateInOrderExecInfo;
    using BaseClass::variableStorage;

    WhiteBox() : ::L0::MCL::MutableCommandListCoreFamily<gfxCoreFamily>(BaseClass::defaultNumIddsPerBlock) {}
};

template <GFXCORE_FAMILY gfxCoreFamily>
using MutableCommandListCoreFamily = WhiteBox<::L0::MCL::MutableCommandListCoreFamily<gfxCoreFamily>>;

template <>
struct WhiteBox<::L0::MCL::MutableCommandListImp> : public ::L0::MCL::MutableCommandListImp {
    using BaseClass = ::L0::MCL::MutableCommandListImp;
    using BaseClass::addVariableDispatch;
    using BaseClass::allocs;
    using BaseClass::appendCmdsToPatch;
    using BaseClass::appendKernelMutableComputeWalker;
    using BaseClass::base;
    using BaseClass::baseCmdListClosed;
    using BaseClass::dispatches;
    using BaseClass::eventMutations;
    using BaseClass::getKernelData;
    using BaseClass::getVariableDescriptorContainer;
    using BaseClass::hasStageCommitVariables;
    using BaseClass::inlineDataSize;
    using BaseClass::iohAlignment;
    using BaseClass::kernelData;
    using BaseClass::kernelMutations;
    using BaseClass::maxPerThreadDataSize;
    using BaseClass::mutableAllocations;
    using BaseClass::mutableKernelGroups;
    using BaseClass::mutableLoadRegisterImmCmds;
    using BaseClass::mutablePipeControlCmds;
    using BaseClass::mutableSemaphoreWaitCmds;
    using BaseClass::mutableStoreDataImmCmds;
    using BaseClass::mutableStoreRegMemCmds;
    using BaseClass::mutableWalkerCmds;
    using BaseClass::nextAppendKernelMutable;
    using BaseClass::nextCommandId;
    using BaseClass::nextMutationFlags;
    using BaseClass::parseDispatchedKernel;
    using BaseClass::stageCommitVariables;
    using BaseClass::updatedCommandList;
    using BaseClass::variableStorage;

    WhiteBox() : ::L0::MCL::MutableCommandListImp(nullptr) {}

    static WhiteBox<::L0::MCL::MutableCommandListImp> *whiteboxCast(::L0::MCL::MutableCommandList *cmdlist) {
        return static_cast<WhiteBox<::L0::MCL::MutableCommandListImp> *>(static_cast<::L0::MCL::MutableCommandListImp *>(cmdlist));
    }

    bool operator==(const WhiteBox &other) const {
        return false;
    }
};

using MutableCommandList = WhiteBox<::L0::MCL::MutableCommandListImp>;

} // namespace ult
} // namespace L0
