/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"

namespace L0 {
struct CmdListKernelLaunchParams;

void CommandList::setAdditionalDispatchKernelArgsFromLaunchParams(NEO::EncodeDispatchKernelArgs &dispatchKernelArgs, const CmdListKernelLaunchParams &launchParams) const {
}

} // namespace L0
