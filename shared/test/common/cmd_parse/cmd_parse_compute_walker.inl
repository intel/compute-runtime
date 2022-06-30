/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// clang-format off
using namespace NEO;
using COMPUTE_WALKER                    = GenStruct::COMPUTE_WALKER;
using CFE_STATE                         = GenStruct::CFE_STATE;
using _3DSTATE_BINDING_TABLE_POOL_ALLOC = GenStruct::_3DSTATE_BINDING_TABLE_POOL_ALLOC;
using MI_SET_PREDICATE                  = GenStruct::MI_SET_PREDICATE;
// clang-format on

template <>
COMPUTE_WALKER *genCmdCast<COMPUTE_WALKER *>(void *buffer) {
    auto pCmd = reinterpret_cast<COMPUTE_WALKER *>(buffer);

    return COMPUTE_WALKER::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   COMPUTE_WALKER::PIPELINE_COMPUTE == pCmd->TheStructure.Common.Pipeline &&
                   COMPUTE_WALKER::COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND == pCmd->TheStructure.Common.ComputeCommandOpcode &&
                   COMPUTE_WALKER::CFE_SUBOPCODE_COMPUTE_WALKER == pCmd->TheStructure.Common.CfeSubopcode
               ? pCmd
               : nullptr;
}

template <>
CFE_STATE *genCmdCast<CFE_STATE *>(void *buffer) {
    auto pCmd = reinterpret_cast<CFE_STATE *>(buffer);

    return CFE_STATE::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   CFE_STATE::PIPELINE_COMPUTE == pCmd->TheStructure.Common.Pipeline &&
                   CFE_STATE::COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND == pCmd->TheStructure.Common.ComputeCommandOpcode &&
                   CFE_STATE::CFE_SUBOPCODE_CFE_STATE == pCmd->TheStructure.Common.CfeSubopcode
               ? pCmd
               : nullptr;
}

template <>
_3DSTATE_BINDING_TABLE_POOL_ALLOC *genCmdCast<_3DSTATE_BINDING_TABLE_POOL_ALLOC *>(void *buffer) {
    auto pCmd = reinterpret_cast<_3DSTATE_BINDING_TABLE_POOL_ALLOC *>(buffer);

    return _3DSTATE_BINDING_TABLE_POOL_ALLOC::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   _3DSTATE_BINDING_TABLE_POOL_ALLOC::COMMAND_SUBTYPE_GFXPIPE_3D == pCmd->TheStructure.Common.CommandSubtype &&
                   _3DSTATE_BINDING_TABLE_POOL_ALLOC::_3D_COMMAND_OPCODE_3DSTATE_NONPIPELINED ==
                       pCmd->TheStructure.Common._3DCommandOpcode &&
                   _3DSTATE_BINDING_TABLE_POOL_ALLOC::_3D_COMMAND_SUB_OPCODE_3DSTATE_BINDING_TABLE_POOL_ALLOC ==
                       pCmd->TheStructure.Common._3DCommandSubOpcode
               ? pCmd
               : nullptr;
}

template <>
MI_SET_PREDICATE *genCmdCast<MI_SET_PREDICATE *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_SET_PREDICATE *>(buffer);

    return MI_SET_PREDICATE::COMMAND_TYPE_MI_COMMAND == pCmd->TheStructure.Common.CommandType &&
                   MI_SET_PREDICATE::MI_COMMAND_OPCODE_MI_SET_PREDICATE == pCmd->TheStructure.Common.MiCommandOpcode
               ? pCmd
               : nullptr;
}

template <class T>
bool CmdParse<T>::parseCommandBuffer(GenCmdList &cmds, void *buffer, size_t length) {
    if (!buffer || length % sizeof(uint32_t)) {
        return false;
    }

    void *bufferEnd = reinterpret_cast<uint8_t *>(buffer) + length;
    while (buffer < bufferEnd) {
        size_t length = getCommandLength(buffer);
        if (!length) {
            return false;
        }

        cmds.push_back(buffer);

        buffer = reinterpret_cast<uint32_t *>(buffer) + length;
    }

    return buffer == bufferEnd;
}

template <>
template <>
void CmdParse<GenGfxFamily>::validateCommand<STATE_BASE_ADDRESS *>(GenCmdList::iterator itorBegin, GenCmdList::iterator itorEnd) {
}

template <>
template <>
void CmdParse<GenGfxFamily>::validateCommand<CFE_STATE *>(GenCmdList::iterator itorBegin, GenCmdList::iterator itorEnd) {
    auto itorCurrent = itorBegin;
    auto itorWalker = itorEnd;

    // Find last COMPUTE_WALKER prior to itorCmd
    while (itorCurrent != itorEnd) {
        if (genCmdCast<COMPUTE_WALKER *>(*itorCurrent)) {
            itorWalker = itorCurrent;
        }

        ++itorCurrent;
    }

    // If we don't find a GPGPU_WALKER, assume the beginning of a cmd list
    itorWalker = itorWalker == itorEnd
                     ? itorBegin
                     : itorWalker;
}
