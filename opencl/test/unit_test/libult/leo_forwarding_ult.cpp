/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/ult_hw_config.h"

#include "opencl/source/api/leo_forwarding.h"

namespace NEO {

bool leoForwardingSelfLoad() {
    return ultHwConfig.leoForwardingSelfLoad;
}

} // namespace NEO
