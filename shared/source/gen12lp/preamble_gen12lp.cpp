/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/source/helpers/flat_batch_buffer_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/helpers/preamble_base.inl"

namespace NEO {

using Family = Gen12LpFamily;

template <typename GfxFamily>
void *PreambleHelper<GfxFamily>::getSpaceForVfeState(LinearStream *pCommandStream,
                                                     const HardwareInfo &hwInfo,
                                                     EngineGroupType engineGroupType) {
    using MEDIA_VFE_STATE = typename GfxFamily::MEDIA_VFE_STATE;
    addPipeControlBeforeVfeCmd(pCommandStream, &hwInfo, engineGroupType);
    return pCommandStream->getSpaceForCmd<MEDIA_VFE_STATE>();
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programVfeState(void *pVfeState,
                                                const RootDeviceEnvironment &rootDeviceEnvironment,
                                                uint32_t scratchSize,
                                                uint64_t scratchAddress,
                                                uint32_t maxFrontEndThreads,
                                                const StreamProperties &streamProperties) {
    using MEDIA_VFE_STATE = typename GfxFamily::MEDIA_VFE_STATE;

    auto pMediaVfeState = reinterpret_cast<MEDIA_VFE_STATE *>(pVfeState);
    MEDIA_VFE_STATE cmd = GfxFamily::cmdInitMediaVfeState;
    cmd.setMaximumNumberOfThreads(maxFrontEndThreads);
    cmd.setNumberOfUrbEntries(1);
    cmd.setUrbEntryAllocationSize(PreambleHelper<GfxFamily>::getUrbEntryAllocationSize());
    cmd.setPerThreadScratchSpace(PreambleHelper<GfxFamily>::getScratchSizeValueToProgramMediaVfeState(scratchSize));
    cmd.setStackSize(PreambleHelper<GfxFamily>::getScratchSizeValueToProgramMediaVfeState(scratchSize));
    uint32_t lowAddress = static_cast<uint32_t>(0xFFFFFFFF & scratchAddress);
    uint32_t highAddress = static_cast<uint32_t>(0xFFFFFFFF & (scratchAddress >> 32));
    cmd.setScratchSpaceBasePointer(lowAddress);
    cmd.setScratchSpaceBasePointerHigh(highAddress);

    appendProgramVFEState(rootDeviceEnvironment, streamProperties, &cmd);
    *pMediaVfeState = cmd;
}

template <typename GfxFamily>
uint64_t PreambleHelper<GfxFamily>::getScratchSpaceAddressOffsetForVfeState(LinearStream *pCommandStream, void *pVfeState) {
    using MEDIA_VFE_STATE = typename GfxFamily::MEDIA_VFE_STATE;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pVfeState) -
                                 reinterpret_cast<uintptr_t>(pCommandStream->getCpuBase()) +
                                 MEDIA_VFE_STATE::PATCH_CONSTANTS::SCRATCHSPACEBASEPOINTER_BYTEOFFSET);
}

