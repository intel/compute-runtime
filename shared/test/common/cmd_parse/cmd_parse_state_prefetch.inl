/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

using namespace NEO;
using STATE_PREFETCH = GenStruct::STATE_PREFETCH;

template <>
STATE_PREFETCH *genCmdCast<STATE_PREFETCH *>(void *buffer) {
    auto pCmd = reinterpret_cast<STATE_PREFETCH *>(buffer);

    return (0x2 == pCmd->TheStructure.Common.DwordLength &&
            0x3 == pCmd->TheStructure.Common._3DCommandSubOpcode &&
            0x0 == pCmd->TheStructure.Common._3DCommandOpcode &&
            0x0 == pCmd->TheStructure.Common.CommandSubtype &&
            0x3 == pCmd->TheStructure.Common.CommandType)
               ? pCmd
               : nullptr;
}
