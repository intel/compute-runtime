/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/csr_definitions.h"
#include "runtime/gen12lp/helpers_gen12lp.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/helpers/preamble_bdw_plus.inl"

#include "reg_configs_common.h"

namespace NEO {

template <>
uint32_t PreambleHelper<TGLLPFamily>::getL3Config(const HardwareInfo &hwInfo, bool useSLM) {
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
void PreambleHelper<TGLLPFamily>::programPipelineSelect(LinearStream *pCommandStream,
                                                        const PipelineSelectArgs &pipelineSelectArgs,
                                                        const HardwareInfo &hwInfo) {

    using PIPELINE_SELECT = typename TGLLPFamily::PIPELINE_SELECT;

    if (HardwareCommandsHelper<TGLLPFamily>::isPipeControlPriorToPipelineSelectWArequired(hwInfo)) {
        auto pipeControl = PipeControlHelper<TGLLPFamily>::addPipeControl(*pCommandStream, false);
        pipeControl->setRenderTargetCacheFlushEnable(true);
    }

    auto pCmd = (PIPELINE_SELECT *)pCommandStream->getSpace(sizeof(PIPELINE_SELECT));
    *pCmd = TGLLPFamily::cmdInitPipelineSelect;

    auto mask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;

    pCmd->setMaskBits(mask);
    pCmd->setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
    pCmd->setMediaSamplerDopClockGateEnable(!pipelineSelectArgs.mediaSamplerRequired);

    Gen12LPHelpers::setAdditionalPipelineSelectFields(pCmd, pipelineSelectArgs, hwInfo);
}

template <>
void PreambleHelper<TGLLPFamily>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo) {
    auto pipeControl = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    *pipeControl = TGLLPFamily::cmdInitPipeControl;
    pipeControl->setCommandStreamerStallEnable(true);
    if (hwInfo->workaroundTable.waSendMIFLUSHBeforeVFE) {
        if (hwInfo->capabilityTable.defaultEngineType != aub_stream::ENGINE_CCS) {
            pipeControl->setRenderTargetCacheFlushEnable(true);
            pipeControl->setDepthCacheFlushEnable(true);
        }
        pipeControl->setDcFlushEnable(true);
    }
}

template <>
void PreambleHelper<TGLLPFamily>::programL3(LinearStream *pCommandStream, uint32_t l3Config) {
}

template <>
uint32_t PreambleHelper<TGLLPFamily>::getUrbEntryAllocationSize() {
    return 1024u;
}

template <>
uint64_t PreambleHelper<TGLLPFamily>::programVFEState(LinearStream *pCommandStream,
                                                      const HardwareInfo &hwInfo,
                                                      int scratchSize,
                                                      uint64_t scratchAddress,
                                                      uint32_t maxFrontEndThreads) {
    using MEDIA_VFE_STATE = typename TGLLPFamily::MEDIA_VFE_STATE;

    addPipeControlBeforeVfeCmd(pCommandStream, &hwInfo);

    auto scratchSpaceAddressOffset = static_cast<uint64_t>(pCommandStream->getUsed() + MEDIA_VFE_STATE::PATCH_CONSTANTS::SCRATCHSPACEBASEPOINTER_BYTEOFFSET);
    auto pMediaVfeState = reinterpret_cast<MEDIA_VFE_STATE *>(pCommandStream->getSpace(sizeof(MEDIA_VFE_STATE)));
    *pMediaVfeState = TGLLPFamily::cmdInitMediaVfeState;
    pMediaVfeState->setMaximumNumberOfThreads(maxFrontEndThreads);
    pMediaVfeState->setNumberOfUrbEntries(1);
    pMediaVfeState->setUrbEntryAllocationSize(PreambleHelper<TGLLPFamily>::getUrbEntryAllocationSize());
    pMediaVfeState->setPerThreadScratchSpace(Kernel::getScratchSizeValueToProgramMediaVfeState(scratchSize));
    pMediaVfeState->setStackSize(Kernel::getScratchSizeValueToProgramMediaVfeState(scratchSize));
    uint32_t lowAddress = static_cast<uint32_t>(0xFFFFFFFF & scratchAddress);
    uint32_t highAddress = static_cast<uint32_t>(0xFFFFFFFF & (scratchAddress >> 32));
    pMediaVfeState->setScratchSpaceBasePointer(lowAddress);
    pMediaVfeState->setScratchSpaceBasePointerHigh(highAddress);

    if (DebugManager.flags.CFEFusedEUDispatch.get() != -1) {
        pMediaVfeState->setDisableSlice0Subslice2(DebugManager.flags.CFEFusedEUDispatch.get());
    }
    return scratchSpaceAddressOffset;
}

// Explicitly instantiate PreambleHelper for TGLLP device family

template struct PreambleHelper<TGLLPFamily>;
} // namespace NEO
