/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/usm_pool_params.h"

#include "shared/source/helpers/constants.h"

namespace NEO {
UsmPoolParams UsmPoolParams::getUsmHostPoolParams() {
    return {
        .poolSize = 2 * MemoryConstants::megaByte,
        .minServicedSize = 0u,
        .maxServicedSize = 1 * MemoryConstants::megaByte};
}
} // namespace NEO
