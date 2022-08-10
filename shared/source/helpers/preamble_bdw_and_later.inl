/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/flat_batch_buffer_helper.h"
#include "shared/source/helpers/preamble_base.inl"
#include "shared/source/kernel/kernel_execution_type.h"

namespace NEO {

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programL3(LinearStream *pCommandStream, uint32_t l3Config) {
    LriHelper<GfxFamily>::program(pCommandStream,
                                  L3CNTLRegisterOffset<GfxFamily>::registerOffset,
                                  l3Config,
                                  false);
}

template <typename GfxFamily>
uint32_t PreambleHelper<GfxFamily>::getUrbEntryAllocationSize() {
    return 0x782;
}

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
                                                const HardwareInfo &hwInfo,
                                                uint32_t scratchSize,
                                                uint64_t scratchAddress,
                                                uint32_t maxFrontEndThreads,
                                                const StreamProperties &streamProperties,
                                                LogicalStateHelper *logicalStateHelper) {
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

    appendProgramVFEState(hwInfo, streamProperties, &cmd);
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
void PreambleHelper<GfxFamily>::appendProgramPipelineSelect(void *cmd, bool isSpecialModeSelected, const HardwareInfo &hwInfo) {}

template <typename GfxFamily>
bool PreambleHelper<GfxFamily>::isSystolicModeConfigurable(const HardwareInfo &hwInfo) {
    return false;
}

template <typename GfxFamily>
bool PreambleHelper<GfxFamily>::isSpecialPipelineSelectModeChanged(bool lastSpecialPipelineSelectMode, bool newSpecialPipelineSelectMode,
                                                                   const HardwareInfo &hwInfo) {
    return false;
}
} // namespace NEO
