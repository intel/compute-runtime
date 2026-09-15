/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/release_helper/release_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

namespace NEO {

template <>
bool ReleaseHelperHw<release>::isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequired(const HardwareInfo &hwInfo, bool isRcs) const {
    if (debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.get() != -1) {
        return debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.get();
    }
    return hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled > 1 && !isRcs;
}

} // namespace NEO
