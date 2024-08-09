/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

using namespace NEO;
using MI_MEM_FENCE = GenStruct::MI_MEM_FENCE;

template <>
MI_MEM_FENCE *genCmdCast<MI_MEM_FENCE *>(void *buffer) {
    auto pCmd = reinterpret_cast<MI_MEM_FENCE *>(buffer);

    return (0x0 == pCmd->TheStructure.Common.MiCommandSubOpcode &&
            0x9 == pCmd->TheStructure.Common.MiCommandOpcode &&
            0x0 == pCmd->TheStructure.Common.CommandType)
               ? pCmd
               : nullptr;
}
