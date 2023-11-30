/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/helpers/preamble_base.inl"

#include "reg_configs_common.h"

namespace NEO {

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo, EngineGroupType engineGroupType) {
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programL3(LinearStream *pCommandStream, uint32_t l3Config) {
}

template <typename GfxFamily>
uint32_t PreambleHelper<GfxFamily>::getUrbEntryAllocationSize() {
    return 0u;
}

template <typename GfxFamily>
void *PreambleHelper<GfxFamily>::getSpaceForVfeState(LinearStream *pCommandStream,
                                                     const HardwareInfo &hwInfo,
                                                     EngineGroupType engineGroupType) {
    using CFE_STATE = typename Family::CFE_STATE;
    return pCommandStream->getSpace(sizeof(CFE_STATE));
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programVfeState(void *pVfeState,
                                                const RootDeviceEnvironment &rootDeviceEnvironment,
                                                uint32_t scratchSize,
                                                uint64_t scratchAddress,
                                                uint32_t maxFrontEndThreads,
                                                const StreamProperties &streamProperties) {
    using CFE_STATE = typename Family::CFE_STATE;

    auto cfeState = reinterpret_cast<CFE_STATE *>(pVfeState);
    CFE_STATE cmd = Family::cmdInitCfeState;

    uint32_t lowAddress = uint32_t(0xFFFFFFFF & scratchAddress);
    cmd.setScratchSpaceBuffer(lowAddress);
    cmd.setMaximumNumberOfThreads(maxFrontEndThreads);
    appendProgramVFEState(rootDeviceEnvironment, streamProperties, &cmd);

    if (debugManager.flags.CFEMaximumNumberOfThreads.get() != -1) {
        cmd.setMaximumNumberOfThreads(debugManager.flags.CFEMaximumNumberOfThreads.get());
    }
    if (debugManager.flags.CFEOverDispatchControl.get() != -1) {
        cmd.setOverDispatchControl(static_cast<typename CFE_STATE::OVER_DISPATCH_CONTROL>(debugManager.flags.CFEOverDispatchControl.get()));
    }
    if (debugManager.flags.CFELargeGRFThreadAdjustDisable.get() != -1) {
        cmd.setLargeGRFThreadAdjustDisable(debugManager.flags.CFELargeGRFThreadAdjustDisable.get());
    }

    *cfeState = cmd;
}

template <typename GfxFamily>
uint64_t PreambleHelper<GfxFamily>::getScratchSpaceAddressOffsetForVfeState(LinearStream *pCommandStream, void *pVfeState) {
    return 0;
}

template <typename GfxFamily>
size_t PreambleHelper<GfxFamily>::getVFECommandsSize() {
    using CFE_STATE = typename Family::CFE_STATE;
    return sizeof(CFE_STATE);
}

template <typename GfxFamily>
uint32_t PreambleHelper<GfxFamily>::getL3Config(const HardwareInfo &hwInfo, bool useSLM) {
    return 0u;
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
