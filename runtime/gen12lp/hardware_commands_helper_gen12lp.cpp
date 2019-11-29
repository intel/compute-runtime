/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hardware_commands_helper_gen12lp.inl"

#include "core/gen12lp/hw_cmds.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/gen12lp/helpers_gen12lp.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/helpers/hardware_commands_helper.inl"
#include "runtime/helpers/hardware_commands_helper_base.inl"

namespace NEO {

template <>
size_t HardwareCommandsHelper<TGLLPFamily>::getSizeRequiredCS(const Kernel *kernel) {
    size_t size = 2 * sizeof(typename TGLLPFamily::MEDIA_STATE_FLUSH) +
                  sizeof(typename TGLLPFamily::MEDIA_INTERFACE_DESCRIPTOR_LOAD);
    return size;
}

template <>
bool HardwareCommandsHelper<TGLLPFamily>::doBindingTablePrefetch() {
    return false;
}

template <>
bool HardwareCommandsHelper<TGLLPFamily>::isPipeControlWArequired(const HardwareInfo &hwInfo) {
    return (Gen12LPHelpers::pipeControlWaRequired(hwInfo.platform.eProductFamily)) && (hwInfo.platform.usRevId == REVISION_A0);
}

template <>
bool HardwareCommandsHelper<TGLLPFamily>::isPipeControlPriorToPipelineSelectWArequired(const HardwareInfo &hwInfo) {
    return (Gen12LPHelpers::pipeControlWaRequired(hwInfo.platform.eProductFamily)) && (hwInfo.platform.usRevId == REVISION_A0);
}

template struct HardwareCommandsHelper<TGLLPFamily>;
} // namespace NEO
