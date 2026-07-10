/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/cmd_parse_compute_walker_no_heap.inl"

// clang-format off
using namespace NEO;
using COMPUTE_WALKER                    = GenStruct::COMPUTE_WALKER;
using CFE_STATE                         = GenStruct::CFE_STATE;
using _3DSTATE_BINDING_TABLE_POOL_ALLOC = GenStruct::_3DSTATE_BINDING_TABLE_POOL_ALLOC;
// clang-format on

template <>
COMPUTE_WALKER *genCmdCast<COMPUTE_WALKER *>(void *buffer) {
    return matchCommandHeader<COMPUTE_WALKER>(buffer, [](const COMPUTE_WALKER &header) {
        return COMPUTE_WALKER::COMMAND_TYPE_GFXPIPE == header.TheStructure.Common.CommandType &&
               COMPUTE_WALKER::PIPELINE_COMPUTE == header.TheStructure.Common.Pipeline &&
               COMPUTE_WALKER::COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND == header.TheStructure.Common.ComputeCommandOpcode &&
               COMPUTE_WALKER::CFE_SUBOPCODE_COMPUTE_WALKER == header.TheStructure.Common.CfeSubopcode;
    });
}

template <>
CFE_STATE *genCmdCast<CFE_STATE *>(void *buffer) {
    return matchCommandHeader<CFE_STATE>(buffer, [](const CFE_STATE &header) {
        return CFE_STATE::COMMAND_TYPE_GFXPIPE == header.TheStructure.Common.CommandType &&
               CFE_STATE::PIPELINE_COMPUTE == header.TheStructure.Common.Pipeline &&
               CFE_STATE::COMPUTE_COMMAND_OPCODE_NEW_CFE_COMMAND == header.TheStructure.Common.ComputeCommandOpcode &&
               CFE_STATE::CFE_SUBOPCODE_CFE_STATE == header.TheStructure.Common.CfeSubopcode;
    });
}

template <>
_3DSTATE_BINDING_TABLE_POOL_ALLOC *genCmdCast<_3DSTATE_BINDING_TABLE_POOL_ALLOC *>(void *buffer) {
    return matchCommandHeader<_3DSTATE_BINDING_TABLE_POOL_ALLOC>(buffer, [](const _3DSTATE_BINDING_TABLE_POOL_ALLOC &header) {
        return _3DSTATE_BINDING_TABLE_POOL_ALLOC::COMMAND_TYPE_GFXPIPE == header.TheStructure.Common.CommandType &&
               _3DSTATE_BINDING_TABLE_POOL_ALLOC::COMMAND_SUBTYPE_GFXPIPE_3D == header.TheStructure.Common.CommandSubtype &&
               _3DSTATE_BINDING_TABLE_POOL_ALLOC::_3D_COMMAND_OPCODE_3DSTATE_NONPIPELINED ==
                   header.TheStructure.Common._3DCommandOpcode &&
               _3DSTATE_BINDING_TABLE_POOL_ALLOC::_3D_COMMAND_SUB_OPCODE_3DSTATE_BINDING_TABLE_POOL_ALLOC ==
                   header.TheStructure.Common._3DCommandSubOpcode;
    });
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