template <typename GfxFamily>
size_t PreambleHelper<GfxFamily>::getVFECommandsSize() {
    using MEDIA_VFE_STATE = typename GfxFamily::MEDIA_VFE_STATE;
    return sizeof(MEDIA_VFE_STATE) + sizeof(PIPE_CONTROL);
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::setSingleSliceDispatchMode(void *cmd, bool enable) {
}

template <>
uint32_t PreambleHelper<Family>::getL3Config(const HardwareInfo &hwInfo, bool useSLM) {
    uint32_t l3Config = 0;

    switch (hwInfo.platform.eProductFamily) {
    case IGFX_TIGERLAKE_LP:
        l3Config = getL3ConfigHelper<IGFX_TIGERLAKE_LP>(useSLM);
        break;
    default:
        l3Config = getL3ConfigHelper<IGFX_TIGERLAKE_LP>(true);
    }
    return l3Config;
}

template <>
void PreambleHelper<Family>::programPipelineSelect(LinearStream *pCommandStream,
                                                   const PipelineSelectArgs &pipelineSelectArgs,
                                                   const RootDeviceEnvironment &rootDeviceEnvironment) {

    using PIPELINE_SELECT = typename Family::PIPELINE_SELECT;

    if (MemorySynchronizationCommands<Family>::isBarrierPriorToPipelineSelectWaRequired(rootDeviceEnvironment)) {
        PipeControlArgs args;
        args.renderTargetCacheFlushEnable = true;
        MemorySynchronizationCommands<Family>::addSingleBarrier(*pCommandStream, args);
    }

    auto cmdSpace = pCommandStream->getSpaceForCmd<PIPELINE_SELECT>();
    PIPELINE_SELECT pipelineSelectCmd = Family::cmdInitPipelineSelect;

    auto mask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    auto pipeline = pipelineSelectArgs.is3DPipelineRequired ? PIPELINE_SELECT::PIPELINE_SELECTION_3D : PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU;

    pipelineSelectCmd.setPipelineSelection(pipeline);
    pipelineSelectCmd.setMediaSamplerDopClockGateEnable(!pipelineSelectArgs.mediaSamplerRequired);

    if (pipelineSelectArgs.systolicPipelineSelectSupport) {
        mask |= pipelineSelectSystolicModeEnableMaskBits;
        pipelineSelectCmd.setSpecialModeEnable(pipelineSelectArgs.systolicPipelineSelectMode);
    }

    pipelineSelectCmd.setMaskBits(mask);

    *cmdSpace = pipelineSelectCmd;
}

template <>
size_t PreambleHelper<Family>::getCmdSizeForPipelineSelect(const RootDeviceEnvironment &rootDeviceEnvironment) {
    size_t size = 0;
    using PIPELINE_SELECT = typename Family::PIPELINE_SELECT;
    size += sizeof(PIPELINE_SELECT);
    if (MemorySynchronizationCommands<Family>::isBarrierPriorToPipelineSelectWaRequired(rootDeviceEnvironment)) {
        size += sizeof(PIPE_CONTROL);
    }
    return size;
}

template <>
void PreambleHelper<Family>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo, EngineGroupType engineGroupType) {
    PipeControlArgs args = {};
    if (hwInfo->workaroundTable.flags.waSendMIFLUSHBeforeVFE) {
        if (engineGroupType != EngineGroupType::compute) {
            args.renderTargetCacheFlushEnable = true;
            args.depthCacheFlushEnable = true;
            args.depthStallEnable = true;
        }
        args.dcFlushEnable = true;
    }

    MemorySynchronizationCommands<Family>::addSingleBarrier(*pCommandStream, args);
}

template <>
void PreambleHelper<Family>::programL3(LinearStream *pCommandStream, uint32_t l3Config, bool isBcs) {
}

template <>
uint32_t PreambleHelper<Family>::getUrbEntryAllocationSize() {
    return 1024u;
}

template <>
void PreambleHelper<Family>::appendProgramVFEState(const RootDeviceEnvironment &rootDeviceEnvironment, const StreamProperties &streamProperties, void *cmd) {
    using FrontEndStateCommand = typename Family::FrontEndStateCommand;
    FrontEndStateCommand *mediaVfeState = static_cast<FrontEndStateCommand *>(cmd);
    bool disableEUFusion = streamProperties.frontEndState.disableEUFusion.value == 1;
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    if (!gfxCoreHelper.isFusedEuDispatchEnabled(hwInfo, disableEUFusion)) {
        mediaVfeState->setDisableSlice0Subslice2(true);
    }
    if (debugManager.flags.MediaVfeStateMaxSubSlices.get() != -1) {
        mediaVfeState->setMaximumNumberOfDualSubslices(debugManager.flags.MediaVfeStateMaxSubSlices.get());
    }
}

// Explicitly instantiate PreambleHelper for Gen12Lp device family
template struct PreambleHelper<Family>;
} // namespace NEO
