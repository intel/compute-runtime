/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/usm_pool_params.h"

#include "shared/source/helpers/constants.h"

namespace NEO {
UsmPoolParams UsmPoolParams::getUsmPoolParams() {
    return {
        .poolSize = UsmPoolParams::getUsmPoolSize(),
        .minServicedSize = 0u,
        .maxServicedSize = 2 * MemoryConstants::megaByte};
}
} // namespace NEO
