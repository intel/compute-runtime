/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"

namespace NEO {

template <typename GfxFamily>
const AuxTranslationMode UnitTestHelper<GfxFamily>::requiredAuxTranslationMode = AuxTranslationMode::none;

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isAdditionalMiSemaphoreWaitRequired(const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto programGlobalFenceAsMiMemFenceCommandInCommandStream = true;
    if (debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get() != -1) {
        programGlobalFenceAsMiMemFenceCommandInCommandStream = !!debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get();
    }
    return !programGlobalFenceAsMiMemFenceCommandInCommandStream;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::getSystolicFlagValueFromPipelineSelectCommand(const typename GfxFamily::PIPELINE_SELECT &pipelineSelectCmd) {
    return false;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::requiresTimestampPacketsInSystemMemory(HardwareInfo &hwInfo) {
    return !hwInfo.featureTable.flags.ftrLocalMemory;
}

template <typename GfxFamily>
GenCmdList::iterator UnitTestHelper<GfxFamily>::findCsrBaseAddressCommand(GenCmdList::iterator begin, GenCmdList::iterator end) {
    return find<typename GfxFamily::STATE_CONTEXT_DATA_BASE_ADDRESS *>(begin, end);
}

template <typename GfxFamily>
std::vector<GenCmdList::iterator> UnitTestHelper<GfxFamily>::findAllMidThreadPreemptionAllocationCommand(GenCmdList::iterator begin, GenCmdList::iterator end) {
    return findAll<typename GfxFamily::STATE_CONTEXT_DATA_BASE_ADDRESS *>(begin, end);
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::timestampRegisterHighAddress() {
    return true;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::getPipeControlHdcPipelineFlush(const typename GfxFamily::PIPE_CONTROL &pipeControl) {
    return pipeControl.getDataportFlush();
}

template <typename GfxFamily>
void UnitTestHelper<GfxFamily>::setPipeControlHdcPipelineFlush(typename GfxFamily::PIPE_CONTROL &pipeControl, bool hdcPipelineFlush) {
    pipeControl.setDataportFlush(hdcPipelineFlush);
}

} // namespace NEO
