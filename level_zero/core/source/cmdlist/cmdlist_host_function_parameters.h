/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/cmdlist/cmdlist_wait_parameters.h"

namespace L0 {
struct CmdListHostFunctionParameters {
    CmdListWaitEventParameters waitEventParams{};
    bool memorySynchronizationRequired = true;
};

} // namespace L0
