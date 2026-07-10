/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

using namespace NEO;
using STATE_CONTEXT_DATA_BASE_ADDRESS = GenStruct::STATE_CONTEXT_DATA_BASE_ADDRESS;

template <>
STATE_CONTEXT_DATA_BASE_ADDRESS *genCmdCast<STATE_CONTEXT_DATA_BASE_ADDRESS *>(void *buffer) {
    return matchCommandHeader<STATE_CONTEXT_DATA_BASE_ADDRESS>(buffer, [](const STATE_CONTEXT_DATA_BASE_ADDRESS &header) {
        return 0x1 == header.TheStructure.Common.DwordLength &&
               0xb == header.TheStructure.Common._3DCommandSubOpcode &&
               0x1 == header.TheStructure.Common._3DCommandOpcode &&
               0x0 == header.TheStructure.Common.CommandSubtype &&
               0x3 == header.TheStructure.Common.CommandType;
    });
}
