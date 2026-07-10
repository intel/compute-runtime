/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

using namespace NEO;
using _3DSTATE_BTD = GenStruct::_3DSTATE_BTD;

template <>
_3DSTATE_BTD *genCmdCast<_3DSTATE_BTD *>(void *buffer) {
    return matchCommandHeader<_3DSTATE_BTD>(buffer, [](const _3DSTATE_BTD &header) {
        return _3DSTATE_BTD::COMMAND_TYPE_GFXPIPE == header.TheStructure.Common.CommandType &&
               _3DSTATE_BTD::COMMAND_SUBTYPE_GFXPIPE_COMMON == header.TheStructure.Common.CommandSubtype &&
               _3DSTATE_BTD::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED == header.TheStructure.Common._3DCommandOpcode &&
               _3DSTATE_BTD::_3D_COMMAND_SUB_OPCODE_3DSTATE_BTD == header.TheStructure.Common._3DCommandSubOpcode;
    });
}
