/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/helpers/engine_node_helper.h"

namespace NEO {
namespace EngineDescriptorHelper {
constexpr EngineDescriptor getDefaultDescriptor() {
    return {{aub_stream::EngineType::ENGINE_RCS, EngineUsage::regular},
            1 /*deviceBitfield*/,
            PreemptionMode::Disabled,
            false /* isRootDevice*/};
}

constexpr EngineDescriptor getDefaultDescriptor(EngineTypeUsage engineTypeUsage) {
    return {engineTypeUsage,
            1 /*deviceBitfield*/,
            PreemptionMode::Disabled,
            false /* isRootDevice*/};
}

constexpr EngineDescriptor getDefaultDescriptor(EngineTypeUsage engineTypeUsage, bool isRootDevice) {
    return {engineTypeUsage,
            1 /*deviceBitfield*/,
            PreemptionMode::Disabled,
            isRootDevice};
}

constexpr EngineDescriptor getDefaultDescriptor(EngineTypeUsage engineTypeUsage, PreemptionMode preemptionMode) {
    return {engineTypeUsage,
            1 /*deviceBitfield*/,
            preemptionMode,
            false /* isRootDevice*/};
}

inline EngineDescriptor getDefaultDescriptor(DeviceBitfield deviceBitfield) {
    return {{aub_stream::EngineType::ENGINE_RCS, EngineUsage::regular},
            deviceBitfield,
            PreemptionMode::Disabled,
            false /* isRootDevice*/};
}

inline EngineDescriptor getDefaultDescriptor(EngineTypeUsage engineTypeUsage, DeviceBitfield deviceBitfield) {
    return {engineTypeUsage,
            deviceBitfield,
            PreemptionMode::Disabled,
            false /* isRootDevice*/};
}

inline EngineDescriptor getDefaultDescriptor(EngineTypeUsage engineTypeUsage, PreemptionMode preemptionMode, DeviceBitfield deviceBitfield) {
    return {engineTypeUsage,
            deviceBitfield,
            preemptionMode,
            false /* isRootDevice*/};
}
inline EngineDescriptor getDefaultDescriptor(EngineTypeUsage engineTypeUsage, PreemptionMode preemptionMode, DeviceBitfield deviceBitfield, bool isRootDevice) {
    return {engineTypeUsage,
            deviceBitfield,
            preemptionMode,
            isRootDevice};
}

} // namespace EngineDescriptorHelper
} // namespace NEO
