/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_options_parser.h"

namespace NEO {

bool requiresL1PolicyMissmatchCheck() {
    return true;
}

} // namespace NEO