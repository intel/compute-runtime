/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/flat_batch_buffer_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/preamble_base.inl"

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
uint64_t PreambleHelper<GfxFamily>::programVFEState(LinearStream *pCommandStream,
                                                    const HardwareInfo &hwInfo,
                                                    uint32_t scratchSize,
                                                    uint64_t scratchAddress,
                                                    uint32_t maxFrontEndThreads,
                                                    aub_stream::EngineType engineType,
                                                    uint32_t additionalExecInfo) {
    using MEDIA_VFE_STATE = typename GfxFamily::MEDIA_VFE_STATE;

    addPipeControlBeforeVfeCmd(pCommandStream, &hwInfo, engineType);

    auto scratchSpaceAddressOffset = static_cast<uint64_t>(pCommandStream->getUsed() + MEDIA_VFE_STATE::PATCH_CONSTANTS::SCRATCHSPACEBASEPOINTER_BYTEOFFSET);
    auto pMediaVfeState = pCommandStream->getSpaceForCmd<MEDIA_VFE_STATE>();
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

    programAdditionalFieldsInVfeState(&cmd, hwInfo);
    *pMediaVfeState = cmd;

    return scratchSpaceAddressOffset;
}

template <typename GfxFamily>
size_t PreambleHelper<GfxFamily>::getVFECommandsSize() {
    using MEDIA_VFE_STATE = typename GfxFamily::MEDIA_VFE_STATE;
    return sizeof(MEDIA_VFE_STATE) + sizeof(PIPE_CONTROL);
}

} // namespace NEO
