/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/relaxed_ordering_helper.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"

namespace NEO {
namespace RelaxedOrderingHelper {

bool isRelaxedOrderingDispatchAllowed(const CommandStreamReceiver &csr, uint32_t numWaitEvents) {
    if (numWaitEvents == 0u) {
        return false;
    }

    uint32_t minimalNumberOfClients = 2;
    if (debugManager.flags.DirectSubmissionRelaxedOrderingMinNumberOfClients.get() != -1) {
        minimalNumberOfClients = static_cast<uint32_t>(debugManager.flags.DirectSubmissionRelaxedOrderingMinNumberOfClients.get());
    }

    return (csr.directSubmissionRelaxedOrderingEnabled() && csr.getNumClients() >= minimalNumberOfClients);
}

} // namespace RelaxedOrderingHelper
} // namespace NEO