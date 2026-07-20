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

/*
addEventsToCmdList(uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents, CommandToPatchContainer *outWaitCmds,
                   bool relaxedOrderingAllowed, bool trackDependencies, bool waitForImplicitInOrderDependency, bool skipAddingWaitEventsToResidency, bool dualStreamCopyOffloadOperation);

appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phEvent, CommandToPatchContainer *outWaitCmds,
                   bool relaxedOrderingAllowed, bool trackDependencies, bool apiRequest, bool skipAddingWaitEventsToResidency, bool skipFlush, bool copyOffloadOperation) = 0;

*/

} // namespace L0
