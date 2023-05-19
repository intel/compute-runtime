/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

void Wddm::populateAdditionalAdapterInfoOptions(const ADAPTER_INFO_KMD &adapterInfo) {
}
void Wddm::populateIpVersion(HardwareInfo &hwInfo) {
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    hwInfo.ipVersion.value = productHelper.getProductConfigFromHwInfo(hwInfo);
}
} // namespace NEO
