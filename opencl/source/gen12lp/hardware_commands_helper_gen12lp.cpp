/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hardware_commands_helper_gen12lp.inl"

#include "shared/source/gen12lp/helpers_gen12lp.h"
#include "shared/source/gen12lp/hw_cmds.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/hardware_commands_helper_base.inl"
#include "opencl/source/helpers/hardware_commands_helper_bdw_plus.inl"

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
bool HardwareCommandsHelper<TGLLPFamily>::isWorkaroundRequired(uint32_t lowestSteppingWithBug, uint32_t steppingWithFix, const HardwareInfo &hwInfo) {
    if (hwInfo.platform.eProductFamily == PRODUCT_FAMILY::IGFX_TIGERLAKE_LP) {
        for (auto stepping : {&lowestSteppingWithBug, &steppingWithFix}) {
            switch (*stepping) {
            case REVISION_A0:
                *stepping = 0x0;
                break;
            case REVISION_B:
                *stepping = 0x1;
                break;
            case REVISION_C:
                *stepping = 0x3;
                break;
            default:
                DEBUG_BREAK_IF(true);
                return false;
            }
        }
        return (lowestSteppingWithBug <= hwInfo.platform.usRevId && hwInfo.platform.usRevId < steppingWithFix);
    }
    return Gen12LPHelpers::workaroundRequired(lowestSteppingWithBug, steppingWithFix, hwInfo);
}

template <>
bool HardwareCommandsHelper<TGLLPFamily>::isPipeControlWArequired(const HardwareInfo &hwInfo) {
    return (Gen12LPHelpers::pipeControlWaRequired(hwInfo.platform.eProductFamily)) && HardwareCommandsHelper<TGLLPFamily>::isWorkaroundRequired(REVISION_A0, REVISION_B, hwInfo);
}

template <>
bool HardwareCommandsHelper<TGLLPFamily>::isPipeControlPriorToPipelineSelectWArequired(const HardwareInfo &hwInfo) {
    return HardwareCommandsHelper<TGLLPFamily>::isPipeControlWArequired(hwInfo);
}

template struct HardwareCommandsHelper<TGLLPFamily>;
} // namespace NEO
