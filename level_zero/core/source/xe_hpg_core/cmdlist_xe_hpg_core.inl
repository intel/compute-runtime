/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"

namespace L0 {
template <PRODUCT_FAMILY productFamily>
void CommandListProductFamily<productFamily>::clearComputeModePropertiesIfNeeded(bool requiresCoherency, uint32_t numGrfRequired, uint32_t threadArbitrationPolicy) {
    CommandListCoreFamily<IGFX_XE_HPG_CORE>::clearComputeModePropertiesIfNeeded(requiresCoherency, numGrfRequired, threadArbitrationPolicy);
}
} // namespace L0