/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

using namespace NEO;
using STATE_SYSTEM_MEM_FENCE_ADDRESS = GenStruct::STATE_SYSTEM_MEM_FENCE_ADDRESS;

template <>
STATE_SYSTEM_MEM_FENCE_ADDRESS *genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(void *buffer) {
    return matchCommandHeader<STATE_SYSTEM_MEM_FENCE_ADDRESS>(buffer, [](const STATE_SYSTEM_MEM_FENCE_ADDRESS &header) {
        return 0x1 == header.TheStructure.Common.DwordLength &&
               0x9 == header.TheStructure.Common._3DCommandSubOpcode &&
               0x1 == header.TheStructure.Common._3DCommandOpcode &&
               0x0 == header.TheStructure.Common.CommandSubtype &&
               0x3 == header.TheStructure.Common.CommandType;
    });
}
