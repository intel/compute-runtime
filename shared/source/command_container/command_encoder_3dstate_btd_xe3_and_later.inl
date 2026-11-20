/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/debug_settings/debug_settings_manager.h"

namespace NEO {

template <typename Family>
void EncodeEnableRayTracing<Family>::append3dStateBtd(void *ptr3dStateBtd) {
    using _3DSTATE_BTD = typename Family::_3DSTATE_BTD;
    using DISPATCH_TIMEOUT_COUNTER = typename Family::_3DSTATE_BTD::DISPATCH_TIMEOUT_COUNTER;
    using CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS = typename Family::_3DSTATE_BTD::CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS;
    auto cmd = static_cast<_3DSTATE_BTD *>(ptr3dStateBtd);
    if (debugManager.flags.ForceTheMaximumNumberOfOutstandingRayqueriesPerSs.get() != -1) {
        auto value = static_cast<CONTROLS_THE_MAXIMUM_NUMBER_OF_OUTSTANDING_RAYQUERIES_PER_SS>(debugManager.flags.ForceTheMaximumNumberOfOutstandingRayqueriesPerSs.get());
        DEBUG_BREAK_IF(value > 3);
        cmd->setControlsTheMaximumNumberOfOutstandingRayqueriesPerSs(value);
    }
    if (debugManager.flags.ForceDispatchTimeoutCounter.get() != -1) {
        auto value = static_cast<DISPATCH_TIMEOUT_COUNTER>(debugManager.flags.ForceDispatchTimeoutCounter.get());
        DEBUG_BREAK_IF(value > 7);
        cmd->setDispatchTimeoutCounter(value);
    }

    cmd->setRtMemStructures64BModeEnable(!is48bResourceNeededForRayTracing());
}

} // namespace NEO
