/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/helpers/preamble_base.inl"

#include "reg_configs_common.h"

// L3 programming:
// All L3 Client Pool: 320KB
// URB Pool: 64KB
// Use Full ways: true
// SLM: reserved (always enabled)

namespace NEO {

template <typename Family>
void PreambleHelper<Family>::programPipelineSelect(LinearStream *pCommandStream,
                                                   const PipelineSelectArgs &pipelineSelectArgs,
                                                   const HardwareInfo &hwInfo) {

    using PIPELINE_SELECT = typename Family::PIPELINE_SELECT;

    PIPELINE_SELECT cmd = Family::cmdInitPipelineSelect;

    if (DebugManager.flags.CleanStateInPreamble.get()) {
        auto cmdBuffer = pCommandStream->getSpaceForCmd<PIPELINE_SELECT>();
        cmd.setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_3D);
        *cmdBuffer = cmd;

        PipeControlArgs args = {};
        args.stateCacheInvalidationEnable = true;
        MemorySynchronizationCommands<Family>::addSingleBarrier(*pCommandStream, args);
    }

    auto cmdBuffer = pCommandStream->getSpaceForCmd<PIPELINE_SELECT>();

    auto mask = pipelineSelectEnablePipelineSelectMaskBits;

    cmd.setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
    if constexpr (Family::isUsingMediaSamplerDopClockGate) {
        mask |= pipelineSelectMediaSamplerDopClockGateMaskBits;
        cmd.setMediaSamplerDopClockGateEnable(!pipelineSelectArgs.mediaSamplerRequired);
    }

    bool systolicSupport = pipelineSelectArgs.systolicPipelineSelectSupport;
    bool systolicValue = pipelineSelectArgs.systolicPipelineSelectMode;
    int32_t overrideSystolic = DebugManager.flags.OverrideSystolicPipelineSelect.get();

    if (overrideSystolic != -1) {
        systolicSupport = true;
        systolicValue = !!overrideSystolic;
    }

    if (systolicSupport) {
        cmd.setSystolicModeEnable(systolicValue);
        mask |= pipelineSelectSystolicModeEnableMaskBits;
    }

    cmd.setMaskBits(mask);

    *cmdBuffer = cmd;

    if (DebugManager.flags.CleanStateInPreamble.get()) {
        PipeControlArgs args = {};
        args.stateCacheInvalidationEnable = true;
        MemorySynchronizationCommands<Family>::addSingleBarrier(*pCommandStream, args);
    }
}

template <>
void PreambleHelper<Family>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo, EngineGroupType engineGroupType) {
}

template <>
void PreambleHelper<Family>::programL3(LinearStream *pCommandStream, uint32_t l3Config) {
}

template <>
uint32_t PreambleHelper<Family>::getUrbEntryAllocationSize() {
    return 0u;
}
template <>
void PreambleHelper<Family>::appendProgramVFEState(const HardwareInfo &hwInfo, const StreamProperties &streamProperties, void *cmd);

template <typename GfxFamily>
void *PreambleHelper<GfxFamily>::getSpaceForVfeState(LinearStream *pCommandStream,
                                                     const HardwareInfo &hwInfo,
                                                     EngineGroupType engineGroupType) {
    using CFE_STATE = typename Family::CFE_STATE;
    return pCommandStream->getSpace(sizeof(CFE_STATE));
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programVfeState(void *pVfeState,
                                                const HardwareInfo &hwInfo,
                                                uint32_t scratchSize,
                                                uint64_t scratchAddress,
                                                uint32_t maxFrontEndThreads,
                                                const StreamProperties &streamProperties,
                                                LogicalStateHelper *logicalStateHelper) {
    using CFE_STATE = typename Family::CFE_STATE;

    auto cfeState = reinterpret_cast<CFE_STATE *>(pVfeState);
    CFE_STATE cmd = Family::cmdInitCfeState;

    uint32_t lowAddress = uint32_t(0xFFFFFFFF & scratchAddress);
    cmd.setScratchSpaceBuffer(lowAddress);
    cmd.setMaximumNumberOfThreads(maxFrontEndThreads);
    appendProgramVFEState(hwInfo, streamProperties, &cmd);

    if (DebugManager.flags.CFEMaximumNumberOfThreads.get() != -1) {
        cmd.setMaximumNumberOfThreads(DebugManager.flags.CFEMaximumNumberOfThreads.get());
    }
    if (DebugManager.flags.CFEOverDispatchControl.get() != -1) {
        cmd.setOverDispatchControl(static_cast<typename CFE_STATE::OVER_DISPATCH_CONTROL>(DebugManager.flags.CFEOverDispatchControl.get()));
    }
    if (DebugManager.flags.CFELargeGRFThreadAdjustDisable.get() != -1) {
        cmd.setLargeGRFThreadAdjustDisable(DebugManager.flags.CFELargeGRFThreadAdjustDisable.get());
    }

    *cfeState = cmd;
}

template <>
uint64_t PreambleHelper<Family>::getScratchSpaceAddressOffsetForVfeState(LinearStream *pCommandStream, void *pVfeState) {
    return 0;
}

template <typename GfxFamily>
size_t PreambleHelper<GfxFamily>::getVFECommandsSize() {
    using CFE_STATE = typename Family::CFE_STATE;
    return sizeof(CFE_STATE);
}

template <>
uint32_t PreambleHelper<Family>::getL3Config(const HardwareInfo &hwInfo, bool useSLM) {
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
