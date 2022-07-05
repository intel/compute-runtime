/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

namespace L0 {

template <typename GfxFamily>
void L0HwHelperHw<GfxFamily>::setAdditionalGroupProperty(ze_command_queue_group_properties_t &groupProperty, NEO::EngineGroupT &group) const {
}

template <typename Family>
bool L0HwHelperHw<Family>::multiTileCapablePlatform() const {
    return false;
}

} // namespace L0
