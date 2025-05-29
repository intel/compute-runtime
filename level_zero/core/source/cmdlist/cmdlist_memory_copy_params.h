/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace L0 {
struct CmdListMemoryCopyParams {
    bool relaxedOrderingDispatch = false;
    bool forceDisableCopyOnlyInOrderSignaling = false;
    bool copyOffloadAllowed = false;
};

} // namespace L0
