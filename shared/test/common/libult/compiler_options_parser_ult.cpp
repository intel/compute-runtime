/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_options_parser.h"
#include "shared/test/common/helpers/ult_hw_config.h"

namespace NEO {

bool requiresL1PolicyMissmatchCheck() {
    return ultHwConfig.recompileKernelsWhenL1PolicyMissmatch;
}

} // namespace NEO