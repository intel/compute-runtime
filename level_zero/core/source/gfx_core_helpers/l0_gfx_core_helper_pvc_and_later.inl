/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/definitions/engine_group_types.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

namespace L0 {
template <typename Family>
void L0GfxCoreHelperHw<Family>::setAdditionalGroupProperty(ze_command_queue_group_properties_t &groupProperty, NEO::EngineGroupT &group) const {
    if (group.engineGroupType == NEO::EngineGroupType::linkedCopy) {
        groupProperty.flags = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY;
        groupProperty.maxMemoryFillPatternSize = sizeof(uint8_t);
    }

    if (group.engineGroupType == NEO::EngineGroupType::copy) {
        groupProperty.flags = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY;

        bool virtualEnginesEnabled = true;
        for (const auto &engine : group.engines) {
            if (engine.osContext) {
                virtualEnginesEnabled &= NEO::EngineHelpers::isBcsVirtualEngineEnabled(engine.getEngineType());
            }
        }
        if (virtualEnginesEnabled) {
            groupProperty.maxMemoryFillPatternSize = sizeof(uint8_t);
        }
    }
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::isResumeWARequired() {
    return false;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::synchronizedDispatchSupported() const {
    return true;
}

} // namespace L0
