/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hw_helper.h"
#include "core/helpers/preamble_base.inl"
#include "runtime/helpers/flat_batch_buffer_helper.h"

namespace NEO {

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programL3(LinearStream *pCommandStream, uint32_t l3Config) {
    auto pCmd = (MI_LOAD_REGISTER_IMM *)pCommandStream->getSpace(sizeof(MI_LOAD_REGISTER_IMM));
    *pCmd = GfxFamily::cmdInitLoadRegisterImm;

    pCmd->setRegisterOffset(L3CNTLRegisterOffset<GfxFamily>::registerOffset);
    pCmd->setDataDword(l3Config);
}

template <typename GfxFamily>
uint32_t PreambleHelper<GfxFamily>::getUrbEntryAllocationSize() {
    return 0x782;
}

template <typename GfxFamily>
uint64_t PreambleHelper<GfxFamily>::programVFEState(LinearStream *pCommandStream,
                                                    const HardwareInfo &hwInfo,
                                                    int scratchSize,
                                                    uint64_t scratchAddress,
                                                    uint32_t maxFrontEndThreads,
                                                    aub_stream::EngineType engineType) {
    using MEDIA_VFE_STATE = typename GfxFamily::MEDIA_VFE_STATE;

    addPipeControlBeforeVfeCmd(pCommandStream, &hwInfo, engineType);

    auto scratchSpaceAddressOffset = static_cast<uint64_t>(pCommandStream->getUsed() + MEDIA_VFE_STATE::PATCH_CONSTANTS::SCRATCHSPACEBASEPOINTER_BYTEOFFSET);
    auto pMediaVfeState = reinterpret_cast<MEDIA_VFE_STATE *>(pCommandStream->getSpace(sizeof(MEDIA_VFE_STATE)));
    *pMediaVfeState = GfxFamily::cmdInitMediaVfeState;
    pMediaVfeState->setMaximumNumberOfThreads(maxFrontEndThreads);
    pMediaVfeState->setNumberOfUrbEntries(1);
    pMediaVfeState->setUrbEntryAllocationSize(PreambleHelper<GfxFamily>::getUrbEntryAllocationSize());
    pMediaVfeState->setPerThreadScratchSpace(Kernel::getScratchSizeValueToProgramMediaVfeState(scratchSize));
    pMediaVfeState->setStackSize(Kernel::getScratchSizeValueToProgramMediaVfeState(scratchSize));
    uint32_t lowAddress = static_cast<uint32_t>(0xFFFFFFFF & scratchAddress);
    uint32_t highAddress = static_cast<uint32_t>(0xFFFFFFFF & (scratchAddress >> 32));
    pMediaVfeState->setScratchSpaceBasePointer(lowAddress);
    pMediaVfeState->setScratchSpaceBasePointerHigh(highAddress);

    programAdditionalFieldsInVfeState(pMediaVfeState, hwInfo);

    return scratchSpaceAddressOffset;
}

template <typename GfxFamily>
size_t PreambleHelper<GfxFamily>::getVFECommandsSize() {
    using MEDIA_VFE_STATE = typename GfxFamily::MEDIA_VFE_STATE;
    return sizeof(MEDIA_VFE_STATE) + sizeof(PIPE_CONTROL);
}

} // namespace NEO
