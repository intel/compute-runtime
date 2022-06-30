/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_container/implicit_scaling_xehp_and_later.inl"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/xe_hp_core/hw_cmds_base.h"

namespace NEO {

using Family = XeHpFamily;

template <>
bool ImplicitScalingDispatch<Family>::pipeControlStallRequired = true;

template <>
bool ImplicitScalingDispatch<Family>::platformSupportsImplicitScaling(const HardwareInfo &hwInfo) {
    if (ApiSpecificConfig::getApiType() == ApiSpecificConfig::ApiType::OCL) {
        return true;
    } else {
        return false;
    }
}

template struct ImplicitScalingDispatch<Family>;
} // namespace NEO
