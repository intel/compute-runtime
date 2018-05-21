/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "unit_tests/helpers/hw_parse.h"

namespace OCLRT {

template <typename FamilyType>
void HardwareParse::findHardwareCommands() {
    typedef typename FamilyType::GPGPU_WALKER GPGPU_WALKER;
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::MEDIA_VFE_STATE MEDIA_VFE_STATE;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;
    typedef typename FamilyType::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;

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
        ASSERT_NE(0u, dynamicStateHeap);
    }

    // interfaceDescriptorData should be located within dynamicStateHeap
    if (cmdMIDL && cmdSBA) {
        auto iddStart = cmdMIDL->getInterfaceDescriptorDataStartAddress();
        auto iddEnd = iddStart + cmdMIDL->getInterfaceDescriptorTotalLength();
        ASSERT_LE(iddEnd, cmdSBA->getDynamicStateBufferSize() * MemoryConstants::pageSize);

        // Extract the interfaceDescriptorData
        cmdInterfaceDescriptorData = (INTERFACE_DESCRIPTOR_DATA *)(dynamicStateHeap + iddStart);
    }
}

template <typename FamilyType>
const void *HardwareParse::getStatelessArgumentPointer(const Kernel &kernel, uint32_t indexArg, IndirectHeap &ioh) {
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
    auto &patchInfo = kernel.getKernelInfo().patchInfo;
    for (auto &arg : patchInfo.statelessGlobalMemObjKernelArgs) {
        if (arg->ArgumentNumber == indexArg) {
            return ptrOffset(pCrossThreadData, arg->DataParamOffset);
        }
    }
    return nullptr;
}

} // namespace OCLRT
