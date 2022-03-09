/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
struct XE_HPC_COREFamily;
using Family = XE_HPC_COREFamily;
} // namespace NEO

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/preamble_xehp_and_later.inl"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

using CFE_STATE = typename Family::CFE_STATE;
template <>
void PreambleHelper<Family>::appendProgramVFEState(const HardwareInfo &hwInfo, const StreamProperties &streamProperties, void *cmd) {
    auto command = static_cast<CFE_STATE *>(cmd);

    command->setComputeOverdispatchDisable(streamProperties.frontEndState.disableOverdispatch.value == 1);
    command->setSingleSliceDispatchCcsMode(streamProperties.frontEndState.singleSliceDispatchCcsMode.value == 1);

    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    if (hwInfoConfig.getSteppingFromHwRevId(hwInfo) >= REVISION_B) {
        const auto programComputeDispatchAllWalkerEnableInCfeState = hwInfoConfig.isComputeDispatchAllWalkerEnableInCfeStateRequired(hwInfo);
        if (programComputeDispatchAllWalkerEnableInCfeState && streamProperties.frontEndState.computeDispatchAllWalkerEnable.value > 0) {
            command->setComputeDispatchAllWalkerEnable(true);
            command->setSingleSliceDispatchCcsMode(true);
        }
    }

    if (DebugManager.flags.CFEComputeDispatchAllWalkerEnable.get() != -1) {
        command->setComputeDispatchAllWalkerEnable(DebugManager.flags.CFEComputeDispatchAllWalkerEnable.get());
    }

    if (DebugManager.flags.CFEComputeOverdispatchDisable.get() != -1) {
        command->setComputeOverdispatchDisable(DebugManager.flags.CFEComputeOverdispatchDisable.get());
    }
    if (DebugManager.flags.CFESingleSliceDispatchCCSMode.get() != -1) {
        command->setSingleSliceDispatchCcsMode(DebugManager.flags.CFESingleSliceDispatchCCSMode.get());
    }

    command->setNumberOfWalkers(1);
    if (DebugManager.flags.CFENumberOfWalkers.get() != -1) {
        command->setNumberOfWalkers(DebugManager.flags.CFENumberOfWalkers.get());
    }
}

template <>
bool PreambleHelper<Family>::isSystolicModeConfigurable(const HardwareInfo &hwInfo) {
    const auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
    return hwInfoConfig.isSystolicModeConfigurable(hwInfo);
}

template <>
bool PreambleHelper<Family>::isSpecialPipelineSelectModeChanged(bool lastSpecialPipelineSelectMode, bool newSpecialPipelineSelectMode,
                                                                const HardwareInfo &hwInfo) {

    const auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
    return (lastSpecialPipelineSelectMode != newSpecialPipelineSelectMode) && hwInfoConfig.isSpecialPipelineSelectModeChanged(hwInfo);
}

template struct PreambleHelper<Family>;

} // namespace NEO
