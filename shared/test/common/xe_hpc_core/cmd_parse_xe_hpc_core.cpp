/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds.h"
using GenStruct = NEO::XE_HPC_CORE;
using GenGfxFamily = NEO::XE_HPC_COREFamily;

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

using MI_MEM_FENCE = GenStruct::MI_MEM_FENCE;
using STATE_SYSTEM_MEM_FENCE_ADDRESS = GenStruct::STATE_SYSTEM_MEM_FENCE_ADDRESS;
using STATE_PREFETCH = GenStruct::STATE_PREFETCH;
using MEM_SET = GenStruct::MEM_SET;

template <>
MI_MEM_FENCE *genCmdCast<MI_MEM_FENCE *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_MEM_FENCE *>(buffer);

    return (0x0 == pCmd->TheStructure.Common.MiCommandSubOpcode &&
            0x9 == pCmd->TheStructure.Common.MiCommandOpcode &&
            0x0 == pCmd->TheStructure.Common.CommandType)
               ? pCmd
               : nullptr;
}

template <>
STATE_SYSTEM_MEM_FENCE_ADDRESS *genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(void *buffer) {
    auto pCmd = reinterpret_cast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(buffer);

    return (0x1 == pCmd->TheStructure.Common.DwordLength &&
            0x9 == pCmd->TheStructure.Common._3DCommandSubOpcode &&
            0x1 == pCmd->TheStructure.Common._3DCommandOpcode &&
            0x0 == pCmd->TheStructure.Common.CommandSubType &&
            0x3 == pCmd->TheStructure.Common.CommandType)
               ? pCmd
               : nullptr;
}

template <>
STATE_PREFETCH *genCmdCast<STATE_PREFETCH *>(void *buffer) {
    auto pCmd = reinterpret_cast<STATE_PREFETCH *>(buffer);

    return (0x2 == pCmd->TheStructure.Common.DwordLength &&
            0x3 == pCmd->TheStructure.Common._3dCommandSubOpcode &&
            0x0 == pCmd->TheStructure.Common._3dCommandOpcode &&
            0x0 == pCmd->TheStructure.Common.CommandSubType &&
            0x3 == pCmd->TheStructure.Common.CommandType)
               ? pCmd
               : nullptr;
}

template <>
MEM_SET *genCmdCast<MEM_SET *>(void *buffer) {
    auto pCmd = reinterpret_cast<MEM_SET *>(buffer);

    return (0x5 == pCmd->TheStructure.Common.DwordLength &&
            0x5B == pCmd->TheStructure.Common.InstructionTarget_Opcode &&
            0x2 == pCmd->TheStructure.Common.Client)
               ? pCmd
               : nullptr;
}

template <>
size_t CmdParse<GenGfxFamily>::getAdditionalCommandLength(void *cmd) {
    {
        if (genCmdCast<MI_MEM_FENCE *>(cmd)) {
            return sizeof(MI_MEM_FENCE) / sizeof(uint32_t);
        }
    }
    {
        if (genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(cmd)) {
            return sizeof(STATE_SYSTEM_MEM_FENCE_ADDRESS) / sizeof(uint32_t);
        }
    }
    {
        if (genCmdCast<STATE_PREFETCH *>(cmd)) {
            return sizeof(STATE_PREFETCH) / sizeof(uint32_t);
        }
    }
    {
        if (genCmdCast<MEM_SET *>(cmd)) {
            return sizeof(MEM_SET) / sizeof(uint32_t);
        }
    }

    return 0;
}

template <>
const char *CmdParse<GenGfxFamily>::getAdditionalCommandName(void *cmd) {

    if (genCmdCast<MI_MEM_FENCE *>(cmd)) {
        return "MI_MEM_FENCE";
    }
    if (genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(cmd)) {
        return "STATE_SYSTEM_MEM_FENCE_ADDRESS";
    }
    if (genCmdCast<STATE_PREFETCH *>(cmd)) {
        return "STATE_PREFETCH";
    }
    if (genCmdCast<MEM_SET *>(cmd)) {
        return "MEM_SET";
    }

    return "UNKNOWN";
}

#include "shared/test/common/cmd_parse/cmd_parse_pvc_and_later.inl"
