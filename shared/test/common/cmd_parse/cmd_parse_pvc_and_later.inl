/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/kernel_info.h"
#include "shared/test/common/cmd_parse/cmd_parse_3d_state_btd.inl"
#include "shared/test/common/cmd_parse/cmd_parse_base.inl"
#include "shared/test/common/cmd_parse/cmd_parse_compute_mi_arb.inl"
#include "shared/test/common/cmd_parse/cmd_parse_compute_mode.inl"
#include "shared/test/common/cmd_parse/cmd_parse_compute_walker.inl"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.inl"

#include "gtest/gtest.h"

using STATE_SIP = GenStruct::STATE_SIP;

template <>
STATE_SIP *genCmdCast<STATE_SIP *>(void *buffer) {
    auto pCmd = reinterpret_cast<STATE_SIP *>(buffer);

    return STATE_SIP::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   STATE_SIP::COMMAND_SUBTYPE_GFXPIPE_COMMON == pCmd->TheStructure.Common.CommandSubtype &&
                   STATE_SIP::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED == pCmd->TheStructure.Common._3DCommandOpcode &&
                   STATE_SIP::_3D_COMMAND_SUB_OPCODE_STATE_SIP == pCmd->TheStructure.Common._3DCommandSubOpcode
               ? pCmd
               : nullptr;
}

template <>
XY_BLOCK_COPY_BLT *genCmdCast<XY_BLOCK_COPY_BLT *>(void *buffer) {
    auto pCmd = reinterpret_cast<XY_BLOCK_COPY_BLT *>(buffer);

    return XY_BLOCK_COPY_BLT::INSTRUCTIONTARGET_OPCODE_OPCODE == pCmd->TheStructure.Common.InstructionTarget_Opcode &&
                   XY_BLOCK_COPY_BLT::CLIENT_2D_PROCESSOR == pCmd->TheStructure.Common.Client
               ? pCmd
               : nullptr;
}

template <>
size_t CmdParse<GenGfxFamily>::getCommandLengthHwSpecific(void *cmd) {
    {
        auto pCmd = genCmdCast<STATE_COMPUTE_MODE *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<COMPUTE_WALKER *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<CFE_STATE *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<_3DSTATE_BINDING_TABLE_POOL_ALLOC *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<MI_SET_PREDICATE *>(cmd);
        if (pCmd)
            return 1;
    }
    {
        auto pCmd = genCmdCast<_3DSTATE_BTD *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<STATE_SIP *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<XY_BLOCK_COPY_BLT *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }

    return 0;
}

template <>
const char *CmdParse<GenGfxFamily>::getCommandNameHwSpecific(void *cmd) {
    if (nullptr != genCmdCast<STATE_COMPUTE_MODE *>(cmd)) {
        return "STATE_COMPUTE_MODE";
    }

    if (nullptr != genCmdCast<COMPUTE_WALKER *>(cmd)) {
        return "COMPUTE_WALKER";
    }

    if (nullptr != genCmdCast<CFE_STATE *>(cmd)) {
        return "CFE_STATE";
    }

    if (nullptr != genCmdCast<_3DSTATE_BINDING_TABLE_POOL_ALLOC *>(cmd)) {
        return "_3DSTATE_BINDING_TABLE_POOL_ALLOC";
    }

    if (nullptr != genCmdCast<MI_SET_PREDICATE *>(cmd)) {
        return "MI_SET_PREDICATE";
    }

    if (nullptr != genCmdCast<_3DSTATE_BTD *>(cmd)) {
        return "_3DSTATE_BTD";
    }

    if (nullptr != genCmdCast<STATE_SIP *>(cmd)) {
        return "STATE_SIP";
    }
    if (genCmdCast<XY_BLOCK_COPY_BLT *>(cmd)) {
        return "XY_BLOCK_COPY_BLT";
    }

    return "UNKNOWN";
}

template struct CmdParse<GenGfxFamily>;

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

    itorMediaVfeState = find<CFE_STATE *>(itorPipelineSelect, itorWalker);
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
    if (arg.is<ArgDescriptor::ArgTPointer>() && isValidOffset(arg.as<ArgDescPointer>().stateless)) {
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
