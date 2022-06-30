/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/definitions/engine_group_types.h"
#include "shared/source/helpers/engine_node_helper.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

#include <limits>

namespace L0 {

template <typename Family>
void L0HwHelperHw<Family>::setAdditionalGroupProperty(ze_command_queue_group_properties_t &groupProperty, NEO::EngineGroupType groupType) const {
    if (groupType == NEO::EngineGroupType::LinkedCopy) {
        groupProperty.flags = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY;
        groupProperty.maxMemoryFillPatternSize = sizeof(uint8_t);
    }

    if (groupType == NEO::EngineGroupType::Copy && NEO::EngineHelpers::isBcsVirtualEngineEnabled()) {
        groupProperty.flags = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY;
        groupProperty.maxMemoryFillPatternSize = sizeof(uint8_t);
    }
}

} // namespace L0
