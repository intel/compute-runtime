/*
 * Copyright (C) 2024 Intel Corporation
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
void HardwareParse::findHardwareCommands<GenGfxFamily>(IndirectHeap *dsh) {
    typedef typename GenGfxFamily::COMPUTE_WALKER COMPUTE_WALKER;
    typedef typename GenGfxFamily::CFE_STATE CFE_STATE;
    typedef typename GenGfxFamily::PIPELINE_SELECT PIPELINE_SELECT;
    typedef typename GenGfxFamily::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename GenGfxFamily::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;
    typedef typename GenGfxFamily::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    typedef typename GenGfxFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename GenGfxFamily::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    itorWalker = find<COMPUTE_WALKER *>(cmdList.begin(), cmdList.end());
    if (itorWalker != cmdList.end()) {
        cmdWalker = *itorWalker;
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

    itorMediaVfeState = find<CFE_STATE *>(requiresPipelineSelectBeforeMediaState<GenGfxFamily>() ? itorPipelineSelect : cmdList.begin(), itorWalker);
    if (itorMediaVfeState != itorWalker) {
        cmdMediaVfeState = *itorMediaVfeState;
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
        }
        ASSERT_NE(0u, dynamicStateHeap);
    }

    itorBindingTableBaseAddress = find<_3DSTATE_BINDING_TABLE_POOL_ALLOC *>(cmdList.begin(), itorWalker);
    if (itorBindingTableBaseAddress != itorWalker) {
        cmdBindingTableBaseAddress = *itorBindingTableBaseAddress;
    }

    // interfaceDescriptorData should be located within COMPUTE_WALKER
    if (cmdWalker) {
        // Extract the interfaceDescriptorData
        INTERFACE_DESCRIPTOR_DATA &idd = reinterpret_cast<COMPUTE_WALKER *>(cmdWalker)->getInterfaceDescriptor();
        cmdInterfaceDescriptorData = &idd;
    }
}

template <>
const void *HardwareParse::getStatelessArgumentPointer<GenGfxFamily>(const KernelInfo &kernelInfo, uint32_t indexArg, IndirectHeap &ioh, uint32_t rootDeviceIndex) {
    typedef typename GenGfxFamily::COMPUTE_WALKER COMPUTE_WALKER;
    typedef typename GenGfxFamily::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

    auto cmdWalker = (COMPUTE_WALKER *)this->cmdWalker;
    EXPECT_NE(nullptr, cmdWalker);
    auto inlineInComputeWalker = cmdWalker->getInlineDataPointer();

    auto cmdSBA = (STATE_BASE_ADDRESS *)cmdStateBaseAddress;
    EXPECT_NE(nullptr, cmdSBA);
    auto argOffset = std::numeric_limits<uint32_t>::max();
    // Determine where the argument is
    const auto &arg = kernelInfo.getArgDescriptorAt(indexArg);
    if (arg.is<ArgDescriptor::argTPointer>() && isValidOffset(arg.as<ArgDescPointer>().stateless)) {
        argOffset = arg.as<ArgDescPointer>().stateless;
    } else {
        return nullptr;
    }

    bool inlineDataActive = kernelInfo.kernelDescriptor.kernelAttributes.flags.passInlineData;
    auto inlineDataSize = 32u;

    auto offsetCrossThreadData = cmdWalker->getIndirectDataStartAddress();

    offsetCrossThreadData -= static_cast<uint32_t>(ioh.getGraphicsAllocation()->getGpuAddressToPatch());

    // Get the base of cross thread
    auto pCrossThreadData = ptrOffset(
        reinterpret_cast<const void *>(ioh.getCpuBase()),
        offsetCrossThreadData);

    if (inlineDataActive) {
        if (argOffset < inlineDataSize) {
            return ptrOffset(inlineInComputeWalker, argOffset);
        } else {
            return ptrOffset(pCrossThreadData, argOffset - inlineDataSize);
        }
    }

    return ptrOffset(pCrossThreadData, argOffset);
}

template <>
void HardwareParse::findHardwareCommands<GenGfxFamily>() {
    findHardwareCommands<GenGfxFamily>(nullptr);
}
} // namespace NEO
