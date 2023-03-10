/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_container/implicit_scaling_xehp_and_later.inl"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

namespace NEO {

using Family = XeHpcCoreFamily;

template <>
bool ImplicitScalingDispatch<Family>::pipeControlStallRequired = false;

template <>
bool ImplicitScalingDispatch<Family>::platformSupportsImplicitScaling(const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (ApiSpecificConfig::getApiType() == ApiSpecificConfig::ApiType::OCL) {
        return true;
    } else {
        auto &productHelper = rootDeviceEnvironment.template getHelper<ProductHelper>();
        auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
        return productHelper.isImplicitScalingSupported(hwInfo);
    }
}

template struct ImplicitScalingDispatch<Family>;
} // namespace NEO
