/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

void Wddm::populateAdditionalAdapterInfoOptions(const ADAPTER_INFO_KMD &adapterInfo) {
}
void Wddm::populateIpVersion(HardwareInfo &hwInfo) {
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    hwInfo.ipVersion.value = compilerProductHelper.getHwIpVersion(hwInfo);
}
} // namespace NEO
