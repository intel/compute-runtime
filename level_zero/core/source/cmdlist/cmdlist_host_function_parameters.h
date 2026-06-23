/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace L0 {
struct CmdListHostFunctionParameters {
    bool relaxedOrderingDispatch = false;
    bool memorySynchronizationRequired = true;
};

} // namespace L0
