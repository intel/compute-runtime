/*
 * Copyright (C) 2018-2024 Intel Corporation
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
    auto pCmd = reinterpret_cast<STATE_SIP *>(buffer);

    return STATE_SIP::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   STATE_SIP::COMMAND_SUBTYPE_GFXPIPE_COMMON == pCmd->TheStructure.Common.CommandSubtype &&
                   STATE_SIP::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED == pCmd->TheStructure.Common._3DCommandOpcode &&
                   STATE_SIP::_3D_COMMAND_SUB_OPCODE_STATE_SIP == pCmd->TheStructure.Common._3DCommandSubOpcode
               ? pCmd
               : nullptr;
}
