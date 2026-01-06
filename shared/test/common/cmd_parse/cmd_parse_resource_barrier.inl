/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

using namespace NEO;
using RESOURCE_BARRIER = GenStruct::RESOURCE_BARRIER;

template <>
RESOURCE_BARRIER *genCmdCast<RESOURCE_BARRIER *>(void *buffer) {
    auto pCmd = reinterpret_cast<RESOURCE_BARRIER *>(buffer);

    return (0x3 == pCmd->TheStructure.Common.DwordLength &&
            0x3 == pCmd->TheStructure.Common.Opcode &&
            0x5 == pCmd->TheStructure.Common.CommandType)
               ? pCmd
               : nullptr;
}
