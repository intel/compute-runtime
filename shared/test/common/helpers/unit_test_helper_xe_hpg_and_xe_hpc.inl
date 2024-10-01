/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/unit_test_helper.h"

namespace NEO {

template <typename GfxFamily>
const AuxTranslationMode UnitTestHelper<GfxFamily>::requiredAuxTranslationMode = AuxTranslationMode::blit;

template <typename GfxFamily>
GenCmdList::iterator UnitTestHelper<GfxFamily>::findCsrBaseAddressCommand(GenCmdList::iterator begin, GenCmdList::iterator end) {
    return end;
}

template <typename GfxFamily>
std::vector<GenCmdList::iterator> UnitTestHelper<GfxFamily>::findAllMidThreadPreemptionAllocationCommand(GenCmdList::iterator begin, GenCmdList::iterator end) {
    std::vector<GenCmdList::iterator> emptyList;
    return emptyList;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::timestampRegisterHighAddress() {
    return false;
}

template <typename GfxFamily>
inline void UnitTestHelper<GfxFamily>::setPipeControlHdcPipelineFlush(typename GfxFamily::PIPE_CONTROL &pipeControl, bool hdcPipelineFlush) {
    pipeControl.setHdcPipelineFlush(hdcPipelineFlush);
}

template <typename GfxFamily>
inline bool UnitTestHelper<GfxFamily>::getPipeControlHdcPipelineFlush(const typename GfxFamily::PIPE_CONTROL &pipeControl) {
    return pipeControl.getHdcPipelineFlush();
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::getSystolicFlagValueFromPipelineSelectCommand(const typename GfxFamily::PIPELINE_SELECT &pipelineSelectCmd) {
    return pipelineSelectCmd.getSystolicModeEnable();
}

} // namespace NEO
