/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/definitions/engine_group_types.h"
#include "shared/source/helpers/engine_node_helper.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

#include <limits>

namespace L0 {

template <typename Family>
void L0HwHelperHw<Family>::setAdditionalGroupProperty(ze_command_queue_group_properties_t &groupProperty, NEO::EngineGroupT &group) const {
    if (group.engineGroupType == NEO::EngineGroupType::LinkedCopy) {
        groupProperty.flags = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY;
        groupProperty.maxMemoryFillPatternSize = sizeof(uint8_t);
    }

    if (group.engineGroupType == NEO::EngineGroupType::Copy) {
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
bool L0HwHelperHw<Family>::platformSupportsCmdListHeapSharing(const NEO::HardwareInfo &hwInfo) const {
    return false;
}

template <typename Family>
bool L0HwHelperHw<Family>::platformSupportsStateComputeModeTracking(const NEO::HardwareInfo &hwInfo) const {
    return false;
}

template <typename Family>
bool L0HwHelperHw<Family>::platformSupportsFrontEndTracking(const NEO::HardwareInfo &hwInfo) const {
    return false;
}

template <typename Family>
bool L0HwHelperHw<Family>::platformSupportsPipelineSelectTracking(const NEO::HardwareInfo &hwInfo) const {
    return false;
}

} // namespace L0
