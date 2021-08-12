/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/engine_node_helper.h"

namespace NEO {
namespace EngineDescriptorHelper {
constexpr EngineDescriptor getDefaultDescriptor() {
    return {{aub_stream::EngineType::ENGINE_RCS, EngineUsage::Regular},
            1 /*deviceBitfield*/,
            PreemptionMode::Disabled,
            false /* isRootDevice*/,
            false /* isEngineInstanced*/};
}

constexpr EngineDescriptor getDefaultDescriptor(EngineTypeUsage engineTypeUsage) {
    return {engineTypeUsage,
            1 /*deviceBitfield*/,
            PreemptionMode::Disabled,
            false /* isRootDevice*/,
            false /* isEngineInstanced*/};
}

constexpr EngineDescriptor getDefaultDescriptor(EngineTypeUsage engineTypeUsage, bool isRootDevice) {
    return {engineTypeUsage,
            1 /*deviceBitfield*/,
            PreemptionMode::Disabled,
            isRootDevice,
            false /* isEngineInstanced*/};
}

constexpr EngineDescriptor getDefaultDescriptor(EngineTypeUsage engineTypeUsage, PreemptionMode preemptionMode) {
    return {engineTypeUsage,
            1 /*deviceBitfield*/,
            preemptionMode,
            false /* isRootDevice*/,
            false /* isEngineInstanced*/};
}

inline EngineDescriptor getDefaultDescriptor(DeviceBitfield deviceBitfield) {
    return {{aub_stream::EngineType::ENGINE_RCS, EngineUsage::Regular},
            deviceBitfield,
            PreemptionMode::Disabled,
            false /* isRootDevice*/,
            false /* isEngineInstanced*/};
}

inline EngineDescriptor getDefaultDescriptor(EngineTypeUsage engineTypeUsage, DeviceBitfield deviceBitfield) {
    return {engineTypeUsage,
            deviceBitfield,
            PreemptionMode::Disabled,
            false /* isRootDevice*/,
            false /* isEngineInstanced*/};
}

inline EngineDescriptor getDefaultDescriptor(EngineTypeUsage engineTypeUsage, PreemptionMode preemptionMode, DeviceBitfield deviceBitfield) {
    return {engineTypeUsage,
            deviceBitfield,
            preemptionMode,
            false /* isRootDevice*/,
            false /* isEngineInstanced*/};
}
inline EngineDescriptor getDefaultDescriptor(EngineTypeUsage engineTypeUsage, PreemptionMode preemptionMode, DeviceBitfield deviceBitfield, bool isRootDevice) {
    return {engineTypeUsage,
            deviceBitfield,
            preemptionMode,
            isRootDevice,
            false /* isEngineInstanced*/};
}

} // namespace EngineDescriptorHelper
} // namespace NEO
