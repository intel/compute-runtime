/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_container/implicit_scaling_xehp_and_later.inl"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

using Family = XE_HPC_COREFamily;

template <>
bool ImplicitScalingDispatch<Family>::pipeControlStallRequired = false;

template <>
bool ImplicitScalingDispatch<Family>::platformSupportsImplicitScaling(const HardwareInfo &hwInfo) {
    if (ApiSpecificConfig::getApiType() == ApiSpecificConfig::ApiType::OCL) {
        return true;
    } else {
        return HwInfoConfig::get(hwInfo.platform.eProductFamily)->getSteppingFromHwRevId(hwInfo) >= REVISION_B;
    }
}

template struct ImplicitScalingDispatch<Family>;
} // namespace NEO
