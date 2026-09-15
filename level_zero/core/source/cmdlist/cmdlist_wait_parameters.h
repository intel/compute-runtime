/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/cmdlist/command_to_patch.h"

namespace L0 {

struct CmdListWaitEventParameters {
    CommandToPatchContainer *outWaitCmds = nullptr;
    bool relaxedOrderingAllowed = false;
    bool trackDependencies = false;
    bool waitForImplicitInOrderDependency = false;
    bool skipAddingWaitEventsToResidency = false;
    bool dualStreamCopyOffloadOperation = false;
    bool apiRequest = false;
    bool skipFlush = false;
};

} // namespace L0
