/*
 * Copyright (C) 2021-2022 Intel Corporation
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

    void setProperties(bool requiresCoherency, uint32_t numGrfRequired, int32_t threadArbitrationPolicy, const HardwareInfo &hwInfo);
    void setProperties(const StateComputeModeProperties &properties);
    bool isDirty() const;

  protected:
    void clearIsDirty();

    bool isDirtyExtra() const;
    void setPropertiesExtra();
    void setPropertiesExtra(const StateComputeModeProperties &properties);
    void clearIsDirtyExtra();
};

struct FrontEndProperties {
    StreamProperty computeDispatchAllWalkerEnable{};
    StreamProperty disableEUFusion{};
    StreamProperty disableOverdispatch{};
    StreamProperty singleSliceDispatchCcsMode{};

    void setProperties(bool isCooperativeKernel, bool disableEUFusion, bool disableOverdispatch, int32_t engineInstancedDevice,
                       const HardwareInfo &hwInfo);
    void setProperties(const FrontEndProperties &properties);
    bool isDirty() const;

  protected:
    void clearIsDirty();
};

} // namespace NEO
