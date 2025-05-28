/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

using GenStruct = NEO::XeHpgCore;
using GenGfxFamily = NEO::XeHpgCoreFamily;

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

using L3_CONTROL = GenStruct::L3_CONTROL;

template <>
L3_CONTROL *genCmdCast<L3_CONTROL *>(void *buffer) {
    auto pCmd = reinterpret_cast<L3_CONTROL *>(buffer);

    return L3_CONTROL::TYPE_GFXPIPE == pCmd->TheStructure.Common.Type &&
                   L3_CONTROL::COMMAND_SUBTYPE_GFXPIPE_3D == pCmd->TheStructure.Common.CommandSubtype &&
                   L3_CONTROL::_3D_COMMAND_OPCODE_L3_CONTROL == pCmd->TheStructure.Common._3DCommandOpcode &&
                   L3_CONTROL::_3D_COMMAND_SUB_OPCODE_L3_CONTROL == pCmd->TheStructure.Common._3DCommandSubOpcode
               ? pCmd
               : nullptr;
}

template <>
size_t CmdParse<GenGfxFamily>::getAdditionalCommandLength(void *cmd) {
    {
        auto pCmd = genCmdCast<L3_CONTROL *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.Length + 2;
    }

    return 0;
}

template <>
const char *CmdParse<GenGfxFamily>::getAdditionalCommandName(void *cmd) {

    if (nullptr != genCmdCast<L3_CONTROL *>(cmd)) {
        auto l3Command = genCmdCast<L3_CONTROL *>(cmd);
        if (l3Command->getPostSyncOperation() == L3_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_NO_WRITE) {

            return "L3_CONTROL(NO_POST_SYNC)";
        } else {
            return "L3_CONTROL(POST_SYNC)";
        }
    }

    return "UNKNOWN";
}
#include "shared/test/common/cmd_parse/cmd_parse_xe_hpg_and_later.inl"
#include "shared/test/common/cmd_parse/hw_parse_base.inl"
#include "shared/test/common/cmd_parse/hw_parse_xe_hpg_and_later.inl"

template const typename GenGfxFamily::RENDER_SURFACE_STATE *NEO::HardwareParse::getSurfaceState<GenGfxFamily>(IndirectHeap *ssh, uint32_t index);
template bool NEO::HardwareParse::isStallingBarrier<GenGfxFamily>(GenCmdList::iterator &iter);