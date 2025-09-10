/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "cmdlist_memory_copy_params_ext.h"

namespace L0 {
struct CmdListMemoryCopyParams {
    bool relaxedOrderingDispatch = false;
    bool forceDisableCopyOnlyInOrderSignaling = false;
    bool copyOffloadAllowed = false;
    bool taskCountUpdateRequired = false;
    CmdListMemoryCopyParamsExt paramsExt{};
};

} // namespace L0
