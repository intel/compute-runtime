/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

using namespace NEO;
using STATE_SYSTEM_MEM_FENCE_ADDRESS = GenStruct::STATE_SYSTEM_MEM_FENCE_ADDRESS;

template <>
STATE_SYSTEM_MEM_FENCE_ADDRESS *genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(void *buffer) {
    auto pCmd = reinterpret_cast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(buffer);

    return (0x1 == pCmd->TheStructure.Common.DwordLength &&
            0x9 == pCmd->TheStructure.Common._3DCommandSubOpcode &&
            0x1 == pCmd->TheStructure.Common._3DCommandOpcode &&
            0x0 == pCmd->TheStructure.Common.CommandSubtype &&
            0x3 == pCmd->TheStructure.Common.CommandType)
               ? pCmd
               : nullptr;
}
