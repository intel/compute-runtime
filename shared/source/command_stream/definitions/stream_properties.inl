/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_property.h"

namespace NEO {

struct StateComputeModeProperties {
    StreamProperty isCoherencyRequired{};
    StreamProperty largeGrfMode{};
    StreamProperty zPassAsyncComputeThreadLimit{};
    StreamProperty pixelAsyncComputeThreadLimit{};
    StreamProperty threadArbitrationPolicy{};

    void setProperties(bool requiresCoherency, uint32_t numGrfRequired, uint32_t threadArbitrationPolicy);
    void setProperties(const StateComputeModeProperties &properties);
    bool isDirty() const;
    void clearIsDirty();
};

struct FrontEndProperties {
    StreamProperty disableOverdispatch{};
    StreamProperty singleSliceDispatchCcsMode{};
    StreamProperty computeDispatchAllWalkerEnable{};

    void setProperties(bool isCooperativeKernel, bool disableOverdispatch, int32_t engineInstancedDevice, const HardwareInfo &hwInfo);
    void setProperties(const FrontEndProperties &properties);
    bool isDirty() const;
    void clearIsDirty();
};

} // namespace NEO
