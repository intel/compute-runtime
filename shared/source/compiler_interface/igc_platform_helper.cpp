/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/igc_platform_helper.h"

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"

#include "ocl_igc_interface/gt_system_info.h"
#include "ocl_igc_interface/igc_features_and_workarounds.h"
#include "ocl_igc_interface/platform_helper.h"

namespace NEO {

template <>
void populateIgcPlatform<>(IGC::Platform<1> &igcPlatform, const HardwareInfo &hwInfo) {
    IGC::PlatformHelper::PopulateInterfaceWith(igcPlatform, hwInfo.platform);
}

template <>
void populateIgcPlatform<>(IGC::Platform<2> &igcPlatform, const HardwareInfo &inputHwInfo) {
    auto hwInfo = inputHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    if (compilerProductHelper) {
        compilerProductHelper->adjustHwInfoForIgc(hwInfo);
    }
    igcPlatform.SetProductFamily(hwInfo.platform.eProductFamily);
    igcPlatform.SetPCHProductFamily(hwInfo.platform.ePCHProductFamily);
    igcPlatform.SetDisplayCoreFamily(hwInfo.platform.eDisplayCoreFamily);
    igcPlatform.SetRenderCoreFamily(hwInfo.platform.eRenderCoreFamily);
    igcPlatform.SetPlatformType(hwInfo.platform.ePlatformType);
    igcPlatform.SetDeviceID(hwInfo.platform.usDeviceID);
    igcPlatform.SetRevId(hwInfo.platform.usRevId);
    igcPlatform.SetDeviceID_PCH(hwInfo.platform.usDeviceID_PCH);
    igcPlatform.SetRevId_PCH(hwInfo.platform.usRevId_PCH);
    igcPlatform.SetGTType(hwInfo.platform.eGTType);
    igcPlatform.SetRenderBlockID(hwInfo.ipVersion.value);
}

} // namespace NEO
