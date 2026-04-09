/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
const uint32_t PreemptionConfig<GfxFamily>::mmioAddress = 0x1A580;

template <>
const uint32_t PreemptionConfig<GfxFamily>::mask = ((1 << 1) | (1 << 2)) << 16;

template <>
const uint32_t PreemptionConfig<GfxFamily>::midThreadVal = 0;

template <>
void PreemptionHelper::programCmdStream<GfxFamily>(LinearStream &cmdStream, PreemptionMode newPreemptionMode,
                                                   PreemptionMode oldPreemptionMode, GraphicsAllocation *preemptionCsr) {
}

template <>
size_t PreemptionHelper::getRequiredCmdStreamSize<GfxFamily>(PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode) {
    return 0u;
}

template <>
void PreemptionHelper::programCmdStreamForLateStart<GfxFamily>(LinearStream &cmdStream) {
    uint32_t regAddress = PreemptionConfig<GfxFamily>::mmioAddress;
    uint32_t regValue = PreemptionConfig<GfxFamily>::midThreadVal | PreemptionConfig<GfxFamily>::mask;

    LriHelper<GfxFamily>::program(&cmdStream, regAddress, regValue, true, false);
}

template <>
size_t PreemptionHelper::getRequiredCmdStreamSizeForLateStart<GfxFamily>() {
    return sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM);
}
