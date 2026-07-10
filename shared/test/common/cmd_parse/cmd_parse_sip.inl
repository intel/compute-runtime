/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// clang-format off
using namespace NEO;
using STATE_SIP                       = GenStruct::STATE_SIP;
// clang-format on

template <>
STATE_SIP *genCmdCast<STATE_SIP *>(void *buffer) {
    return matchCommandHeader<STATE_SIP>(buffer, [](const STATE_SIP &header) {
        return STATE_SIP::COMMAND_TYPE_GFXPIPE == header.TheStructure.Common.CommandType &&
               STATE_SIP::COMMAND_SUBTYPE_GFXPIPE_COMMON == header.TheStructure.Common.CommandSubtype &&
               STATE_SIP::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED == header.TheStructure.Common._3DCommandOpcode &&
               STATE_SIP::_3D_COMMAND_SUB_OPCODE_STATE_SIP == header.TheStructure.Common._3DCommandSubOpcode;
    });
}
