/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hardware_commands_helper_gen12lp.inl"

#include "shared/source/gen12lp/hw_cmds.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/gen12lp/helpers_gen12lp.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/hardware_commands_helper.inl"
#include "opencl/source/helpers/hardware_commands_helper_base.inl"

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
