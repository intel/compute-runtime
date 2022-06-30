/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

using namespace NEO;
using _3DSTATE_BTD = GenStruct::_3DSTATE_BTD;

template <>
_3DSTATE_BTD *genCmdCast<_3DSTATE_BTD *>(void *buffer) {
    auto pCmd = reinterpret_cast<_3DSTATE_BTD *>(buffer);

    return _3DSTATE_BTD::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   _3DSTATE_BTD::COMMAND_SUBTYPE_GFXPIPE_COMMON == pCmd->TheStructure.Common.CommandSubtype &&
                   _3DSTATE_BTD::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED == pCmd->TheStructure.Common._3DCommandOpcode &&
                   _3DSTATE_BTD::_3D_COMMAND_SUB_OPCODE_3DSTATE_BTD == pCmd->TheStructure.Common._3DCommandSubOpcode
               ? pCmd
               : nullptr;
}
