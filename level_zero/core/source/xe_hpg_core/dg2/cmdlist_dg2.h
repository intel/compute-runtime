/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"

namespace L0 {
template <>
struct CommandListImmediateProductFamily<IGFX_DG2> : public CommandListCoreFamilyImmediate<IGFX_XE_HPG_CORE> {
    using CommandListCoreFamilyImmediate::CommandListCoreFamilyImmediate;
};
template <>
struct CommandListProductFamily<IGFX_DG2> : public CommandListCoreFamily<IGFX_XE_HPG_CORE> {
    using CommandListCoreFamily::CommandListCoreFamily;
    void clearComputeModePropertiesIfNeeded(bool requiresCoherency, uint32_t numGrfRequired, uint32_t threadArbitrationPolicy) override {
        finalStreamState.stateComputeMode = {};
        finalStreamState.stateComputeMode.setProperties(requiresCoherency, numGrfRequired, threadArbitrationPolicy);
    }
};

} // namespace L0
