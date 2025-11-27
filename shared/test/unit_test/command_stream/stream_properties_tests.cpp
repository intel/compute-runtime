/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/test/unit_test/command_stream/stream_properties_tests_common.h"

namespace NEO {

std::vector<StreamProperty *> getAllStateComputeModeProperties(StateComputeModeProperties &properties) {
    std::vector<StreamProperty *> allProperties;
    allProperties.push_back(&properties.isCoherencyRequired);
    allProperties.push_back(&properties.largeGrfMode);
    allProperties.push_back(&properties.zPassAsyncComputeThreadLimit);
    allProperties.push_back(&properties.pixelAsyncComputeThreadLimit);
    allProperties.push_back(&properties.threadArbitrationPolicy);
    allProperties.push_back(&properties.pipelinedEuThreadArbitration);
    allProperties.push_back(&properties.memoryAllocationForScratchAndMidthreadPreemptionBuffers);
    allProperties.push_back(&properties.enableVariableRegisterSizeAllocation);
    allProperties.push_back(&properties.lscSamplerBackingThreshold);
    allProperties.push_back(&properties.enableOutOfBoundariesInTranslationException);
    allProperties.push_back(&properties.enablePageFaultException);
    allProperties.push_back(&properties.enableSystemMemoryReadFence);
    allProperties.push_back(&properties.enableMemoryException);
    allProperties.push_back(&properties.enableBreakpoints);
    allProperties.push_back(&properties.enableForceExternalHaltAndForceException);
    return allProperties;
}

std::vector<StreamProperty *> getAllFrontEndProperties(FrontEndProperties &properties) {
    std::vector<StreamProperty *> allProperties;
    allProperties.push_back(&properties.disableEUFusion);
    allProperties.push_back(&properties.disableOverdispatch);
    allProperties.push_back(&properties.singleSliceDispatchCcsMode);
    allProperties.push_back(&properties.computeDispatchAllWalkerEnable);
    return allProperties;
}

std::vector<StreamProperty *> getAllPipelineSelectProperties(PipelineSelectProperties &properties) {
    std::vector<StreamProperty *> allProperties;
    allProperties.push_back(&properties.modeSelected);
    allProperties.push_back(&properties.systolicMode);
    return allProperties;
}

} // namespace NEO
