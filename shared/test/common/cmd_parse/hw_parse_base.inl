/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/program/kernel_info.h"
#include "shared/test/common/cmd_parse/hw_parse.h"

namespace NEO {

template <typename FamilyType>
void HardwareParse::findHardwareCommands(IndirectHeap *dsh) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    itorWalker = find<GPGPU_WALKER *>(cmdList.begin(), cmdList.end());
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

    MEDIA_INTERFACE_DESCRIPTOR_LOAD *cmdMIDL = nullptr;
    itorMediaInterfaceDescriptorLoad = find<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmdList.begin(), itorWalker);
    if (itorMediaInterfaceDescriptorLoad != itorWalker) {
        cmdMIDL = (MEDIA_INTERFACE_DESCRIPTOR_LOAD *)*itorMediaInterfaceDescriptorLoad;
        cmdMediaInterfaceDescriptorLoad = *itorMediaInterfaceDescriptorLoad;
    }

    itorPipelineSelect = find<PIPELINE_SELECT *>(cmdList.begin(), itorWalker);
    if (itorPipelineSelect != itorWalker) {
        cmdPipelineSelect = *itorPipelineSelect;
    }

    itorMediaVfeState = find<MEDIA_VFE_STATE *>(itorPipelineSelect, itorWalker);
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

    // interfaceDescriptorData should be located within dynamicStateHeap
    if constexpr (!FamilyType::isHeaplessRequired()) {
        using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
        if (cmdMIDL && cmdSBA) {
            auto iddStart = cmdMIDL->getInterfaceDescriptorDataStartAddress();
            auto iddEnd = iddStart + cmdMIDL->getInterfaceDescriptorTotalLength();
            ASSERT_LE(iddEnd, cmdSBA->getDynamicStateBufferSize() * MemoryConstants::pageSize);

            // Extract the interfaceDescriptorData
            cmdInterfaceDescriptorData = (INTERFACE_DESCRIPTOR_DATA *)(dynamicStateHeap + iddStart);
        }
    }
}

template <typename FamilyType>
void HardwareParse::findHardwareCommands() {
    findHardwareCommands<FamilyType>(nullptr);
}

template <typename FamilyType>
const void *HardwareParse::getStatelessArgumentPointer(const KernelInfo &kernelInfo, uint32_t indexArg, IndirectHeap &ioh, uint32_t rootDeviceIndex) {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

    auto cmdWalker = (GPGPU_WALKER *)this->cmdWalker;
    EXPECT_NE(nullptr, cmdWalker);

    auto cmdSBA = (STATE_BASE_ADDRESS *)cmdStateBaseAddress;
    EXPECT_NE(nullptr, cmdSBA);

    auto offsetCrossThreadData = cmdWalker->getIndirectDataStartAddress();
    EXPECT_LT(offsetCrossThreadData, cmdSBA->getIndirectObjectBufferSize() * MemoryConstants::pageSize);

    offsetCrossThreadData -= static_cast<uint32_t>(ioh.getGraphicsAllocation()->getGpuAddressToPatch());

    // Get the base of cross thread
    auto pCrossThreadData = ptrOffset(
        reinterpret_cast<const void *>(ioh.getCpuBase()),
        offsetCrossThreadData);

    // Determine where the argument is
    const auto &arg = kernelInfo.getArgDescriptorAt(indexArg);
    if (arg.is<ArgDescriptor::argTPointer>() && isValidOffset(arg.as<ArgDescPointer>().stateless)) {
        return ptrOffset(pCrossThreadData, arg.as<ArgDescPointer>().stateless);
    }
    return nullptr;
}

template <typename FamilyType>
const typename FamilyType::RENDER_SURFACE_STATE *HardwareParse::getSurfaceState(IndirectHeap *ssh, uint32_t index) {
    if constexpr (!FamilyType::isHeaplessRequired()) {
        typedef typename FamilyType::BINDING_TABLE_STATE BINDING_TABLE_STATE;
        typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;
        typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
        typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

        const auto &interfaceDescriptorData = *(INTERFACE_DESCRIPTOR_DATA *)cmdInterfaceDescriptorData;

        auto cmdSBA = (STATE_BASE_ADDRESS *)cmdStateBaseAddress;
        auto surfaceStateHeap = cmdSBA->getSurfaceStateBaseAddress();
        if (ssh && (ssh->getHeapGpuBase() == surfaceStateHeap)) {
            surfaceStateHeap = reinterpret_cast<uint64_t>(ssh->getCpuBase());
        }
        EXPECT_NE(0u, surfaceStateHeap);

        auto bindingTablePointer = interfaceDescriptorData.getBindingTablePointer();

        const auto &bindingTableState = reinterpret_cast<BINDING_TABLE_STATE *>(surfaceStateHeap + bindingTablePointer)[index];
        auto surfaceStatePointer = bindingTableState.getSurfaceStatePointer();

        return (RENDER_SURFACE_STATE *)(surfaceStateHeap + surfaceStatePointer);
    } else {
        UNRECOVERABLE_IF(true);
        return nullptr;
    }
}

template <typename FamilyType>
bool HardwareParse::isStallingBarrier(GenCmdList::iterator &iter) {
    PIPE_CONTROL *pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*iter);
    if (pipeControlCmd == nullptr) {
        return false;
    }
    EXPECT_EQ(pipeControlCmd->getCommandStreamerStallEnable(), true);
    EXPECT_EQ(pipeControlCmd->getDcFlushEnable(), false);
    EXPECT_EQ(pipeControlCmd->getRenderTargetCacheFlushEnable(), false);
    EXPECT_EQ(pipeControlCmd->getInstructionCacheInvalidateEnable(), false);
    EXPECT_EQ(pipeControlCmd->getTextureCacheInvalidationEnable(), false);
    EXPECT_EQ(pipeControlCmd->getPipeControlFlushEnable(), false);
    EXPECT_EQ(pipeControlCmd->getVfCacheInvalidationEnable(), false);
    EXPECT_EQ(pipeControlCmd->getConstantCacheInvalidationEnable(), false);
    EXPECT_EQ(pipeControlCmd->getStateCacheInvalidationEnable(), false);
    return true;
}

} // namespace NEO
