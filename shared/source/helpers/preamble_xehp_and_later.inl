/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/helpers/preamble_base.inl"

namespace NEO {

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo, EngineGroupType engineGroupType) {
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programL3(LinearStream *pCommandStream, uint32_t l3Config, bool isBcs) {
}

template <typename GfxFamily>
uint32_t PreambleHelper<GfxFamily>::getUrbEntryAllocationSize() {
    return 0u;
}

template <typename GfxFamily>
void *PreambleHelper<GfxFamily>::getSpaceForVfeState(LinearStream *pCommandStream,
                                                     const HardwareInfo &hwInfo,
                                                     EngineGroupType engineGroupType) {
    if constexpr (GfxFamily::isHeaplessRequired() == false) {
        using CFE_STATE = typename GfxFamily::CFE_STATE;
        return pCommandStream->getSpace(sizeof(CFE_STATE));
    } else {
        return nullptr;
    }
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programVfeState(void *pVfeState,
                                                const RootDeviceEnvironment &rootDeviceEnvironment,
                                                uint32_t scratchSize,
                                                uint64_t scratchAddress,
                                                uint32_t maxFrontEndThreads,
                                                const StreamProperties &streamProperties) {
    if constexpr (GfxFamily::isHeaplessRequired() == false) {
        using CFE_STATE = typename GfxFamily::CFE_STATE;

        auto cfeState = reinterpret_cast<CFE_STATE *>(pVfeState);
        CFE_STATE cmd = GfxFamily::cmdInitCfeState;

        uint32_t lowAddress = uint32_t(0xFFFFFFFF & scratchAddress);
        cmd.setScratchSpaceBuffer(lowAddress);
        cmd.setMaximumNumberOfThreads(maxFrontEndThreads);

        cmd.setComputeOverdispatchDisable(streamProperties.frontEndState.disableOverdispatch.value == 1);

        PreambleHelper<GfxFamily>::setSingleSliceDispatchMode(&cmd, streamProperties.frontEndState.singleSliceDispatchCcsMode.value == 1);

        appendProgramVFEState(rootDeviceEnvironment, streamProperties, &cmd);

        if (debugManager.flags.ComputeOverdispatchDisable.get() != -1) {
            cmd.setComputeOverdispatchDisable(debugManager.flags.ComputeOverdispatchDisable.get());
        }

        if (debugManager.flags.MaximumNumberOfThreads.get() != -1) {
            cmd.setMaximumNumberOfThreads(debugManager.flags.MaximumNumberOfThreads.get());
        }
        if (debugManager.flags.OverDispatchControl.get() != -1) {
            cmd.setOverDispatchControl(static_cast<typename CFE_STATE::OVER_DISPATCH_CONTROL>(debugManager.flags.OverDispatchControl.get()));
        }

        *cfeState = cmd;
    }
}

template <typename GfxFamily>
uint64_t PreambleHelper<GfxFamily>::getScratchSpaceAddressOffsetForVfeState(LinearStream *pCommandStream, void *pVfeState) {
    return 0;
}

template <typename GfxFamily>
size_t PreambleHelper<GfxFamily>::getVFECommandsSize() {
    if constexpr (GfxFamily::isHeaplessRequired() == false) {
        using CFE_STATE = typename GfxFamily::CFE_STATE;
        return sizeof(CFE_STATE);
    } else {
        return 0;
    }
}

template <typename GfxFamily>
uint32_t PreambleHelper<GfxFamily>::getL3Config(const HardwareInfo &hwInfo, bool useSLM) {
    return 0u;
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::setSingleSliceDispatchMode(void *cmd, bool enable) {
    if constexpr (GfxFamily::isHeaplessRequired() == false) {
        auto cfeState = reinterpret_cast<typename GfxFamily::CFE_STATE *>(cmd);

        cfeState->setSingleSliceDispatchCcsMode(enable);

        if (debugManager.flags.CFESingleSliceDispatchCCSMode.get() != -1) {
            cfeState->setSingleSliceDispatchCcsMode(debugManager.flags.CFESingleSliceDispatchCCSMode.get());
        }
    }
}

template <>
const uint32_t L3CNTLRegisterOffset<Family>::registerOffset = std::numeric_limits<uint32_t>::max();

template <>
struct DebugModeRegisterOffset<Family> {
    enum {
        registerOffset = 0x20d8,
        debugEnabledValue = (1 << 5) | (1 << 21)
    };
};

template <>
struct TdDebugControlRegisterOffset<Family> {
    enum {
        registerOffset = 0xe400,
        debugEnabledValue = (1 << 7) | (1 << 4) | (1 << 2) | (1 << 0)
    };
};

} // namespace NEO
