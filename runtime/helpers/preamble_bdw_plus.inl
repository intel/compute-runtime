/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/preamble_base.inl"

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
void PreambleHelper<GfxFamily>::programVFEState(LinearStream *pCommandStream, const HardwareInfo &hwInfo, int scratchSize, uint64_t scratchAddress) {
    typedef typename GfxFamily::MEDIA_VFE_STATE MEDIA_VFE_STATE;

    addPipeControlBeforeVfeCmd(pCommandStream, &hwInfo);

    auto pMediaVfeState = (MEDIA_VFE_STATE *)pCommandStream->getSpace(sizeof(MEDIA_VFE_STATE));
    *pMediaVfeState = GfxFamily::cmdInitMediaVfeState;
    pMediaVfeState->setMaximumNumberOfThreads(PreambleHelper<GfxFamily>::getMaxThreadsForVfe(hwInfo));
    pMediaVfeState->setNumberOfUrbEntries(1);
    pMediaVfeState->setUrbEntryAllocationSize(PreambleHelper<GfxFamily>::getUrbEntryAllocationSize());
    pMediaVfeState->setPerThreadScratchSpace(Kernel::getScratchSizeValueToProgramMediaVfeState(scratchSize));
    pMediaVfeState->setStackSize(Kernel::getScratchSizeValueToProgramMediaVfeState(scratchSize));
    uint32_t lowAddress = uint32_t(0xFFFFFFFF & scratchAddress);
    uint32_t highAddress = uint32_t(0xFFFFFFFF & (scratchAddress >> 32));
    pMediaVfeState->setScratchSpaceBasePointer(lowAddress);
    pMediaVfeState->setScratchSpaceBasePointerHigh(highAddress);
}

} // namespace NEO
