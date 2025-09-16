/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "cmdlist_memory_copy_params_ext.h"

#include <cstddef>

namespace L0 {
struct CmdListMemoryCopyParams {
    const void *bcsSplitBaseSrcPtr = nullptr;
    void *bcsSplitBaseDstPtr = nullptr;
    size_t bcsSplitTotalSrcSize = 0;
    size_t bcsSplitTotalDstSize = 0;
    bool relaxedOrderingDispatch = false;
    bool forceDisableCopyOnlyInOrderSignaling = false;
    bool copyOffloadAllowed = false;
    bool taskCountUpdateRequired = false;
    bool bscSplitEnabled = false;
    CmdListMemoryCopyParamsExt paramsExt{};
};

} // namespace L0
