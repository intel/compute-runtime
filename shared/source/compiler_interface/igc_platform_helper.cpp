/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/igc_platform_helper.h"

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/validators.h"

#include "ocl_igc_interface/platform_helper.h"

namespace NEO {

void populateIgcPlatform(PlatformTag &igcPlatform, const HardwareInfo &inputHwInfo) {
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

bool initializeIgcDeviceContext(NEO::IgcOclDeviceCtxTag *igcDeviceCtx, const HardwareInfo &hwInfo, const CompilerProductHelper *compilerProductHelper) {
    auto igcPlatform = igcDeviceCtx->GetPlatformHandle<NEO::PlatformTag>();
    auto igcGtSystemInfo = igcDeviceCtx->GetGTSystemInfoHandle<NEO::GTSystemInfoTag>();
    auto igcFtrWa = igcDeviceCtx->GetIgcFeaturesAndWorkaroundsHandle<NEO::IgcFeaturesAndWorkaroundsTag>();

    if (false == NEO::areNotNullptr(igcPlatform.get(), igcGtSystemInfo.get(), igcFtrWa.get())) {
        return false;
    }
    populateIgcPlatform(*igcPlatform, hwInfo);

    IGC::GtSysInfoHelper::PopulateInterfaceWith(*igcGtSystemInfo, hwInfo.gtSystemInfo);

    if (compilerProductHelper) {
        igcFtrWa->SetFtrGpGpuMidThreadLevelPreempt(compilerProductHelper->isMidThreadPreemptionSupported(hwInfo));
    }
    igcFtrWa->SetFtrWddm2Svm(hwInfo.featureTable.flags.ftrWddm2Svm);
    igcFtrWa->SetFtrPooledEuEnabled(hwInfo.featureTable.flags.ftrPooledEuEnabled);

    return true;
}

} // namespace NEO
