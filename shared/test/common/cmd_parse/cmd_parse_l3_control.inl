/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

using namespace NEO;
using L3_CONTROL_BASE = GenStruct::L3_CONTROL_BASE;
using L3_CONTROL = GenStruct::L3_CONTROL;

template <>
L3_CONTROL_BASE *genCmdCast<L3_CONTROL_BASE *>(void *buffer) {
    auto pCmd = reinterpret_cast<L3_CONTROL_BASE *>(buffer);

    return L3_CONTROL_BASE::TYPE_GFXPIPE == pCmd->TheStructure.Common.Type &&
                   L3_CONTROL_BASE::COMMAND_SUBTYPE_GFXPIPE_3D == pCmd->TheStructure.Common.CommandSubtype &&
                   L3_CONTROL_BASE::_3D_COMMAND_OPCODE_L3_CONTROL == pCmd->TheStructure.Common._3DCommandOpcode &&
                   L3_CONTROL_BASE::_3D_COMMAND_SUB_OPCODE_L3_CONTROL == pCmd->TheStructure.Common._3DCommandSubOpcode
               ? pCmd
               : nullptr;
}

template <>
L3_CONTROL *genCmdCast<L3_CONTROL *>(void *buffer) {
    auto pCmd = genCmdCast<L3_CONTROL_BASE *>(buffer);
    if (pCmd == nullptr) {
        return nullptr;
    }

    return reinterpret_cast<L3_CONTROL *>(pCmd);
}
