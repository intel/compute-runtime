/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/relaxed_ordering_helper.h"
#include "shared/source/memory_manager/prefetch_manager.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_dg2_and_pvc.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_gen12lp_to_xe3.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xe_hpc_and_later.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xehp_and_later.inl"

#include "cmdlist_extended.inl"
#include "implicit_args.h"

namespace L0 {

template <>
bool CommandListCoreFamilyImmediate<IGFX_XE_HPC_CORE>::isRelaxedOrderingDispatchAllowed(uint32_t numWaitEvents, bool copyOffload) {
    const auto copyOffloadModeForOperation = getCopyOffloadModeForOperation(copyOffload);

    auto csr = getCsr(copyOffload);
    if (!csr->directSubmissionRelaxedOrderingEnabled()) {
        return false;
    }

    auto numEvents = numWaitEvents + (this->hasInOrderDependencies() ? 1 : 0);

    if (NEO::debugManager.flags.DirectSubmissionRelaxedOrderingCounterHeuristic.get() != 0) {
        uint32_t relaxedOrderingCounterThreshold = csr->getDirectSubmissionRelaxedOrderingQueueDepth();

        auto queueTaskCount = getCmdQImmediate(copyOffloadModeForOperation)->getTaskCount();
        auto csrTaskCount = csr->peekTaskCount();

        bool skipTaskCountCheck = (csrTaskCount - queueTaskCount == 1) && csr->isLatestFlushIsTaskCountUpdateOnly();

        if (NEO::debugManager.flags.DirectSubmissionRelaxedOrderingCounterHeuristicTreshold.get() != -1) {
            relaxedOrderingCounterThreshold = static_cast<uint32_t>(NEO::debugManager.flags.DirectSubmissionRelaxedOrderingCounterHeuristicTreshold.get());
        }

        if (queueTaskCount == csrTaskCount || skipTaskCountCheck) {
            relaxedOrderingCounter++;
        } else {
            // Submission from another queue. Reset counter and keep relaxed ordering allowed
            relaxedOrderingCounter = 0;
            this->keepRelaxedOrderingEnabled = true;
        }

        if (relaxedOrderingCounter > static_cast<uint64_t>(relaxedOrderingCounterThreshold)) {
            this->keepRelaxedOrderingEnabled = false;
            return false;
        }

        return (keepRelaxedOrderingEnabled && (numEvents > 0));
    }

    return NEO::RelaxedOrderingHelper::isRelaxedOrderingDispatchAllowed(*csr, numEvents);
}

template <>
bool CommandListCoreFamilyImmediate<IGFX_XE_HPC_CORE>::handleRelaxedOrderingSignaling(Event *event, bool &hasStallingCmds, bool &relaxedOrderingDispatch, ze_result_t &result) {
    bool nonWalkerSignalingHasRelaxedOrdering = false;

    if (NEO::debugManager.flags.EnableInOrderRelaxedOrderingForEventsChaining.get() != 0) {
        auto counterValueBeforeSecondCheck = this->relaxedOrderingCounter;
        nonWalkerSignalingHasRelaxedOrdering = isRelaxedOrderingDispatchAllowed(1, false);
        this->relaxedOrderingCounter = counterValueBeforeSecondCheck; // dont increment twice
    }

    if (nonWalkerSignalingHasRelaxedOrdering) {
        if (event && event->isCounterBased()) {
            event->hostEventSetValue(Event::STATE_INITIAL);
        }
        result = flushImmediate(result, true, hasStallingCmds, relaxedOrderingDispatch, NEO::AppendOperations::kernel, false, nullptr, false, nullptr, nullptr);
        NEO::RelaxedOrderingHelper::encodeRegistersBeforeDependencyCheckers<GfxFamily>(*this->commandContainer.getCommandStream(), isCopyOnly(false));
        relaxedOrderingDispatch = true;
        hasStallingCmds = hasStallingCmdsForRelaxedOrdering(1, relaxedOrderingDispatch);
    }
    return nonWalkerSignalingHasRelaxedOrdering;
}

template struct CommandListCoreFamily<IGFX_XE_HPC_CORE>;
template struct CommandListCoreFamilyImmediate<IGFX_XE_HPC_CORE>;

} // namespace L0
