/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

using namespace NEO;
using XY_BLOCK_COPY_BLT = GenStruct::XY_BLOCK_COPY_BLT;

template <>
XY_BLOCK_COPY_BLT *genCmdCast<XY_BLOCK_COPY_BLT *>(void *buffer) {
    return matchCommandHeader<XY_BLOCK_COPY_BLT>(buffer, [](const XY_BLOCK_COPY_BLT &header) {
        return XY_BLOCK_COPY_BLT::INSTRUCTIONTARGET_OPCODE_OPCODE == header.TheStructure.Common.InstructionTarget_Opcode &&
               XY_BLOCK_COPY_BLT::CLIENT_2D_PROCESSOR == header.TheStructure.Common.Client;
    });
}
