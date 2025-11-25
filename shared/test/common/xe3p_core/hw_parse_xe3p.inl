/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/program/kernel_info.h"
#include "shared/test/common/cmd_parse/hw_parse.h"

#include "gtest/gtest.h"

namespace NEO {

template <>
void NEO::HardwareParse::findHardwareCommands<GenGfxFamily>(IndirectHeap *dsh) {
    using WalkerType = typename GenGfxFamily::DefaultWalkerType;
    using PIPE_CONTROL = typename GenGfxFamily::PIPE_CONTROL;
    using CFE_STATE = typename GenGfxFamily::CFE_STATE;
    using PIPELINE_SELECT = typename GenGfxFamily::PIPELINE_SELECT;
    using STATE_BASE_ADDRESS = typename GenGfxFamily::STATE_BASE_ADDRESS;
    using MI_BATCH_BUFFER_START = typename GenGfxFamily::MI_BATCH_BUFFER_START;
    using MI_LOAD_REGISTER_IMM = typename GenGfxFamily::MI_LOAD_REGISTER_IMM;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename GenGfxFamily::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    itorWalker = NEO::UnitTestHelper<GenGfxFamily>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());

    bool isHeapless = GenGfxFamily::isHeaplessMode<WalkerType>();

    if (itorWalker != cmdList.end()) {
        auto walker = genCmdCast<WalkerType *>(*itorWalker);

        this->cmdWalker = static_cast<void *>(walker);

        auto &idd = walker->getInterfaceDescriptor();
        this->cmdInterfaceDescriptorData = static_cast<void *>(&idd);
    }

    itorBBStartAfterWalker = find<MI_BATCH_BUFFER_START *>(itorWalker, cmdList.end());
    if (itorBBStartAfterWalker != cmdList.end()) {
        cmdBBStartAfterWalker = *itorBBStartAfterWalker;
    }
    for (auto it = cmdList.begin(); it != cmdList.end(); it++) {
        auto lri = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
        if (lri) {
            lriList.push_back(*it);
        }
    }

    if (parsePipeControl) {
        for (auto it = cmdList.begin(); it != cmdList.end(); it++) {
            auto pipeControl = genCmdCast<PIPE_CONTROL *>(*it);
            if (pipeControl) {
                pipeControlList.push_back(*it);
            }
        }
    }

    itorPipelineSelect = find<PIPELINE_SELECT *>(cmdList.begin(), itorWalker);
    if (itorPipelineSelect != itorWalker) {
        cmdPipelineSelect = *itorPipelineSelect;
    }

    if (!isHeapless) {
        itorMediaVfeState = find<CFE_STATE *>(requiresPipelineSelectBeforeMediaState<GenGfxFamily>() ? itorPipelineSelect : cmdList.begin(), itorWalker);
        if (itorMediaVfeState != itorWalker) {
            cmdMediaVfeState = *itorMediaVfeState;
        }
    }

    STATE_BASE_ADDRESS *cmdSBA = nullptr;
    uint64_t dynamicStateHeap = 0;
    itorStateBaseAddress = find<STATE_BASE_ADDRESS *>(cmdList.begin(), itorWalker);
    if (itorStateBaseAddress != itorWalker) {
        cmdSBA = (STATE_BASE_ADDRESS *)*itorStateBaseAddress;
        cmdStateBaseAddress = *itorStateBaseAddress;

        // Extract the dynamicStateHeap
        dynamicStateHeap = cmdSBA->getDynamicStateBaseAddress();
        if (dsh && (dsh->getHeapGpuBase() == dynamicStateHeap)) {
            dynamicStateHeap = reinterpret_cast<uint64_t>(dsh->getCpuBase());
            ASSERT_NE(0u, dynamicStateHeap);
        }
    }

    itorBindingTableBaseAddress = find<_3DSTATE_BINDING_TABLE_POOL_ALLOC *>(cmdList.begin(), itorWalker);
    if (itorBindingTableBaseAddress != itorWalker) {
        cmdBindingTableBaseAddress = *itorBindingTableBaseAddress;
    }
}
template <>
const void *HardwareParse::getStatelessArgumentPointer<GenGfxFamily>(const KernelInfo &kernelInfo, uint32_t indexArg, IndirectHeap &ioh, uint32_t rootDeviceIndex) {
    using DefaultWalkerType = typename GenGfxFamily::DefaultWalkerType;
    using STATE_BASE_ADDRESS = typename GenGfxFamily::STATE_BASE_ADDRESS;

    bool isHeapless = false;
    void *inlineInComputeWalker = nullptr;

    auto walker = genCmdCast<DefaultWalkerType *>(*itorWalker);
    using WalkerType = std::decay_t<decltype(*walker)>;

    EXPECT_NE(nullptr, walker);
    inlineInComputeWalker = walker->getInlineDataPointer();

    if constexpr (std::is_same_v<WalkerType, COMPUTE_WALKER_2>) {
        isHeapless = true;
    }

    auto argOffset = 0u;
    const auto &arg = kernelInfo.getArgDescriptorAt(indexArg);
    if (arg.is<ArgDescriptor::argTPointer>() && isValidOffset(arg.as<ArgDescPointer>().stateless)) {
        argOffset = arg.as<ArgDescPointer>().stateless;
    } else {
        return nullptr;
    }

    bool inlineDataActive = kernelInfo.kernelDescriptor.kernelAttributes.flags.passInlineData;
    auto inlineDataSize = isHeapless ? 64u : 32u;
    bool argumentInInlineData = (argOffset < inlineDataSize);

    if (isHeapless) {
        EXPECT_TRUE(inlineDataActive);

        if (argumentInInlineData) {
            return ptrOffset(inlineInComputeWalker, argOffset);
        } else {

            void *crossThreadData = ioh.getGraphicsAllocation()->getUnderlyingBuffer();

            return ptrOffset(crossThreadData, argOffset - inlineDataSize);
        }

    } else {

        auto cmdSBA = static_cast<STATE_BASE_ADDRESS *>(cmdStateBaseAddress);
        EXPECT_NE(nullptr, cmdSBA);

        auto offsetCrossThreadData = static_cast<COMPUTE_WALKER *>(cmdWalker)->getIndirectDataStartAddress();

        offsetCrossThreadData -= static_cast<uint32_t>(ioh.getGraphicsAllocation()->getGpuAddressToPatch());

        // Get the base of cross thread
        auto pCrossThreadData = ptrOffset(reinterpret_cast<const void *>(ioh.getCpuBase()), offsetCrossThreadData);

        if (inlineDataActive) {
            if (argOffset < inlineDataSize) {
                return ptrOffset(inlineInComputeWalker, argOffset);
            } else {
                return ptrOffset(pCrossThreadData, argOffset - inlineDataSize);
            }
        }

        return ptrOffset(pCrossThreadData, argOffset);
    }
}

template <>
void HardwareParse::findHardwareCommands<GenGfxFamily>() {
    findHardwareCommands<GenGfxFamily>(nullptr);
}
} // namespace NEO
